// Copyright (c) 2013-2014, David Keller
// All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//	 * Redistributions of source code must retain the above copyright
//	   notice, this list of conditions and the following disclaimer.
//	 * Redistributions in binary form must reproduce the above copyright
//	   notice, this list of conditions and the following disclaimer in the
//	   documentation and/or other materials provided with the distribution.
//	 * Neither the name of the University of California, Berkeley nor the
//	   names of its contributors may be used to endorse or promote products
//	   derived from this software without specific prior written permission.
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

#include "kademlia/error.hpp"
#include "kademlia/Session.h"
#include "kademlia/detail/Util.h"
#include "common.hpp"
#include "Network.h"
#include "gtest/gtest.h"
#include <cstdint>


namespace {

namespace k = kademlia;
namespace kd = kademlia::detail;
using namespace Poco::Net;
using Session = Kademlia::Session;

TEST(FirstSessionTest, opens_sockets_on_all_interfaces_by_default)
{
	Session s;

	k::test::checkListening("0.0.0.0", Session::DEFAULT_PORT);
	k::test::checkListening("::", Session::DEFAULT_PORT);
}

TEST(FirstSessionTest, opens_both_ipv4_ipv6_sockets)
{
	// Create listening socket.
	std::uint16_t const port1 = kd::getAvailablePort(SocketAddress::IPv4);
	std::uint16_t const port2 = kd::getAvailablePort(SocketAddress::IPv6);
	k::endpoint ipv4_endpoint{ "127.0.0.1", port1 };
	k::endpoint ipv6_endpoint{ "::1", port2 };

	Session s{ ipv4_endpoint, ipv6_endpoint };

	k::test::checkListening("127.0.0.1", port1);
	k::test::checkListening("::1", port2);
}

TEST(FirstSessionTest, throw_on_invalid_ipv6_address)
{
	// Create listening socket.
	std::uint16_t const port1 = kd::getAvailablePort(SocketAddress::IPv4);
	std::uint16_t const port2 = kd::getAvailablePort(SocketAddress::IPv6);
	k::endpoint ipv4_endpoint{ "127.0.0.1", port1 };
	k::endpoint ipv6_endpoint{ "0.0.0.0", port2 };

	EXPECT_THROW(Session s(ipv4_endpoint, ipv6_endpoint),std::exception);
}

TEST(FirstSessionTest, throw_on_invalid_ipv4_address)
{
	// Create listening socket.
	std::uint16_t const port1 = kd::getAvailablePort(SocketAddress::IPv4);
	std::uint16_t const port2 = kd::getAvailablePort(SocketAddress::IPv6);
	k::endpoint ipv4_endpoint{ "::", port1 };
	k::endpoint ipv6_endpoint{ "::1", port2 };

	EXPECT_THROW(Session s(ipv4_endpoint, ipv6_endpoint), std::exception);
}


TEST(FirstSessionTest, run_can_be_aborted)
{
	Session s;
	s.abort();
	auto result = s.wait();
	EXPECT_TRUE(result/*.get()*/ == k::RUN_ABORTED);
}

}

