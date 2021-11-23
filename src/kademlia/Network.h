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

#ifndef KADEMLIA_NETWORK_H
#define KADEMLIA_NETWORK_H

#ifdef _MSC_VER
#   pragma once
#endif

#include <functional>
#include "Poco/Net/SocketProactor.h"
#include "Poco/Net/SocketAddress.h"
#include "kademlia/log.hpp"
#include "MessageSocket.h"
#include "kademlia/buffer.hpp"

namespace kademlia {
namespace detail {


template<typename MessageSocketType>
class Network final
{
public:
	using SocketAddressList = std::vector<Poco::Net::SocketAddress>;
	using SocketAddress = Poco::Net::SocketAddress;
	using on_message_received_type = std::function<void (Poco::Net::SocketAddress const&, buffer::const_iterator, buffer::const_iterator)>;

	Network(Poco::Net::SocketProactor& io_service,
		MessageSocketType&& socket_ipv4,
		MessageSocketType&& socket_ipv6,
		on_message_received_type on_message_received): io_service_(io_service),
			socket_ipv4_(std::move(socket_ipv4)),
			socket_ipv6_(std::move(socket_ipv6)),
			on_message_received_(on_message_received)
	{
		start_message_reception(on_message_received);
		LOG_DEBUG(Network, this) << "Network created at '" << socket_ipv4_.local_endpoint().toString()
			<< "' and '" << socket_ipv6_.local_endpoint().toString() << "'." << std::endl;
	}

	Network(Network const&) = delete;

	Network& operator = (Network const&) = delete;

	template<typename Message, typename OnMessageSent>
	void send(Message&& message, const Poco::Net::SocketAddress& e, OnMessageSent const& on_message_sent)
	{
		get_socket_for(e).async_send(std::move(message), e, on_message_sent);
	}

	template<typename Endpoint>
	SocketAddressList resolve_endpoint(Endpoint const& e)
	{
		return MessageSocketType::resolve_endpoint(e);
	}

private:
	void start_message_reception(on_message_received_type on_message_received)
	{
		schedule_receive_on_socket(socket_ipv4_);
		schedule_receive_on_socket(socket_ipv6_);
	}

	MessageSocketType& get_socket_for(Poco::Net::SocketAddress const& e)
	{
		if (e.host().isV4()) return socket_ipv4_;
		return socket_ipv6_;
	}

private:
	void schedule_receive_on_socket(MessageSocketType & current_subnet)
	{
		auto on_new_message = [ this, &current_subnet ]
			( std::error_code const& failure
			, Poco::Net::SocketAddress const& sender
			, buffer::const_iterator i
			, buffer::const_iterator e )
		{
			// Reception failure are fatal.
			if ( failure )
			{
				std::cerr << "failure=" << failure.message() << std::endl;
				throw std::system_error{failure};
			}

			on_message_received_(sender, i, e);
			schedule_receive_on_socket(current_subnet);
		};

		current_subnet.async_receive( on_new_message );
	}

	Poco::Net::SocketProactor& io_service_;
	MessageSocketType socket_ipv4_;
	MessageSocketType socket_ipv6_;
	on_message_received_type on_message_received_;
};

} // namespace detail
} // namespace kademlia

#endif
