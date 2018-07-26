#include <future>

#include "gtest/gtest.h"
#include "socket.hpp"

#define IP "127.0.0.1"
#define PORT 8888
TEST(Socket, ConstructDestruct) {
  ListeningSocket(PORT);
}

TEST(Socket, TwoSameAddress) {
  ListeningSocket s1(PORT);
  EXPECT_THROW(new ListeningSocket(PORT), std::exception);
}

TEST(Socket, Connect) {
  ListeningSocket s(PORT);
  std::future<RWSocket&> outF = std::async(&ListeningSocket::accept, &s);
  ConnectedSocket in(IP, PORT);
  outF.get();
}

TEST(Socket, SendReceive) {
  ListeningSocket s(PORT);
  std::future<RWSocket&> outF = std::async(&ListeningSocket::accept, &s);
  ConnectedSocket in(IP, PORT);
  RWSocket& out = outF.get();
  for (uint32_t input = 0; input < 0xabcd; input++) {
    uint32_t network_format = htonl(input);
    out.write(&network_format, sizeof(uint32_t));
    uint32_t output;
    in.read(&output, sizeof(uint32_t));
    output = ntohl(output);
    EXPECT_EQ(input, output);
  }
}

