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
  protected:
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
};

class Listening;

class Connected : public Base {
  private:
    const Address input_address;
  protected:
    Connected(FileDescriptor&& sockfd);
  public:
    Connected(const Address& to);
    Connected(Connected&& o);
    ~Connected(void);
    std::string get_ip(void) const;
    bool read(void* buf, size_t count, int timeout_ms=-1);
    Address get_input_address(void) const noexcept;
};

class Bidirectional final : public Connected {
  friend class Listening;
  private:
    const Address address;
  public:
    Bidirectional(Listening& listener);
    // This constructor shouldn't be public but is necessary for Listening to call make_unique.
  public:
    Bidirectional(const Address& to);
    Bidirectional(Bidirectional&& o);
    void write(const void* buf, size_t count);
    Address get_address(void) const noexcept;
};

class Listening final : public Base {
  friend class Bidirectional;
  private:
    Address address;
    FileDescriptor listen_epfd;
    FileDescriptor connections_epfd;
    std::list<std::shared_ptr<Bidirectional>> _connections;
    Mutex mutex;
  public:
    Listening(short port);
    Listening(Listening&& o);
    ~Listening(void);
    std::shared_ptr<Bidirectional> accept(int timeout_ms=-1);
    void broadcast(const void* buf, size_t count);
    size_t connections(void) const noexcept;
    bool remove_disconnected(int timeout_ms=-1);
    Address get_address(void) const noexcept;
};

std::string get_my_ip(void);

} // namespace socket
} // namespace wrapper
#endif
