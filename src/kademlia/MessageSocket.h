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

#ifndef KADEMLIA_MESSAGE_SOCKET_H
#define KADEMLIA_MESSAGE_SOCKET_H

#ifdef _MSC_VER
#   pragma once
#endif

#include <assert.h>
#include <vector>
#include <deque>
#include <algorithm>
#include <memory>
#include <mutex>
#include <type_traits>
#include "Poco/Error.h"
#include "Poco/NumberParser.h"
#include "Poco/Net/SocketProactor.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/SocketNotification.h"
#include "SocketAdapter.h"
#include "Poco/Net/NetException.h"
#include "kademlia/error_impl.hpp"
#include <kademlia/detail/cxx11_macros.hpp>
#include "kademlia/buffer.hpp"
#include "Message.h"
#include "IPEndpoint.h"
#include "kademlia/log.hpp"
#include "Poco/Net/DNS.h"
#include "Poco/Net/HostEntry.h"
#include "Poco/Net/IPAddress.h"

namespace kademlia {
namespace detail {


template<typename SocketType>
class MessageSocket final
{
public:
	using endpoint_type = IPEndpoint;
	using resolved_endpoints = std::vector<endpoint_type>;
	using underlying_socket_type = SocketType;
	using underlying_endpoint_type = Poco::Net::SocketAddress;
	using SendPacket = std::pair<Poco::Net::SocketAddress, buffer>;
 
	template<typename EndpointType>
	static resolved_endpoints resolve_endpoint(EndpointType const& e)
	{
		using Poco::Net::IPAddress;
		resolved_endpoints re;
		try
		{
			IPAddress addr;
			if (IPAddress::tryParse(e.address(), addr))
			{
				re.emplace_back(toIPEndpoint(addr.toString(),
					static_cast<std::uint16_t>(Poco::NumberParser::parse(e.service()))));
			}
			else
			{
				Poco::Net::HostEntry he = Poco::Net::DNS::resolve(e.address());

				re.reserve(he.addresses().size());
				for (const auto& addr : he.addresses())
				{
					re.emplace_back(toIPEndpoint(addr.toString(),
						static_cast<std::uint16_t>(Poco::NumberParser::parse(e.service()))));
				}
			}
		}
		catch (Poco::Net::HostNotFoundException&)
		{
			re.emplace_back(toIPEndpoint(e.address(),
				static_cast<std::uint16_t>(Poco::NumberParser::parse(e.service()))));
		}
		return re;
	}

	template<typename EndpointType>
	static MessageSocket ipv4(Poco::Net::SocketProactor& io_service, EndpointType const& e)
	{
		std::cout << "Resolving " << e.address() << ':' << e.service() << " ..." << std::endl;
		auto endpoints = resolve_endpoint(e);
		for (auto const& i : endpoints)
		{
			try
			{
				if (i.address_.isV4())
				{
					std::cout << "Resolved: " << i << std::endl;
					return MessageSocket(io_service, i);
				}
			}
			catch (Poco::Net::NetException& ex)
			{
				throw std::runtime_error(ex.displayText());
			}
		}
		throw std::system_error{ make_error_code(INVALID_IPV4_ADDRESS) };
	}

	template<typename EndpointType>
	static MessageSocket ipv6(Poco::Net::SocketProactor& io_service, EndpointType const& e)
	{
		auto endpoints = resolve_endpoint(e);
		for (auto const& i : endpoints)
		{
			try
			{
				if (i.address_.isV6())
					return MessageSocket(io_service, i);
			}
			catch (Poco::Net::NetException& ex)
			{
				throw std::runtime_error(ex.displayText());
			}
		}
		throw std::system_error{ make_error_code(INVALID_IPV6_ADDRESS) };
	}

	MessageSocket(MessageSocket&& o): reception_buffer_(std::move(o.reception_buffer_)),
		current_message_sender_(std::move(o.current_message_sender_)),
		_socket(std::move(o._socket)),
		_messageQueue(std::move(o._messageQueue)),
		_ioService(o._ioService),
		_pMutex(std::move(o._pMutex))
	{
	}

	MessageSocket& operator = (MessageSocket&& o) = delete;

	~MessageSocket()
	{
	}

	explicit MessageSocket(MessageSocket const& o) = delete;
	MessageSocket& operator = (MessageSocket const& o) = delete;

	template<typename ReceiveCallback>
	void async_receive(ReceiveCallback const& callback)
	{
		auto on_completion = [ this, callback ]
			(std::error_code const& failure
			, std::size_t bytes_received)
		{
#ifdef _MSC_VER
			// On Windows, an UDP socket may return connection_reset
			// to inform application that a previous send by this socket
			// has generated an ICMP port unreachable.
			// https://msdn.microsoft.com/en-us/library/ms740120.aspx
			// Ignore it and schedule another read.
			if (failure == std::errc::connection_reset)
				return async_receive(callback);
#endif
			auto i = reception_buffer_.begin(), e = i;
			if (!failure) std::advance(e, bytes_received);
			callback(failure, convert_endpoint(current_message_sender_), i, e);
		};

		_socket.asyncReceiveFrom(reception_buffer_, current_message_sender_, std::move(on_completion));
	}

	template<typename SendCallback>
	void async_send(buffer&& message, const endpoint_type& to, SendCallback const& callback)
	{
		using Poco::Net::SocketAddress;
		std::unique_lock<std::mutex> l(*_pMutex);
		_messageQueue.push_back(std::make_pair(Poco::Net::SocketAddress(to.address_, to.port_), message));

		auto on_completion = [callback]
			(std::error_code const& failure, std::size_t /* bytes_sent */)
		{
			callback(failure);
		};
		_socket.asyncSendTo(std::move(message), convert_endpoint(to), std::move(on_completion));
	}

	endpoint_type local_endpoint() const
	{
		return { _socket.address().host(), _socket.address().port() };
	}

private:
	MessageSocket(Poco::Net::SocketProactor& io_service, endpoint_type const& e):
			current_message_sender_(),
			_socket(&io_service, convert_endpoint(e), false, true),
			_ioService(io_service),
			_pMutex(new std::mutex())
	{
		kademlia::detail::enable_log_for("MessageSocket");
		LOG_DEBUG(MessageSocket, this) << "MessageSocket created for " << _socket.address().toString() << std::endl;
	}

	static underlying_socket_type create_underlying_socket(Poco::Net::SocketProactor& io_service, endpoint_type const& endpoint)
	{

		auto const e = convert_endpoint(endpoint);
		underlying_socket_type new_socket(e.family());
		new_socket.bind();
		return new_socket;
	}

	static endpoint_type convert_endpoint(underlying_endpoint_type const& e)
	{
		return endpoint_type{ e.host(), e.port() };
	}

	static underlying_endpoint_type convert_endpoint(endpoint_type const& e)
	{
		return underlying_endpoint_type(e.address_, e.port_);
	}

	buffer reception_buffer_;
	Poco::Net::SocketAddress current_message_sender_;
	underlying_socket_type _socket;
	std::deque<SendPacket> _messageQueue;
	Poco::Net::SocketProactor& _ioService;
	std::unique_ptr<std::mutex> _pMutex = nullptr;
};


} // namespace detail
} // namespace kademlia

#endif

