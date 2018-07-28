#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <cassert>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <list>
#include <system_error>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BACKLOG 16

class Serializable { };

class Deserializable { };

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

class RWSocket : protected SocketBase {
  protected:
    RWSocket(void) {}
  public:
    RWSocket(int sockfd) : SocketBase(sockfd) {}
    // this constructor shouldn't be public but it needs a special allocator or
    // must be public for emplace to work
  public:
    void write(const void* buf, size_t count) {
      if (::write(this->sockfd, buf, count) == -1)
        throw std::system_error(errno, std::generic_category(), "socket write failed");
    }
    void read(void* buf, size_t count) {
      if (::read(this->sockfd, buf, count) == -1)
        throw std::system_error(errno, std::generic_category(), "socket read failed");
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
      int err = connect(sockfd, (sockaddr*)&addr, sizeof(sockaddr_in));
      /*if (err == -1 && errno == EINPROGRESS) {
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(sockfd, &writefds);
        if (select(1, NULL, &writefds, NULL,  NULL) == -1)
          throw std::system_error(errno, std::generic_category(), "socket select failed");
        socklen_t len = sizeof(int);
        if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &len) == -1)
          throw std::system_error(errno, std::generic_category(), "socket getsockopt failed");
        if (len != sizeof(int))
          throw std::runtime_error("socket getsockopt failed");
        // now err is properly set
      }*/
      if (err == -1)
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
    int epfd;
    epoll_event ev;
    std::list<std::shared_ptr<RWSocket>> connections;
  public:
    ListeningSocket(short port) : epfd(epoll_create(1)) {
      if (epfd == -1)
        throw std::system_error(errno, std::generic_category(), "epoll create failed");
      sockaddr_in addr;
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      addr.sin_addr.s_addr = INADDR_ANY; // Automatically select an IP address
      if (bind(sockfd, (sockaddr*)&addr, sizeof(sockaddr_in)) == -1)
        throw std::system_error(errno, std::generic_category(), "socket bind failed");
      if (listen(this->sockfd, BACKLOG) == -1)
        throw std::system_error(errno, std::generic_category(), "socket listen failed");
      this->ev.events = EPOLLIN;
      if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev) == -1)
        throw std::system_error(errno, std::generic_category(), "epoll_ctl failed");
    }
    ~ListeningSocket(void) {
      if (shutdown(sockfd, SHUT_RDWR) == -1)
        std::cerr << "WARNING: socket shutdown failed: " << std::strerror(errno) << std::endl;
      close(epfd);
    }
    std::shared_ptr<RWSocket> accept(int timeout_ms) {
      epoll_event ev;
      int ret = epoll_wait(epfd, &ev, 1, timeout_ms);
      if (ret == -1)
        throw std::system_error(errno, std::generic_category(), "epoll wait failed");
      if (ret == 0) // timeout
        return nullptr;
      assert(ret == 1);
      sockaddr_in connection;
      int connection_size = sizeof(sockaddr_in);
      int confd = ::accept(sockfd, (sockaddr*) &connection, (socklen_t*) &connection_size);
      if (connection_size != sizeof(sockaddr_in))
        throw std::runtime_error("Connection is incorrect type. It is not sockaddr_in.");
      if (confd == -1)
          throw std::system_error(errno, std::generic_category(), "socket accept failed");
      connections.emplace_back(std::make_shared<RWSocket>(confd));
      return connections.back();
    }
    void broadcast(const void* buf, size_t count) {
      for (std::shared_ptr<RWSocket>& connection : connections) {
        connection->write(buf, count);
      }
    }
};

#endif

