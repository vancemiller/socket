#include "socket/address.hpp"

#include <cstring>

namespace wrapper {
namespace socket {

Address::Address(const std::string& ip, unsigned short port) : _port(port) {
  std::strncpy(_ip, ip.c_str(), INET_ADDRSTRLEN);
}

const std::string Address::ip(void) const { return std::string(_ip); }

const unsigned short Address::port(void) const { return _port; }

bool Address::operator==(const Address& o) const {
  return _port == o._port && !std::strncmp(_ip, o._ip, INET_ADDRSTRLEN);
}

bool Address::operator!=(const Address& o) const { return !(*this == o); }

std::ostream& operator<<(std::ostream& os, const Address& a) {
  return os << a.ip() << ":" << std::to_string(a.port());
}

} // namespace socket
} // namespace wrapper
