#ifndef WRAPPER_SOCKET_ADDRESS_HPP
#define WRAPPER_SOCKET_ADDRESS_HPP

#include <iostream>
#include <string>
#include <utility>

namespace wrapper {
namespace socket {

class Address {
  public:
    const std::pair<std::string, unsigned short> address;
  public:
    Address(const std::pair<std::string, unsigned short>& address);
    Address(const std::string& ip, unsigned short port);
    const std::string& ip(void) const;
    const unsigned short port(void) const;
    bool operator==(const Address& o) const;
    bool operator!=(const Address& o) const;
};
std::ostream& operator<<(std::ostream& os, const Address& a);

} // namespace socket
} // namespace wrapper
#endif
