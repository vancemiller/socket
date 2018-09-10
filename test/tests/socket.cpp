#include <future>

#include "gtest/gtest.h"
#include "socket/socket.hpp"

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
  std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s, -1);
  Connected in(s.get_address());
  outF.get();
}

TEST(Socket, Move) {
  Listening s(PORT);
  Listening s2 = std::move(s);
  std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s2, -1);
  Connected in(s.get_address());
  outF.get();
}

TEST(Socket, Move2) {
  Listening s(PORT);
  Listening s2 = std::move(s);
  std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s2, -1);
  Connected in(s.get_address());
  outF.get();
  Connected in2(std::move(in));
}

TEST(Socket, SendReceive1) {
  Listening s(PORT);
  std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s, -1);
  Connected in(s.get_address());
  std::shared_ptr<Bidirectional> out = outF.get();
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
  std::future<std::shared_ptr<Bidirectional>> outF1 = std::async(&Listening::accept, &s, -1);
  Connected in1(s.get_address());
  std::shared_ptr<Bidirectional> out1 = outF1.get();
  std::future<std::shared_ptr<Bidirectional>> outF2 = std::async(&Listening::accept, &s, -1);
  Connected in2(s.get_address());
  std::shared_ptr<Bidirectional> out2 = outF2.get();
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
  std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s, -1);
  Connected in(s.get_address());
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
  std::future<std::shared_ptr<Bidirectional>> outF1 = std::async(&Listening::accept, &s, -1);
  Connected in1(s.get_address());
  outF1.get();// make async return
  std::future<std::shared_ptr<Bidirectional>> outF2 = std::async(&Listening::accept, &s, -1);
  Connected in2(s.get_address());
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
    std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s, -1);
    connections.emplace_back(s.get_address());
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
  const int n_connections_max = 128;
  Listening s(PORT);
  for (uint32_t n_connections = 0; n_connections < n_connections_max; n_connections++) {
    if (n_connections > 0) s.remove_disconnected(-1);
    std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s, -1);
    Connected c(s.get_address());
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
      std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s, -1);
      connections.emplace_back(std::make_unique<Connected>(s.get_address()));
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

TEST(Socket, DetectIP) {
  std::string my_ip  = get_my_ip();
  std::cout << "Detect IP returned: " << my_ip << std::endl;
  EXPECT_NE("", my_ip);
}

TEST(Socket, DataNotAvailable) {
  Listening s(PORT);
  std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s, -1);
  Connected in(s.get_address());
  std::shared_ptr<Bidirectional> out = outF.get();
  EXPECT_FALSE(in.data_available());
}

TEST(Socket, DataAvailable) {
  Listening s(PORT);
  std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s, -1);
  Connected in(s.get_address());
  std::shared_ptr<Bidirectional> out = outF.get();
  uint32_t network_format = htonl(123);
  out->write(&network_format, sizeof(uint32_t));
  EXPECT_TRUE(in.data_available());
}

TEST(Socket, Timeout) {
  Listening s(PORT);
  std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s, -1);
  Connected in(s.get_address());
  std::shared_ptr<Bidirectional> out = outF.get();
  uint32_t output;
  EXPECT_FALSE(in.read(&output, sizeof(uint32_t), 10));
}

TEST(Socket, PartialTimeout) {
  Listening s(PORT);
  std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s, -1);
  Connected in(s.get_address());
  std::shared_ptr<Bidirectional> out = outF.get();
  uint32_t output;
  char d = 'a';
  out->write(&d, sizeof(char));
  EXPECT_FALSE(in.read(&output, sizeof(uint32_t), 10));
}

TEST(Socket, ConnectedAddress) {
  Listening s(PORT);
  EXPECT_EQ(Address(get_my_ip(), PORT), s.get_address());
  std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s, -1);
  Connected in(s.get_address());
  std::shared_ptr<Bidirectional> out = outF.get();
  EXPECT_EQ(s.get_address(), in.get_listening_address());
}

TEST(Socket, BidirectionalAddress) {
  Listening s(PORT);
  std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s, -1);
  Bidirectional a1(s.get_address());
  std::shared_ptr<Bidirectional> a2 = outF.get();
  EXPECT_EQ(a1.get_listening_address(), s.get_address());
  EXPECT_EQ(a1.get_address(), a2->get_input_address());
  EXPECT_EQ(a1.get_input_address(), a2->get_address());
}

TEST(Socket, TwoSocketDifferentAddress) {
  Listening s(PORT);
  std::future<std::shared_ptr<Bidirectional>> outF = std::async(&Listening::accept, &s, -1);
  Bidirectional a1(s.get_address());
  std::shared_ptr<Bidirectional> a2 = outF.get();
  outF = std::async(&Listening::accept, &s, -1);
  Bidirectional b1(s.get_address());
  std::shared_ptr<Bidirectional> b2 = outF.get();
  EXPECT_EQ(a1.get_address(), a2->get_input_address());
  EXPECT_EQ(a1.get_input_address(), a2->get_address());
  EXPECT_EQ(b1.get_address(), b2->get_input_address());
  EXPECT_EQ(b1.get_input_address(), b2->get_address());
  EXPECT_EQ(a1.get_listening_address(), b2->get_listening_address());
}

TEST(Socket, Bidirectional) {
  const int count = 123;
  Listening s(PORT);
  std::future<std::shared_ptr<Bidirectional>> ioA_F = std::async(&Listening::accept, &s, -1);
  Bidirectional ioB(s.get_address());
  std::shared_ptr<Bidirectional> ioA = ioA_F.get();
  for (int i = 0; i < count; i++) {
    {
      uint32_t network_format = htonl(i);
      ioA->write(&network_format, sizeof(uint32_t));
      uint32_t output;
      ioB.read(&output, sizeof(uint32_t));
      output = ntohl(output);
      EXPECT_EQ(i, output);
    }
    {
      uint32_t network_format = htonl(i * 2);
      ioB.write(&network_format, sizeof(uint32_t));
      uint32_t output;
      ioA->read(&output, sizeof(uint32_t));
      output = ntohl(output);
      EXPECT_EQ(i * 2, output);
    }
  }
}
} // namespace socket
} // namespace wrapper
