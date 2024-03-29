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

#ifndef KADEMLIA_TEST_HELPERS_PEER_FACTORY_H
#define KADEMLIA_TEST_HELPERS_PEER_FACTORY_H

#include <cstdlib>

#include "kademlia/id.hpp"
#include "Poco/Net/SocketAddress.h"
#include "kademlia/Peer.h"

inline Poco::Net::SocketAddress
createEndpoint
    ( std::string const& ip = std::string{ "127.0.0.1" }
    , std::uint16_t const& service = 12345 )
{
    using endpoint_type = Poco::Net::SocketAddress;

    return endpoint_type{ Poco::Net::IPAddress(ip), service };
}

inline kademlia::detail::Peer
createPeer
    ( kademlia::detail::id const& id = kademlia::detail::id()
    , Poco::Net::SocketAddress const& endpoint = createEndpoint() )
{
    using peer_type = kademlia::detail::Peer;

    return peer_type{ id, endpoint };
}

#endif // KADEMLIA_TEST_HELPERS_PEER_FACTORY_H
