#ifndef SOCKET_HPP
#define SOCKET_HPP

#include "lock.hpp"
#include "file_descriptor.hpp"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <poll.h>
#include <system_error>
#include <string>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace wrapper {
namespace socket {

#define BACKLOG 16

class Base {
  private:
    FileDescriptor sockfd;
  protected:
    Base(FileDescriptor&& sockfd): sockfd(std::move(sockfd)) {}
  public:
    Base(void) : Base(::socket(AF_INET, SOCK_STREAM, 0)) {
      if (sockfd == -1)
        throw std::system_error(errno, std::generic_category(), "socket creation failed");
      bool value = true;
      if (setsockopt(sockfd.get(), SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) == -1)
        throw std::system_error(errno, std::generic_category(), "socket setsockopt failed");
    }
    virtual ~Base(void) {}
    Base(Base&) = delete;
    Base(const Base&) = delete;
    Base(Base&& o) : sockfd(std::move(o.sockfd)) {}

    bool data_available(void) const {
      pollfd fds{sockfd.get(), POLLIN, 0};
      if (poll(&fds, 1, 0) == -1)
        throw std::system_error(errno, std::generic_category(), "poll failed");
      return fds.revents & POLLIN;
    }

    std::string get_ip(void) const {
      sockaddr_in name;
      socklen_t name_size = sizeof(name);
      if (getsockname(fd(), (sockaddr*) &name, &name_size) == -1)
        throw std::system_error(errno, std::generic_category(), "getsockname failed");
      char* dst = (char*) malloc(sizeof(char) * INET_ADDRSTRLEN);
      if (inet_ntop(AF_INET, &name.sin_addr, dst, INET_ADDRSTRLEN) != dst)
        throw std::system_error(errno, std::generic_category(), "inet_ntop failed");
      return std::string(dst);
    }

  protected:
    int fd(void) const { return sockfd.get(); }
};

class Listening;

class Connected final : public Base {
  friend class Listening;
  public:
    Connected(FileDescriptor&& sockfd) : Base(std::move(sockfd)) {}
    // this constructor shouldn't be public but ConnectedNeeds needs a special allocator or
    // a public constructor for emplace to work
  public:
    Connected(const char* address, short port) {
      sockaddr_in addr;
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      if (!inet_aton(address, &addr.sin_addr))
        throw std::runtime_error("invalid address");
      if (connect(fd(), (sockaddr*)&addr, sizeof(sockaddr_in)) == -1)
        throw std::system_error(errno, std::generic_category(), "socket connect failed");
    }

    ~Connected(void) {
      if (fd() != -1 && shutdown(fd(), SHUT_RDWR) == -1)
        if (errno != ENOTCONN) // ok to shut down a disconnected socket
          std::cerr << "WARNING: socket shutdown failed: " << std::strerror(errno) << std::endl;
    }

    Connected(Connected&& o) : Base(std::move(o)) {}

    void read(void* buf, size_t count) {
      size_t received = 0;
      do {
        int ret = ::read(fd(), &((char*) buf)[received], count - received);
        if (ret == 0 || ret == -1)
          throw std::system_error(errno, std::generic_category(), "socket read failed");
        received += ret;
      } while(received < count);
    }

    void write(const void* buf, size_t count) {
      size_t sent = 0;
      do {
        int ret = ::send(fd(), &((char*) buf)[sent], count - sent, MSG_NOSIGNAL);
        if (ret == -1)
          throw std::system_error(errno, std::generic_category(), "socket write failed");
        sent += ret;
      } while (sent < count);
    }
};

class Listening final : public Base {
  private:
    FileDescriptor listen_epfd;
    FileDescriptor connections_epfd;
    std::list<std::shared_ptr<Connected>> _connections;
    Mutex mutex;
  public:
    Listening(short port) : listen_epfd(epoll_create(1)), connections_epfd(epoll_create(1)) {
      if (listen_epfd == -1 || connections_epfd == -1)
        throw std::system_error(errno, std::generic_category(), "epoll create failed");
      sockaddr_in addr;
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      addr.sin_addr.s_addr = INADDR_ANY; // Automatically select an IP address
      if (bind(fd(), (sockaddr*)&addr, sizeof(sockaddr_in)) == -1)
        throw std::system_error(errno, std::generic_category(), "socket bind failed");
      if (listen(fd(), BACKLOG) == -1)
        throw std::system_error(errno, std::generic_category(), "socket listen failed");
      epoll_event ev;
      ev.events = EPOLLIN;
      ev.data.fd = fd();
      if (epoll_ctl(listen_epfd.get(), EPOLL_CTL_ADD, fd(), &ev) == -1)
        throw std::system_error(errno, std::generic_category(), "epoll_ctl failed");
    }

    ~Listening(void) {
      if (fd() != -1)
        if (shutdown(fd(), SHUT_RDWR) == -1)
          std::cerr << "WARNING: socket shutdown failed: " << std::strerror(errno) << std::endl;
    }

    Listening(Listening&& o) : Base(std::move(o)), listen_epfd(std::move(o.listen_epfd)),
        connections_epfd(std::move(o.connections_epfd)), _connections(std::move(o._connections)),
        mutex(std::move(o.mutex)) {}

    std::shared_ptr<Connected> accept(int timeout_ms) {
      epoll_event ev;
      int ret = epoll_wait(listen_epfd.get(), &ev, 1, timeout_ms);
      if ((ret == -1 && errno == EINTR) || ret == 0) // interrupted or timeout
        return nullptr;
      if (ret == -1)
        throw std::system_error(errno, std::generic_category(), "epoll wait failed");
      assert(ret == 1);
      assert(ev.events & EPOLLIN);
      sockaddr_in connection;
      socklen_t connection_size = sizeof(sockaddr_in);
      int confd = ::accept(fd(), (sockaddr*) &connection, &connection_size);
      if (connection_size != sizeof(sockaddr_in))
        throw std::runtime_error("Connection is incorrect type. It is not sockaddr_in.");
      if (confd == -1)
        throw std::system_error(errno, std::generic_category(), "socket accept failed");
      assert(ev.data.fd == fd());
      epoll_event con_ev;
      con_ev.events = EPOLLRDHUP;
      con_ev.data.fd = confd;
      if (epoll_ctl(connections_epfd.get(), EPOLL_CTL_ADD, confd, &con_ev) == -1)
        throw std::system_error(errno, std::generic_category(), "epoll_ctl failed");
      std::lock_guard<Mutex> lock(mutex);
      _connections.emplace_back(std::make_shared<Connected>(confd));
      return _connections.back();
    }

    void broadcast(const void* buf, size_t count) {
      std::lock_guard<Mutex> lock(mutex);
      for (std::list<std::shared_ptr<Connected>>::iterator c = _connections.begin();
          c != _connections.end(); ) {
        try {
          (*c)->write(buf, count);
          c++;
        } catch (const std::system_error& e) {
          c = _connections.erase(c);
        }
      }
    }

    size_t connections(void) const noexcept { return _connections.size(); }

    bool remove_disconnected(int timeout_ms) {
      const int N_EVENTS = 16;
      epoll_event ev[N_EVENTS];
      int ret = epoll_wait(connections_epfd.get(), ev, N_EVENTS, timeout_ms);
      if ((ret == -1 && errno == EINTR) || ret == 0) // interrupted or timeout
        return false;
      if (ret == -1)
        throw std::system_error(errno, std::generic_category(), "epoll wait failed");
      // remove connection with ev.data.fd from _connections

      for (int i = 0; i < ret; i++) {
        assert(ev[i].events & EPOLLRDHUP);
        if (epoll_ctl(connections_epfd.get(), EPOLL_CTL_DEL, ev[i].data.fd, NULL) == -1)
          throw std::system_error(errno, std::generic_category(), "epoll_ctl failed");
      }
      std::lock_guard<Mutex> lock(mutex);
      _connections.remove_if([&ev, &ret] (const std::shared_ptr<Connected>& c) -> bool {
          for (int i = 0; i < ret; i++)
            if (c->fd() == ev[i].data.fd)
              return true;
        return false;
      });
      return true;
    }
};

#define DEFAULT_CONNECT_IP "8.8.8.8"
#define DEFAULT_CONNECT_PORT 53
inline std::string get_my_ip(void) {
  return Connected(DEFAULT_CONNECT_IP, DEFAULT_CONNECT_PORT).get_ip();
}
} // namespace socket
} // namespace wrapper
#endif
