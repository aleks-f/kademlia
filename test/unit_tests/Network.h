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

#ifndef KADEMLIA_TEST_HELPERS_NETWORK_H
#define KADEMLIA_TEST_HELPERS_NETWORK_H

#include <cstdint>
#include "Poco/Net/Socket.h"
#include "Poco/Net/NetException.h"
#include "kademlia/SocketAdapter.h"
#include "common.hpp"

namespace kademlia {
namespace test {

template< typename Socket >
int createSocket(std::string const& ip, std::uint16_t port)
{
	int err = 0;
	Poco::Net::SocketAddress sa(ip, port);
	Poco::Net::DatagramSocket sock(sa.family());
	try
	{
		if (sa.family() == Poco::Net::SocketAddress::IPv4)
			sock.bind(sa, false);
		else if (sa.family() == Poco::Net::SocketAddress::IPv6)
			sock.bind6(sa, false, false, true);
		else
			throw Poco::InvalidArgumentException("createSocket()", sa.toString());
	}
	catch (Poco::IOException&)
	{
		err = Socket::lastError();
		if (!err) // if nothing, one more attempt
			sock.getOption(SOL_SOCKET, SO_ERROR, err);
	}

	return err;
}

void checkListening(std::string const& ip, std::uint16_t port);

//std::uint16_t getTemporaryListeningPort(Poco::Net::IPAddress::Family family = Poco::Net::SocketAddress::IPv4,
//	std::uint16_t port = 1234);

} // namespace test
} // namespace kademlia

#endif

