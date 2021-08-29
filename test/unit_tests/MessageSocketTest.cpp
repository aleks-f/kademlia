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

#include "Poco/Net/SocketProactor.h"
#include "kademlia/endpoint.hpp"
#include "kademlia/MessageSocket.h"
#include "kademlia/SocketAdapter.h"
#include "kademlia/detail/Util.h"
#include "Network.h"
#include "gtest/gtest.h"

namespace {

namespace k = kademlia;
namespace kd = kademlia::detail;

using MessageSocket_type = kd::MessageSocket<kd::SocketAdapter<Poco::Net::DatagramSocket>>;


TEST(MessageSocketTest, faulty_address_are_detected)
{
	Poco::Net::SocketProactor io_service;

	{
		k::endpoint const endpoint{ "error", "27980" };

		EXPECT_THROW(MessageSocket_type::resolve_endpoint(endpoint);,
					 std::exception);
	}
}

TEST(MessageSocketTest, dns_can_be_resolved)
{
	Poco::Net::SocketProactor io_service;

	k::endpoint const endpoint{ "localhost", "27980" };

	auto const e = MessageSocket_type::resolve_endpoint(endpoint);
	EXPECT_LE(1, e.size());
}

TEST(MessageSocketTest, ipv4_address_can_be_resolved)
{
	Poco::Net::SocketProactor io_service;

	k::endpoint const endpoint{ "127.0.0.1", "27980" };

	auto const e = MessageSocket_type::resolve_endpoint(endpoint);

	EXPECT_LE(1, e.size());
}

TEST(MessageSocketTest, ipv6_address_can_be_resolved)
{
	Poco::Net::SocketProactor io_service;

	k::endpoint const endpoint{ "::1", "27980" };

	auto e = MessageSocket_type::resolve_endpoint(endpoint);
	EXPECT_LE/*EQ*/(1, e.size());
}


TEST(MessageSocketTest, ipv4_socket_can_be_created)
{
	Poco::Net::SocketProactor io_service;

	k::endpoint const endpoint("127.0.0.1", kd::getAvailablePort());

	EXPECT_NO_THROW(MessageSocket_type::ipv4(io_service, endpoint););
}

TEST(MessageSocketTest, ipv6_socket_can_be_created)
{
	Poco::Net::SocketProactor io_service;

	k::endpoint const endpoint("::1", kd::getAvailablePort());

	EXPECT_NO_THROW(MessageSocket_type::ipv6(io_service, endpoint););
}

}
