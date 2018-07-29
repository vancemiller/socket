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
  std::future<std::shared_ptr<RWSocket>> outF = std::async(&ListeningSocket::accept, &s, -1);
  ConnectedSocket in(IP, PORT);
  outF.get();
}

TEST(Socket, SendReceive1) {
  ListeningSocket s(PORT);
  std::future<std::shared_ptr<RWSocket>> outF = std::async(&ListeningSocket::accept, &s, -1);
  ConnectedSocket in(IP, PORT);
  std::shared_ptr<RWSocket> out = outF.get();
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
  ListeningSocket s(PORT);
  std::future<std::shared_ptr<RWSocket>> outF1 = std::async(&ListeningSocket::accept, &s, -1);
  ConnectedSocket in1(IP, PORT);
  std::shared_ptr<RWSocket> out1 = outF1.get();
  std::future<std::shared_ptr<RWSocket>> outF2 = std::async(&ListeningSocket::accept, &s, -1);
  ConnectedSocket in2(IP, PORT);
  std::shared_ptr<RWSocket> out2 = outF2.get();
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
  ListeningSocket s(PORT);
  std::future<std::shared_ptr<RWSocket>> outF = std::async(&ListeningSocket::accept, &s, -1);
  ConnectedSocket in(IP, PORT);
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
  ListeningSocket s(PORT);
  std::future<std::shared_ptr<RWSocket>> outF1 = std::async(&ListeningSocket::accept, &s, -1);
  ConnectedSocket in1(IP, PORT);
  outF1.get();// make async return
  std::future<std::shared_ptr<RWSocket>> outF2 = std::async(&ListeningSocket::accept, &s, -1);
  ConnectedSocket in2(IP, PORT);
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
  ListeningSocket s(PORT);
  std::list<ConnectedSocket> connections;
  for (uint32_t input = 0; input < 128; input++) {
    std::future<std::shared_ptr<RWSocket>> outF = std::async(&ListeningSocket::accept, &s, -1);
    connections.emplace_back(IP, PORT);
    outF.get();// make async return
    uint32_t network_format = htonl(input);
    s.broadcast(&network_format, sizeof(uint32_t));
    for (ConnectedSocket& connection : connections) {
      uint32_t output;
      connection.read(&output, sizeof(uint32_t));
      output = ntohl(output);
      EXPECT_EQ(input, output);
    }
  }
}

TEST(Socket, Disconnect) {
  ListeningSocket s(PORT);
  for (uint32_t n_connections = 0; n_connections < 2048; n_connections++) {
    std::future<std::shared_ptr<RWSocket>> outF = std::async(&ListeningSocket::accept, &s, -1);
    ConnectedSocket c(IP, PORT);
    outF.get();
    uint32_t network_format = htonl(n_connections);
    s.broadcast(&network_format, sizeof(uint32_t));
    uint32_t output;
    c.read(&output, sizeof(uint32_t));
    output = ntohl(output);
    EXPECT_EQ(n_connections, output);
  }
}

