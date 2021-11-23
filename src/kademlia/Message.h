// Copyright (c) 2013, David Keller
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

#ifndef KADEMLIA_MESSAGE_H
#define KADEMLIA_MESSAGE_H

#ifdef _MSC_VER
#   pragma once
#endif

#include <iosfwd>
#include <cstdint>
#include <algorithm>
#include <system_error>
#include <vector>

#include <kademlia/detail/cxx11_macros.hpp>

#include "kademlia/Peer.h"
#include "kademlia/id.hpp"
#include "kademlia/buffer.hpp"

namespace kademlia {
namespace detail {


struct Header final
{
	enum version : std::uint8_t
	{
		V1 = 1,
	} version_;

	enum type : std::uint8_t
	{
		PING_REQUEST,
		PING_RESPONSE,
		STORE_REQUEST,
		FIND_PEER_REQUEST,
		FIND_PEER_RESPONSE,
		FIND_VALUE_REQUEST,
		FIND_VALUE_RESPONSE,
	} type_;

	id source_id_;
	id random_token_;
};


std::ostream& operator<<(std::ostream & out, Header::type const& h);

std::ostream & operator<<(std::ostream & out, Header const& h);

template<typename MessageBodyType>
struct message_traits;

void serialize(Header const& h, buffer & b);

std::error_code deserialize(buffer::const_iterator & i, buffer::const_iterator e, Header & h);

struct FindPeerRequestBody final
{
	id peer_to_find_id_;
};


inline std::ostream & operator<< (std::ostream & out, FindPeerRequestBody const& body)
{
	out << body.peer_to_find_id_;
	return out;
}

template<>
struct message_traits< FindPeerRequestBody >
{ static CXX11_CONSTEXPR Header::type TYPE_ID = Header::FIND_PEER_REQUEST; };


void serialize(FindPeerRequestBody const& body, buffer & b);


std::error_code deserialize(buffer::const_iterator & i, buffer::const_iterator e, FindPeerRequestBody & body);


struct FindPeerResponseBody final
{
	std::vector<Peer> peers_;
};

inline std::ostream & operator<< (std::ostream & out, FindPeerResponseBody const& body)
{
	for (auto& p : body.peers_)
	{
		out << '[' << p.endpoint_.toString() << "](" << p.id_ << ')' << std::endl;
	}
	return out;
}

template<>
struct message_traits< FindPeerResponseBody >
{ static CXX11_CONSTEXPR Header::type TYPE_ID = Header::FIND_PEER_RESPONSE; };

void serialize(FindPeerResponseBody const& body, buffer & b);

std::error_code deserialize(buffer::const_iterator & i, buffer::const_iterator e, FindPeerResponseBody & body);

struct FindValueRequestBody final
{
	id value_to_find_;
};

inline std::ostream & operator<< (std::ostream & out, FindValueRequestBody const& body)
{
	out << body.value_to_find_;
	return out;
}

template<>
struct message_traits< FindValueRequestBody >
{ static CXX11_CONSTEXPR Header::type TYPE_ID = Header::FIND_VALUE_REQUEST; };

void serialize(FindValueRequestBody const& body, buffer & b);

std::error_code deserialize(buffer::const_iterator & i, buffer::const_iterator e, FindValueRequestBody & body);

struct FindValueResponseBody final
{
	std::vector< std::uint8_t > data_;
};

inline std::ostream & operator<< (std::ostream & out, FindValueResponseBody const& body)
{
	for (auto& d : body.data_) out << (int)d;
	return out;
}

template<>
struct message_traits< FindValueResponseBody >
{ static CXX11_CONSTEXPR Header::type TYPE_ID = Header::FIND_VALUE_RESPONSE; };

void serialize(FindValueResponseBody const& body, buffer & b);

std::error_code deserialize(buffer::const_iterator & i, buffer::const_iterator e, FindValueResponseBody & body);

struct StoreValueRequestBody final
{
	id data_key_hash_;
	std::vector< std::uint8_t > data_value_;
};

inline std::ostream & operator<< (std::ostream & out, StoreValueRequestBody const& body)
{
	out << body.data_key_hash_;
	return out;
}

template<>
struct message_traits< StoreValueRequestBody >
{ static CXX11_CONSTEXPR Header::type TYPE_ID = Header::STORE_REQUEST; };

void serialize(StoreValueRequestBody const& body, buffer & b);

std::error_code deserialize(buffer::const_iterator & i, buffer::const_iterator e, StoreValueRequestBody & body);

} // namespace detail
} // namespace kademlia

#endif
