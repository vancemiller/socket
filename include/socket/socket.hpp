#ifndef WRAPPER_SOCKET_SOCKET_HPP
#define WRAPPER_SOCKET_SOCKET_HPP

#include "address.hpp"
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
    Base(FileDescriptor&& sockfd);
  public:
    Base(void);
    Base(Base&) = delete;
    Base(const Base&) = delete;
    Base(Base&& o);
    virtual ~Base(void);
    bool data_available(void) const;
  protected:
    int fd(void) const;
};

class Listening;

class Connected final : public Base {
  friend class Listening;
  public:
    Connected(FileDescriptor&& sockfd);
    // this constructor shouldn't be public but ConnectedNeeds needs a special allocator or
    // a public constructor for emplace to work
  public:
    Connected(const Address& address);
    Connected(Connected&& o);
    ~Connected(void);
    std::string get_ip(void) const;
    bool read(void* buf, size_t count, int timeout_ms=-1);
    void write(const void* buf, size_t count);
};

class Listening final : public Base {
  private:
    FileDescriptor listen_epfd;
    FileDescriptor connections_epfd;
    std::list<std::shared_ptr<Connected>> _connections;
    Mutex mutex;
  public:
    Listening(short port);
    Listening(Listening&& o);
    ~Listening(void);
    std::shared_ptr<Connected> accept(int timeout_ms=-1);
    void broadcast(const void* buf, size_t count);
    size_t connections(void) const noexcept;
    bool remove_disconnected(int timeout_ms=-1);
};

std::string get_my_ip(void);

} // namespace socket
} // namespace wrapper
#endif
