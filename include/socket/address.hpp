#ifndef WRAPPER_SOCKET_ADDRESS_HPP
#define WRAPPER_SOCKET_ADDRESS_HPP

#include <arpa/inet.h>
#include <iostream>
#include <utility>

namespace wrapper {
namespace socket {

class Address {
  private:
    char _ip[INET_ADDRSTRLEN];
    unsigned short _port;
  public:
    Address(const std::string& ip, unsigned short port);
    const std::string ip(void) const;
    const unsigned short port(void) const;
    bool operator==(const Address& o) const;
    bool operator!=(const Address& o) const;
};
std::ostream& operator<<(std::ostream& os, const Address& a);

static_assert(std::is_trivially_copyable<Address>::value);

} // namespace socket
} // namespace wrapper
#endif
