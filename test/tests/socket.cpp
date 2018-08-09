#include <future>

#include "gtest/gtest.h"
#include "socket.hpp"

#define IP "127.0.0.1"
#define PORT 8888

namespace wrapper {
namespace socket {

TEST(Socket, ConstructDestruct) {
  Listening(PORT);
}

TEST(Socket, TwoSameAddress) {
  Listening s1(PORT);
  EXPECT_THROW(new Listening(PORT), std::exception);
}

TEST(Socket, Connect) {
  Listening s(PORT);
  std::future<std::shared_ptr<Connected>> outF = std::async(&Listening::accept, &s, -1);
  Connected in(IP, PORT);
  outF.get();
}

TEST(Socket, SendReceive1) {
  Listening s(PORT);
  std::future<std::shared_ptr<Connected>> outF = std::async(&Listening::accept, &s, -1);
  Connected in(IP, PORT);
  std::shared_ptr<Connected> out = outF.get();
  for (uint32_t input = 0; input < 0xabcd; input++) {
    uint32_t network_format = htonl(input);
    out->write(&network_format, sizeof(uint32_t));
    uint32_t output;
    in.read(&output, sizeof(uint32_t));
    output = ntohl(output);
    EXPECT_EQ(input, output);
  }
}

TEST(Socket, SendReceive2) {
  Listening s(PORT);
  std::future<std::shared_ptr<Connected>> outF1 = std::async(&Listening::accept, &s, -1);
  Connected in1(IP, PORT);
  std::shared_ptr<Connected> out1 = outF1.get();
  std::future<std::shared_ptr<Connected>> outF2 = std::async(&Listening::accept, &s, -1);
  Connected in2(IP, PORT);
  std::shared_ptr<Connected> out2 = outF2.get();
  for (uint32_t input = 0; input < 0xabcd; input++) {
    uint32_t network_format = htonl(input);
    out1->write(&network_format, sizeof(uint32_t));
    out2->write(&network_format, sizeof(uint32_t));
    uint32_t output;
    in1.read(&output, sizeof(uint32_t));
    output = ntohl(output);
    EXPECT_EQ(input, output);
    in2.read(&output, sizeof(uint32_t));
    output = ntohl(output);
    EXPECT_EQ(input, output);
  }
}

TEST(Socket, Broadcast1) {
  Listening s(PORT);
  std::future<std::shared_ptr<Connected>> outF = std::async(&Listening::accept, &s, -1);
  Connected in(IP, PORT);
  outF.get();// make async return
  for (uint32_t input = 0; input < 0xabcd; input++) {
    uint32_t network_format = htonl(input);
    s.broadcast(&network_format, sizeof(uint32_t));
    uint32_t output;
    in.read(&output, sizeof(uint32_t));
    output = ntohl(output);
    EXPECT_EQ(input, output);
  }
}

TEST(Socket, Broadcast2) {
  Listening s(PORT);
  std::future<std::shared_ptr<Connected>> outF1 = std::async(&Listening::accept, &s, -1);
  Connected in1(IP, PORT);
  outF1.get();// make async return
  std::future<std::shared_ptr<Connected>> outF2 = std::async(&Listening::accept, &s, -1);
  Connected in2(IP, PORT);
  outF2.get();// make async return
  for (uint32_t input = 0; input < 0xabcd; input++) {
    uint32_t network_format = htonl(input);
    s.broadcast(&network_format, sizeof(uint32_t));
    uint32_t output;
    in1.read(&output, sizeof(uint32_t));
    output = ntohl(output);
    EXPECT_EQ(input, output);
    in2.read(&output, sizeof(uint32_t));
    output = ntohl(output);
    EXPECT_EQ(input, output);
  }
}

TEST(Socket, Broadcast3) {
  Listening s(PORT);
  std::list<Connected> connections;
  for (uint32_t input = 0; input < 128; input++) {
    std::future<std::shared_ptr<Connected>> outF = std::async(&Listening::accept, &s, -1);
    connections.emplace_back(IP, PORT);
    outF.get();// make async return
    uint32_t network_format = htonl(input);
    s.broadcast(&network_format, sizeof(uint32_t));
    for (Connected& connection : connections) {
      uint32_t output;
      connection.read(&output, sizeof(uint32_t));
      output = ntohl(output);
      EXPECT_EQ(input, output);
    }
  }
}

TEST(Socket, Disconnect) {
  Listening s(PORT);
  for (uint32_t n_connections = 0; n_connections < 2048; n_connections++) {
    if (n_connections > 0) s.remove_disconnected(-1);
    std::future<std::shared_ptr<Connected>> outF = std::async(&Listening::accept, &s, -1);
    Connected c(IP, PORT);
    outF.get();
    uint32_t network_format = htonl(n_connections);
    s.broadcast(&network_format, sizeof(uint32_t));
    uint32_t output;
    c.read(&output, sizeof(uint32_t));
    output = ntohl(output);
    EXPECT_EQ(n_connections, output);
  }
}

TEST(Socket, Disconnect2) {
  const int n_connections = 128;
  Listening s(PORT);
  for (uint32_t i = 0; i < 16; i++) {
    if (i > 0) s.remove_disconnected(-1);
    std::vector<std::unique_ptr<Connected>> connections;
    for (uint32_t n = 0; n < n_connections; n++) {
      std::future<std::shared_ptr<Connected>> outF = std::async(&Listening::accept, &s, -1);
      connections.emplace_back(std::make_unique<Connected>(IP, PORT));
      outF.get();
    }
    uint32_t network_format = htonl(i);
    s.broadcast(&network_format, sizeof(uint32_t));
    for (uint32_t n = 0; n < n_connections; n++) {
      if (n % 2) {
        connections[n].reset(); // close connection
        continue;
      }
      uint32_t output;
      connections[n]->read(&output, sizeof(uint32_t));
      output = ntohl(output);
      EXPECT_EQ(i, output);
    }
  }
}

#define DEFAULT_CONNECT_IP "8.8.8.8"
#define DEFAULT_CONNECT_PORT 53
TEST(Socket, DetectIP) {
  std::string my_ip  = Connected(DEFAULT_CONNECT_IP, DEFAULT_CONNECT_PORT).get_ip();
  std::cout << "Detect IP returned: " << my_ip << std::endl;
  EXPECT_NE("", my_ip);
}
} // namespace socket
} // namespace wrapper
