#include <future>

#include "gtest/gtest.h"
#include "socket.hpp"

#define IP "127.0.0.1"
#define PORT 8888
TEST(Socket, ConstructDestruct) {
  Socket(PORT);
}

TEST(Socket, TwoSameAddress) {
  Socket s1(PORT);
  EXPECT_THROW(new Socket(PORT), std::exception);
}

TEST(Socket, Connect) {
  Socket s(PORT);
  std::future<ReadWriteSocket&> outF = std::async(&Socket::accept, &s);
  ReadOnlySocket in(IP, PORT);
  outF.get();
}

TEST(Socket, SendReceive) {
  Socket s(PORT);
  std::future<ReadWriteSocket&> outF = std::async(&Socket::accept, &s);
  ReadOnlySocket in(IP, PORT);
  ReadWriteSocket& out = outF.get();
  for (uint32_t input = 0; input < 0xabcd; input++) {
    uint32_t network_format = htonl(input);
    out.write(&network_format, sizeof(uint32_t));
    uint32_t output;
    in.read(&output, sizeof(uint32_t));
    output = ntohl(output);
    EXPECT_EQ(input, output);
  }
}

