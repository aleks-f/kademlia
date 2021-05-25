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

#include "Message.h"
#include <iostream>
#include "Poco/Net/IPAddress.h"
#include "kademlia/error_impl.hpp"


using Poco::Net::IPAddress;


namespace kademlia {
namespace detail {


namespace {


template<typename IntegerType>
inline void serialize_integer(IntegerType value, buffer& b)
{
	// Cast the integer as unsigned because
	// right shifting signed is UB.
	using unsigned_integer_type = typename std::make_unsigned<IntegerType>::type;

	for ( auto i = 0u; i < sizeof( value ); ++i )
	{
		b.push_back( buffer::value_type( value ) );
		static_cast< unsigned_integer_type & >( value ) >>= 8;
	}
}


template<typename IntegerType>
inline std::error_code deserialize_integer(buffer::const_iterator & i,
	buffer::const_iterator e, IntegerType & value )
{
	value = 0;

	if ( std::size_t( std::distance( i, e ) ) < sizeof( value ) )
		return make_error_code( TRUNCATED_SIZE );

	for ( auto j = 0u; j < sizeof( value ); ++j )
		value |= IntegerType{ *i++ } << 8 * j;

	return std::error_code{};
}

inline void serialize(std::vector< std::uint8_t > data, buffer & b )
{
	serialize_integer( data.size(), b );
	b.insert( b.end(), data.begin(), data.end() );
}


inline std::error_code deserialize(buffer::const_iterator & i
	, buffer::const_iterator e, std::vector< std::uint8_t > & data )
{
	std::uint64_t size;
	auto failure = deserialize_integer( i, e, size );
	if ( failure )
		return failure;

	if ( std::size_t( std::distance( i, e ) ) < size )
		return make_error_code( CORRUPTED_BODY );

	e = std::next( i, size );
	data.insert( data.end(), i, e );
	i = e;

	return std::error_code{};
}

inline void serialize(id const& i, buffer & b )
{
	b.insert( b.end(), i.begin(), i.end() );
}


inline std::error_code deserialize(buffer::const_iterator & i, buffer::const_iterator e, id & new_id)
{
	if ( std::size_t( std::distance( i, e ) ) < id::BLOCKS_COUNT )
		return make_error_code( TRUNCATED_ID );

	std::copy_n( i, id::BLOCKS_COUNT, new_id.begin() );
	std::advance( i, id::BLOCKS_COUNT );

	return std::error_code{};
}


std::error_code deserialize(buffer::const_iterator & i, buffer::const_iterator e,
	Header::version & v, Header::type & t )
{
	if ( std::distance( i, e ) < 1 )
		return make_error_code( TRUNCATED_HEADER );

	v = static_cast< Header::version >( *i & 0xf );
	t = static_cast< Header::type >( *i >> 4 );

	if ( v != Header::V1 )
		return make_error_code( UNKNOWN_PROTOCOL_VERSION );

	std::advance( i, 1 );
	return std::error_code{};
}

enum
{
	KADEMLIA_ENDPOINT_SERIALIZATION_IPV4 = 1,
	KADEMLIA_ENDPOINT_SERIALIZATION_IPV6 = 2
};


inline void serialize(IPAddress const& address, buffer & b)
{
	if (address.isV4())
	{
		b.push_back(KADEMLIA_ENDPOINT_SERIALIZATION_IPV4);
		IPAddress::RawIPv4 a = address.toV4Bytes();
		b.insert( b.end(), a.begin(), a.end() );
	}
	else
	{
		if (!address.isV6())
			throw Poco::InvalidArgumentException("Unknown IP version");

		b.push_back(KADEMLIA_ENDPOINT_SERIALIZATION_IPV6);
		IPAddress::RawIPv6 a = address.toV6Bytes();
		b.insert(b.end(), a.begin(), a.end());
	}
}

/*
template<typename Address>
inline std::error_code deserialize_address(buffer::const_iterator & i
	, buffer::const_iterator e, Address & address )
{
	typename Address::bytes_type buffer;
	if (std::size_t(std::distance(i, e )) < buffer.size())
		return make_error_code(TRUNCATED_ADDRESS);

	std::copy_n( i, buffer.size(), buffer.begin() );
	std::advance( i, buffer.size() );

	address = Address{ buffer };
	return std::error_code{};
}
*/

inline std::error_code
deserialize(buffer::const_iterator & i, buffer::const_iterator e, IPAddress& address )
{
/*
	TODO: probably not needed - unlike boost, Poco has one IP address
	if ( std::distance( i, e ) < 1 )
		return make_error_code( TRUNCATED_ENDPOINT );

	auto const protocol = *i++;
	if ( protocol == KADEMLIA_ENDPOINT_SERIALIZATION_IPV4 )
	{
		boost::asio::ip::address_v4 a;
		auto const failure = deserialize_address( i, e, a );
		if ( failure )
			return failure;

		address = a;
	}
	else
	{
		assert( protocol == KADEMLIA_ENDPOINT_SERIALIZATION_IPV6
			  && "unknown IP version");

		boost::asio::ip::address_v6 a;
		auto const failure = deserialize_address( i, e, a );
		if ( failure )
			return failure;

		address = a;
	}
*/
	return std::error_code{};
}


inline void
serialize(Peer const& n, buffer & b )
{
	serialize(n.id_, b);
	serialize_integer(n.endpoint_.port_, b);
	serialize(n.endpoint_.address_, b);
}


inline std::error_code
deserialize(buffer::const_iterator & i, buffer::const_iterator e, Peer & n )
{
	auto failure = deserialize(i, e, n.id_);
	if ( failure ) return failure;

	failure = deserialize_integer(i, e, n.endpoint_.port_);
	if ( failure ) return failure;

	return deserialize(i, e, n.endpoint_.address_);
}

} // anonymous namespace

std::ostream& operator << (std::ostream & out, Header::type const& t )
{
	switch (t)
	{
		default:
			throw std::runtime_error{ "unknown message type" };
		case Header::PING_REQUEST:
			return out << "ping_request";
		case Header::PING_RESPONSE:
			return out << "ping_response";
		case Header::STORE_REQUEST:
			return out << "store_request";
		case Header::FIND_PEER_REQUEST:
			return out << "find_peer_request";
		case Header::FIND_PEER_RESPONSE:
			return out << "find_peer_response";
		case Header::FIND_VALUE_REQUEST:
			return out << "find_value_request";
		case Header::FIND_VALUE_RESPONSE:
			return out << "find_value_response";
	}
}

std::ostream& operator << (std::ostream & out, Header const& h)
{
	return out << h.type_;
}

void serialize(Header const& h, buffer & b )
{
	b.push_back( h.version_ | h.type_ << 4 );
	serialize( h.source_id_, b );
	serialize( h.random_token_, b );
}

std::error_code deserialize(buffer::const_iterator & i, buffer::const_iterator e, Header & h )
{
	auto failure = deserialize( i, e, h.version_, h.type_ );
	if (failure) return failure;

	failure = deserialize( i, e, h.source_id_ );
	if (failure) return failure;

	return deserialize( i, e, h.random_token_ );
}

void serialize(FindPeerRequestBody const& body, buffer & b )
{
	serialize(body.peer_to_find_id_, b);
}

std::error_code deserialize(buffer::const_iterator & i, buffer::const_iterator e, FindPeerRequestBody & body )
{
	return deserialize( i, e, body.peer_to_find_id_ );
}

void serialize(FindPeerResponseBody const& body, buffer & b )
{
	serialize_integer( body.peers_.size(), b );

	for (auto const & n : body.peers_) serialize( n, b );
}


std::error_code deserialize(buffer::const_iterator & i, buffer::const_iterator e, FindPeerResponseBody & body)
{
	std::uint64_t size;
	auto failure = deserialize_integer( i, e, size );
	for (; size > 0 && !failure; -- size)
	{
		body.peers_.resize( body.peers_.size() + 1 );
		failure = deserialize( i, e, body.peers_.back() );
	}

	return failure;
}


void serialize(FindValueRequestBody const& body, buffer & b )
{
	serialize(body.value_to_find_, b);
}


std::error_code deserialize(buffer::const_iterator & i, buffer::const_iterator e, FindValueRequestBody & body)
{
	return deserialize( i, e, body.value_to_find_ );
}


void serialize(FindValueResponseBody const& body, buffer & b)
{
	serialize(body.data_, b);
}


std::error_code deserialize(buffer::const_iterator & i, buffer::const_iterator e, FindValueResponseBody & body)
{
	return deserialize(i, e, body.data_);
}

void serialize(StoreValueRequestBody const& body, buffer & b )
{
	serialize(body.data_key_hash_, b);
	serialize( body.data_value_, b );
}

std::error_code deserialize(buffer::const_iterator & i, buffer::const_iterator e, StoreValueRequestBody & body)
{
	auto failure = deserialize( i, e, body.data_key_hash_ );
	if (failure) return failure;
	return deserialize( i, e, body.data_value_ );
}

} // namespace detail
} // namespace kademlia
