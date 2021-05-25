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

#ifndef KADEMLIA_IP_ENDPOINT_H
#define KADEMLIA_IP_ENDPOINT_H

#ifdef _MSC_VER
#   pragma once
#endif

#include <iosfwd>
#include <cstdint>
#include <string>
#include "Poco/Net/IPAddress.h"


namespace kademlia {
namespace detail {


struct IPEndpoint final
{
	Poco::Net::IPAddress address_;
	uint16_t port_;
};


inline IPEndpoint toIPEndpoint( std::string const& ip, std::uint16_t port )
{
	return IPEndpoint{ Poco::Net::IPAddress(ip), port };
}


std::ostream& operator << (std::ostream & out, IPEndpoint const& i);


inline bool operator == (const IPEndpoint & a, const IPEndpoint & b)
{
	return a.address_ == b.address_ && a.port_ == b.port_;
}


inline bool operator != (IPEndpoint const& a, IPEndpoint const& b)
{
	return ! ( a == b );
}


} // namespace detail
} // namespace kademlia

#endif
