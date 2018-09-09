#include "socket/address.hpp"

namespace wrapper {
namespace socket {

Address::Address(const std::pair<std::string, unsigned short>& address) : address(address) {}

Address::Address(const std::string& ip, unsigned short port) : Address(std::make_pair(ip, port)) {}

const std::string& Address::ip(void) const { return address.first; }

const unsigned short Address::port(void) const { return address.second; }

bool Address::operator==(const Address& o) const { return address == o.address; }

bool Address::operator!=(const Address& o) const { return !(address == o.address); }

std::ostream& operator<<(std::ostream& os, const Address& a) {
  return os << a.ip() << ":" << std::to_string(a.port());
}

} // namespace socket
} // namespace wrapper
