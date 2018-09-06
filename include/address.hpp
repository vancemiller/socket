#ifndef WRAPPER_SOCKET_ADDRESS_HPP
#define WRAPPER_SOCKET_ADDRESS_HPP

#include <iostream>
#include <string>
#include <utility>

namespace wrapper {
namespace socket {

class Address {
  public:
    const std::pair<std::string, short> address;
  public:
    Address(const std::pair<std::string, short>& address);
    Address(const std::string& ip, short port);
    const std::string& ip(void) const;
    const short port(void) const;
    bool operator==(const Address& o) const;
    bool operator!=(const Address& o) const;
};
std::ostream& operator<<(std::ostream& os, const Address& a);

} // namespace socket
} // namespace wrapper
#endif
