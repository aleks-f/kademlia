// Copyright (c) 2013-2014, David Keller
// All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the University of California, Berkeley nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY DAVID KELLER AND CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include "Poco/Net/DatagramSocket.h"
#include "kademlia/SocketAdapter.h"
#include "kademlia/log.hpp"
#include "Network.h"
#include "gtest/gtest.h"
#include "TaskFixture.h"

namespace {

using namespace kademlia::detail;
using namespace kademlia::test;
using namespace Poco::Net;

struct SocketAdapterTest : TaskFixture
{
	SocketAdapterTest(): TaskFixture()
	{
		kademlia::detail::enable_log_for("SocketAdapterTest");
		LOG_DEBUG(SocketAdapterTest, this) << "Created SocketAdapterTest." << std::endl;
	}
};


TEST_F(SocketAdapterTest, copy_and_move)
{
	std::uint16_t port = getTemporaryListeningPort(SocketAddress::IPv4);
	SocketAddress addr("127.0.0.1", port);
	Poco::Net::SocketReactor ioService;
	SocketAdapter<DatagramSocket> sock1(&ioService, addr, false, true);
	LOG_DEBUG(SocketAdapterTest, this) << sock1.address().toString() << std::endl;
	SocketAdapter<DatagramSocket> sock2(sock1);
	EXPECT_EQ(sock1.address(), sock2.address());
	SocketAdapter<DatagramSocket> sock3(std::move(sock1));
	EXPECT_EQ(sock2.address(), sock3.address());
	SocketAdapter<DatagramSocket> sock4(&ioService, SocketAddress(), false, true);
	EXPECT_EQ(SocketAddress().host(), sock4.address().host());
	sock4 = std::move(sock3);
	EXPECT_EQ(sock2.address(), sock4.address());
}


TEST_F(SocketAdapterTest, send_to_recv_from_ipv4)
{
	std::size_t sent = 0, recvd = 0;

	auto onSendCompletion = [&](boost::system::error_code const& failure, std::size_t bytes_sent)
	{
		sent += bytes_sent;
		LOG_DEBUG(SocketAdapterTest, this) << "onSendCompletion: sent " << bytes_sent << " bytes" << std::endl;
	};

	auto onRecvCompletion = [&](boost::system::error_code const& failure, std::size_t bytes_received)
	{
		recvd += bytes_received;
		LOG_DEBUG(SocketAdapterTest, this) << "onRecvCompletion: received " << bytes_received << " bytes" << std::endl;
	};

	Poco::Net::SocketReactor ioService;

	std::uint16_t port1 = getTemporaryListeningPort(SocketAddress::IPv4);
	std::uint16_t port2 = getTemporaryListeningPort(SocketAddress::IPv4, port1);
	SocketAddress addr1("127.0.0.1", port1);
	SocketAddress addr2("127.0.0.1", port2);

	SocketAdapter<DatagramSocket> sock1(&ioService, addr1, false, true);
	SocketAdapter<DatagramSocket> sock2(&ioService, addr2, false, true);

	std::string hello = "hello";
	boost::asio::const_buffer sendBuf(hello.data(), hello.length());
	sock1.asyncSendTo(sendBuf, addr2, onSendCompletion);
	boost::asio::mutable_buffer recvBuf;
	sock2.asyncReceiveFrom(recvBuf, addr1, onRecvCompletion);

	while (recvd < hello.size()) ioService.poll();

	EXPECT_GT(sent, 0);
	EXPECT_EQ(sent, recvd);
	EXPECT_EQ(recvd, hello.size());
}


TEST_F(SocketAdapterTest, send_to_recv_from_ipv6)
{
	std::size_t sent = 0;
	std::size_t recvd = 0;
	auto onSendCompletion = [&](boost::system::error_code const& failure, std::size_t bytes_sent)
	{
		sent += bytes_sent;
		LOG_DEBUG(SocketAdapterTest, this) << "onSendCompletion: sent " << bytes_sent << " bytes" << std::endl;
	};

	auto onRecvCompletion = [&](boost::system::error_code const& failure, std::size_t bytes_received)
	{
		recvd += bytes_received;
		LOG_DEBUG(SocketAdapterTest, this) << "onRecvCompletion: received " << bytes_received << " bytes" << std::endl;
	};

	Poco::Net::SocketReactor ioService;

	std::uint16_t port1 = getTemporaryListeningPort(SocketAddress::IPv6);
	std::uint16_t port2 = getTemporaryListeningPort(SocketAddress::IPv6, port1);
	SocketAddress addr1("::1", port1);
	SocketAddress addr2("::1", port2);

	SocketAdapter<DatagramSocket> sock1(&ioService, addr1, false, true);
	SocketAdapter<DatagramSocket> sock2(&ioService, addr2, false, true);

	std::string hello = "hello";
	boost::asio::const_buffer sendBuf(hello.data(), hello.length());
	sock1.asyncSendTo(sendBuf, addr2, onSendCompletion);
	boost::asio::mutable_buffer recvBuf;
	sock2.asyncReceiveFrom(recvBuf, addr1, onRecvCompletion);

	while (recvd < hello.size()) ioService.poll();

	EXPECT_GT(sent, 0);
	EXPECT_EQ(sent, recvd);
	EXPECT_EQ(recvd, hello.size());
}


}

