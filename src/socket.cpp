#include "socket/socket.hpp"

namespace wrapper {
namespace socket {

Base::Base(FileDescriptor&& sockfd) : sockfd(std::move(sockfd)) {}

Base::Base(void) : Base(::socket(AF_INET, SOCK_STREAM, 0)) {
  if (sockfd == -1)
    throw std::system_error(errno, std::generic_category(), "socket creation failed");
  bool value = true;
  if (setsockopt(sockfd.get(), SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) == -1)
    throw std::system_error(errno, std::generic_category(), "socket setsockopt failed");
}

Base::Base(Base&& o) : sockfd(std::move(o.sockfd)) {}

Base::~Base(void) {}

bool Base::data_available(void) const {
  pollfd fds{sockfd.get(), POLLIN, 0};
  if (poll(&fds, 1, 0) == -1)
    throw std::system_error(errno, std::generic_category(), "poll failed");
  return fds.revents & POLLIN;
}

Connected::Connected(FileDescriptor&& sockfd) : Base(std::move(sockfd)),
    input_address([this] () -> Address {
      sockaddr_in addr;
      socklen_t addr_size = sizeof(sockaddr_in);
      if (getpeername(this->sockfd.get(), (sockaddr*) &addr, &addr_size) == -1)
        throw std::system_error(errno, std::generic_category(), "getpeername failed");
      if (addr_size > sizeof(sockaddr_in))
        throw std::runtime_error("getpeername returned more bytes than sockaddr_in can hold");
      std::unique_ptr<char[]> dst(std::make_unique<char[]>(sizeof(char) * INET_ADDRSTRLEN));
      if (inet_ntop(AF_INET, &addr.sin_addr, dst.get(), INET_ADDRSTRLEN) != dst.get())
        throw std::system_error(errno, std::generic_category(), "inet_ntop failed");
      return Address{std::string(dst.get()), static_cast<short>(addr.sin_port)};
    }()) {}

Connected::Connected(const Address& to) : input_address(to) {
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(input_address.port());
  if (!inet_aton(input_address.ip().c_str(), &addr.sin_addr))
    throw std::runtime_error("invalid address");
  if (connect(sockfd.get(), (sockaddr*)&addr, sizeof(sockaddr_in)) == -1)
    throw std::system_error(errno, std::generic_category(), "socket connect failed");
}

Connected::Connected(Connected&& o) : Base(std::move(o)), input_address(o.input_address) {}

Connected::~Connected(void) {
  if (sockfd.get() != -1 && shutdown(sockfd.get(), SHUT_RDWR) == -1)
    if (errno != ENOTCONN) // ok to shut down a disconnected socket
      std::cerr << "WARNING: socket shutdown failed: " << std::strerror(errno) << std::endl;
}

std::string Connected::get_ip(void) const {
  sockaddr_in name;
  socklen_t name_size = sizeof(name);
  if (getsockname(sockfd.get(), (sockaddr*) &name, &name_size) == -1)
    throw std::system_error(errno, std::generic_category(), "getsockname failed");
  if (name_size > sizeof(name))
    throw std::runtime_error("getsockname returned more bytes than sockaddr_in can hold");
  std::unique_ptr<char[]> dst(std::make_unique<char[]>(sizeof(char) * INET_ADDRSTRLEN));
  if (inet_ntop(AF_INET, &name.sin_addr, dst.get(), INET_ADDRSTRLEN) != dst.get())
    throw std::system_error(errno, std::generic_category(), "inet_ntop failed");
  return std::string(dst.get());
}

bool Connected::read(void* buf, size_t count, int timeout_ms) {
  timeval timeout;
  if (timeout_ms == -1) {
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
  } else {
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
  }
  setsockopt(sockfd.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeval));
  size_t received = 0;
  do {
    int ret = ::read(sockfd.get(), &((char*) buf)[received], count - received);
    if (ret == 0 || ret == -1) {
      if (errno == EAGAIN) return false; // timed out
      throw std::system_error(errno, std::generic_category(), "socket read failed");
    }
    received += ret;
  } while(received < count);
  return true;
}

Address Connected::get_input_address(void) const noexcept {
  return input_address;
}

Bidirectional::Bidirectional(const Address& to) : Connected(to), address({"", 0}) {}

Bidirectional::Bidirectional(Listening& listener) : Connected([&listener] (void) -> FileDescriptor {
      sockaddr_in addr;
      socklen_t addr_size = sizeof(sockaddr_in);
      FileDescriptor fd([&listener, &addr, &addr_size] (void) -> int {
            int sockfd;
            if ((sockfd = ::accept(listener.sockfd.get(), (sockaddr*) &addr, &addr_size)) == -1)
              throw std::system_error(errno, std::generic_category(), "socket accept failed");
            return sockfd;
          }());
      if (addr_size != sizeof(sockaddr_in))
        throw std::runtime_error("accept returned incorrect number of bytes");
      return fd;
    }()), address([this] (void) -> Address {
      sockaddr_in addr;
      socklen_t addr_size = sizeof(sockaddr_in);
      if (getsockname(this->sockfd.get(), (sockaddr*) &addr, &addr_size) == -1)
        throw std::system_error(errno, std::generic_category(), "getsockname failed");
      if (addr_size > sizeof(sockaddr_in))
        throw std::runtime_error("getsockname returned more bytes than sockaddr_in can hold");
      return Address{get_my_ip(), static_cast<short>(ntohs(addr.sin_port))};
    }()) {}

Bidirectional::Bidirectional(Bidirectional && o) : Connected(std::move(o)), address(o.address) {}

void Bidirectional::write(const void* buf, size_t count) {
  size_t sent = 0;
  do {
    int ret = ::send(sockfd.get(), &((char*) buf)[sent], count - sent, MSG_NOSIGNAL);
    if (ret == -1)
      throw std::system_error(errno, std::generic_category(), "socket write failed");
    sent += ret;
  } while (sent < count);
}

Address Bidirectional::get_address(void) const noexcept {
  return address;
}

Listening::Listening(short port) : address{get_my_ip(), port}, listen_epfd(epoll_create(1)),
    connections_epfd(epoll_create(1)) {
  if (listen_epfd == -1 || connections_epfd == -1)
    throw std::system_error(errno, std::generic_category(), "epoll create failed");
  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY; // Automatically select an IP address
  if (bind(sockfd.get(), (sockaddr*)&addr, sizeof(sockaddr_in)) == -1)
    throw std::system_error(errno, std::generic_category(), "socket bind failed");
  if (listen(sockfd.get(), BACKLOG) == -1)
    throw std::system_error(errno, std::generic_category(), "socket listen failed");
  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = sockfd.get();
  if (epoll_ctl(listen_epfd.get(), EPOLL_CTL_ADD, sockfd.get(), &ev) == -1)
    throw std::system_error(errno, std::generic_category(), "epoll_ctl failed");
}

Listening::Listening(Listening&& o) : Base(std::move(o)), address(o.address),
    listen_epfd(std::move(o.listen_epfd)), connections_epfd(std::move(o.connections_epfd)),
    _connections(std::move(o._connections)), mutex(std::move(o.mutex)) {}

Listening::~Listening(void) {
  if (sockfd.get() != -1)
    if (shutdown(sockfd.get(), SHUT_RDWR) == -1)
      std::cerr << "WARNING: socket shutdown failed: " << std::strerror(errno) << std::endl;
}

std::shared_ptr<Bidirectional> Listening::accept(int timeout_ms) {
  epoll_event ev;
  int ret = epoll_wait(listen_epfd.get(), &ev, 1, timeout_ms);
  if ((ret == -1 && errno == EINTR) || ret == 0) // interrupted or timeout
    return nullptr;
  if (ret == -1)
    throw std::system_error(errno, std::generic_category(), "epoll wait failed");
  assert(ret == 1);
  assert(ev.events & EPOLLIN);
  std::shared_ptr<Bidirectional> accepted(std::make_shared<Bidirectional>(*this));

  epoll_event con_ev;
  con_ev.events = EPOLLRDHUP;
  con_ev.data.fd = accepted->sockfd.get();
  if (epoll_ctl(connections_epfd.get(), EPOLL_CTL_ADD, con_ev.data.fd, &con_ev) == -1)
    throw std::system_error(errno, std::generic_category(), "epoll_ctl failed");
  std::lock_guard<Mutex> lock(mutex);
  _connections.emplace_back(accepted);
  return _connections.back();
}

void Listening::broadcast(const void* buf, size_t count) {
  std::lock_guard<Mutex> lock(mutex);
  for (std::list<std::shared_ptr<Bidirectional>>::iterator c = _connections.begin();
      c != _connections.end(); ) {
    try {
      (*c)->write(buf, count);
      c++;
    } catch (const std::system_error& e) {
      c = _connections.erase(c);
    }
  }
}

size_t Listening::connections(void) const noexcept { return _connections.size(); }

bool Listening::remove_disconnected(int timeout_ms) {
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
  _connections.remove_if([&ev, &ret] (const std::shared_ptr<Bidirectional>& c) -> bool {
      for (int i = 0; i < ret; i++)
        if (c->sockfd.get() == ev[i].data.fd)
          return true;
    return false;
  });
  return true;
}

Address Listening::get_address(void) const noexcept {
  return address;
}

#define DEFAULT_CONNECT_IP "8.8.8.8"
#define DEFAULT_CONNECT_PORT 53
std::string get_my_ip(void) {
  return Connected(Address(DEFAULT_CONNECT_IP, DEFAULT_CONNECT_PORT)).get_ip();
}
} // namespace socket
} // namespace wrapper
