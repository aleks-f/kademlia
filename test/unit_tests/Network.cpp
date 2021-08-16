// Copyright (c) 2013-2014, David Keller
// All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//         * Redistributions of source code must retain the above copyright
//           notice, this list of conditions and the following disclaimer.
//         * Redistributions in binary form must reproduce the above copyright
//           notice, this list of conditions and the following disclaimer in the
//           documentation and/or other materials provided with the distribution.
//         * Neither the name of the University of California, Berkeley nor the
//           names of its contributors may be used to endorse or promote products
//           derived from this software without specific prior written permission.
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

#include "Network.h"
#include "Poco/Net/SocketDefs.h"
#include "Poco/Net/DatagramSocket.h"
#include "gtest/gtest.h"

using namespace Poco::Net;

namespace kademlia {
namespace test {

void checkListening(std::string const& ip, std::uint16_t port)
{
	auto udp_failure = createSocket<DatagramSocket>(ip, port);
	EXPECT_EQ(POCO_EADDRINUSE, udp_failure);
}

std::uint16_t getTemporaryListeningPort(IPAddress::Family family, std::uint16_t port)
{
	bool failed = false;
	do
	{
		try
		{
			failed = false;
			SocketAddress sa(family, port++);
			DatagramSocket socket(sa);
		}
		catch(NetException&)
		{
			failed = true;
		}
	}
	while (failed && Socket::lastError() == POCO_EADDRINUSE);
	return port;
}

} // namespace test
} // namespace kademlia

