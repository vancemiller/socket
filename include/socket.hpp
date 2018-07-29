#ifndef SOCKET_HPP
#define SOCKET_HPP

#include "lock.hpp"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <list>
#include <mutex>
#include <system_error>
#include <string>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BACKLOG 16

class SocketBase {
  protected:
    const int sockfd;
    SocketBase(int sockfd): sockfd(sockfd) {}
  public:
    SocketBase(void) : SocketBase(socket(AF_INET, SOCK_STREAM, 0)) {
      if (sockfd == -1)
        throw std::system_error(errno, std::generic_category(), "socket creation failed");
      bool value = true;
      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) == -1)
        throw std::system_error(errno, std::generic_category(), "socket setsockopt failed");
    };
    virtual ~SocketBase(void) {
      if (close(sockfd) == -1)
        std::cerr << "WARNING: socket close failed: " << std::strerror(errno) << std::endl;
    }
    SocketBase(const SocketBase&) = delete; // don't copy because it will close the file descriptor
};

class ListeningSocket;

class RWSocket : protected SocketBase {
  friend class ListeningSocket;
  protected:
    RWSocket(void) {}
  public:
    RWSocket(int sockfd) : SocketBase(sockfd) {}
    // this constructor shouldn't be public but it needs a special allocator or
    // must be public for emplace to work
  public:
    void write(const void* buf, size_t count) {
      int ret = ::send(this->sockfd, buf, count, MSG_NOSIGNAL);
      if (ret == -1)
        throw std::system_error(errno, std::generic_category(), "socket write failed");
      if (ret != (int) count)
        throw std::runtime_error("write did not write all the bytes");
    }
    void read(void* buf, size_t count) {
      int ret = ::read(this->sockfd, buf, count);
      if (ret == -1)
        throw std::system_error(errno, std::generic_category(), "socket read failed");
      if (ret != (int) count)
        throw std::runtime_error("read did not read all the bytes");
    }
};

class ConnectedSocket final : private RWSocket {
  public:
    ConnectedSocket(const char* address, short port) {
      sockaddr_in addr;
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      if (!inet_aton(address, &addr.sin_addr))
        throw std::runtime_error("invalid address");
      if (connect(sockfd, (sockaddr*)&addr, sizeof(sockaddr_in)) == -1)
        throw std::system_error(errno, std::generic_category(), "socket connect failed");
    }
    ~ConnectedSocket(void) {
      if (shutdown(sockfd, SHUT_RDWR) == -1)
        std::cerr << "WARNING: socket shutdown failed: " << std::strerror(errno) << std::endl;
    }
    void read(void* buf, size_t count) {
      if (::read(this->sockfd, buf, count) == -1)
        throw std::system_error(errno, std::generic_category(), "socket read failed");
    }
};

class ListeningSocket final : private SocketBase {
  private:
    int listen_epfd;
    int connections_epfd;
    std::list<std::shared_ptr<RWSocket>> _connections;
    Mutex mutex;
  public:
    ListeningSocket(short port) : listen_epfd(epoll_create(1)), connections_epfd(epoll_create(1)) {
      if (listen_epfd == -1 || connections_epfd == -1)
        throw std::system_error(errno, std::generic_category(), "epoll create failed");
      sockaddr_in addr;
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      addr.sin_addr.s_addr = INADDR_ANY; // Automatically select an IP address
      if (bind(sockfd, (sockaddr*)&addr, sizeof(sockaddr_in)) == -1)
        throw std::system_error(errno, std::generic_category(), "socket bind failed");
      if (listen(this->sockfd, BACKLOG) == -1)
        throw std::system_error(errno, std::generic_category(), "socket listen failed");
      epoll_event ev;
      ev.events = EPOLLIN;
      ev.data.fd = sockfd;
      if (epoll_ctl(listen_epfd, EPOLL_CTL_ADD, sockfd, &ev) == -1)
        throw std::system_error(errno, std::generic_category(), "epoll_ctl failed");
    }
    ~ListeningSocket(void) {
      if (shutdown(sockfd, SHUT_RDWR) == -1)
        std::cerr << "WARNING: socket shutdown failed: " << std::strerror(errno) << std::endl;
      close(connections_epfd);
      close(listen_epfd);
    }
    std::shared_ptr<RWSocket> accept(int timeout_ms) {
      epoll_event ev;
      int ret = epoll_wait(listen_epfd, &ev, 1, timeout_ms);
      if ((ret == -1 && errno == EINTR) || ret == 0) // interrupted or timeout
        return nullptr;
      if (ret == -1)
        throw std::system_error(errno, std::generic_category(), "epoll wait failed");
      assert(ret == 1);
      assert(ev.events & EPOLLIN);
      sockaddr_in connection;
      int connection_size = sizeof(sockaddr_in);
      int confd = ::accept(sockfd, (sockaddr*) &connection, (socklen_t*) &connection_size);
      if (connection_size != sizeof(sockaddr_in))
        throw std::runtime_error("Connection is incorrect type. It is not sockaddr_in.");
      if (confd == -1)
        throw std::system_error(errno, std::generic_category(), "socket accept failed");
      assert(ev.data.fd == sockfd);
      epoll_event con_ev;
      con_ev.events = EPOLLRDHUP;
      con_ev.data.fd = confd;
      if (epoll_ctl(connections_epfd, EPOLL_CTL_ADD, confd, &con_ev) == -1)
        throw std::system_error(errno, std::generic_category(), "epoll_ctl failed");
      std::lock_guard<Mutex> lock(mutex);
      _connections.emplace_back(std::make_shared<RWSocket>(confd));
      return _connections.back();
    }

    void broadcast(const void* buf, size_t count) {
      std::lock_guard<Mutex> lock(mutex);
      for (std::list<std::shared_ptr<RWSocket>>::iterator c = _connections.begin();
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
      int ret = epoll_wait(connections_epfd, ev, N_EVENTS, timeout_ms);
      if ((ret == -1 && errno == EINTR) || ret == 0) // interrupted or timeout
        return false;
      if (ret == -1)
        throw std::system_error(errno, std::generic_category(), "epoll wait failed");
      // remove connection with ev.data.fd from _connections

      for (int i = 0; i < ret; i++) {
        assert(ev[i].events & EPOLLRDHUP);
        if (epoll_ctl(connections_epfd, EPOLL_CTL_DEL, ev[i].data.fd, NULL) == -1)
          throw std::system_error(errno, std::generic_category(), "epoll_ctl failed");
      }
      std::lock_guard<Mutex> lock(mutex);
      _connections.remove_if([&ev, &ret] (const std::shared_ptr<RWSocket>& c) -> bool {
          for (int i = 0; i < ret; i++)
            if (c->sockfd == ev[i].data.fd)
              return true;
        return false;
      });
      return true;
    }
};

#endif

