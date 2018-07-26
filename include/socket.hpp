#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <cerrno>
#include <cstring>
#include <system_error>
#include <string>
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
};

class ReadOnlySocket final : private SocketBase {
  public:
    ReadOnlySocket(const char* address, short port) {
      sockaddr_in addr;
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      if (!inet_aton(address, &addr.sin_addr))
        throw std::runtime_error("invalid address");
      if (connect(sockfd, (sockaddr*)&addr, sizeof(sockaddr_in)) == -1)
        throw std::system_error(errno, std::generic_category(), "socket connect failed");
    }
    ~ReadOnlySocket(void) {
      if (shutdown(sockfd, SHUT_RDWR) == -1)
        std::cerr << "WARNING: socket shutdown failed: " << std::strerror(errno) << std::endl;
    }
    void read(void* buf, size_t count) {
      if (::read(this->sockfd, buf, count) == -1)
        throw std::system_error(errno, std::generic_category(), "socket read failed");
    }
};

class ReadWriteSocket final : private SocketBase {
  public:
    ReadWriteSocket(int sockfd) : SocketBase(sockfd) {}

    void write(const void* buf, size_t count) {
      if (::write(this->sockfd, buf, count) == -1)
        throw std::system_error(errno, std::generic_category(), "socket write failed");
    }
    void read(void* buf, size_t count) {
      if (::read(this->sockfd, buf, count) == -1)
        throw std::system_error(errno, std::generic_category(), "socket read failed");
    }
};

class Socket final : private SocketBase {
  private:
    std::vector<ReadWriteSocket> connections;
  public:
    Socket(short port) {
      sockaddr_in addr;
      addr.sin_family = AF_INET;
      addr.sin_port = htons(port);
      addr.sin_addr.s_addr = INADDR_ANY; // Automatically select an IP address
      if (bind(sockfd, (sockaddr*)&addr, sizeof(sockaddr_in)) == -1)
        throw std::system_error(errno, std::generic_category(), "socket bind failed");
      if (listen(this->sockfd, BACKLOG) == -1)
        throw std::system_error(errno, std::generic_category(), "socket listen failed");
    }
    ~Socket(void) {
      if (shutdown(sockfd, SHUT_RDWR) == -1)
        std::cerr << "WARNING: socket shutdown failed: " << std::strerror(errno) << std::endl;
    }
    ReadWriteSocket& accept(void) {
      sockaddr_in connection;
      int connection_size = sizeof(sockaddr_in);
      int confd = ::accept(sockfd, (sockaddr*) &connection, (socklen_t*) &connection_size);
      if (connection_size != sizeof(sockaddr_in))
        throw std::runtime_error("Connection is incorrect type. It is not sockaddr_in.");
      if (confd == -1)
        throw std::system_error(errno, std::generic_category(), "socket accept failed");
      connections.emplace_back(confd);
      return connections.back();
    }
};

#endif

