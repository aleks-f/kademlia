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

#include <vector>
#include <deque>
#include <algorithm>
#include <memory>
#include <mutex>
#include "Poco/Error.h"
#include "Poco/NumberParser.h"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/SocketNotification.h"
#include "Poco/Net/NetException.h"
#include "kademlia/error_impl.hpp"
#include <kademlia/detail/cxx11_macros.hpp>
#include "kademlia/buffer.hpp"
#include "Message.h"
#include "kademlia/boost_to_std_error.hpp"
#include "IPEndpoint.h"

namespace kademlia {
namespace detail {


template<typename SocketType>
class MessageSocket final
{
public:
	/// Consider we won't receive IPv6 jumbo datagram.
	static CXX11_CONSTEXPR std::size_t INPUT_BUFFER_SIZE = UINT16_MAX;
	using endpoint_type = IPEndpoint;
	using resolved_endpoints = std::vector<endpoint_type>;
	using RecvCallbackType = std::function<void (endpoint_type const&, buffer::const_iterator, buffer::const_iterator)>;
	using underlying_endpoint_type = SocketType;
	using SendPacket = std::pair<Poco::Net::SocketAddress, buffer>;

	void setRecvCallbackType(RecvCallbackType onRecvCallback)
	{
		_onRecvCallback = onRecvCallback;
	}
 
	template<typename EndpointType>
	static resolved_endpoints resolve_endpoint(Poco::Net::SocketReactor& io_service, EndpointType const& e)
	{
	/*
		using protocol_type = typename SocketType::protocol_type;

		typename protocol_type::resolver r{ io_service };
		// Resolve addresses even if not reachable.
		typename protocol_type::resolver::query::flags const f{};
		typename protocol_type::resolver::query q{ e.address(), e.service(), f };
		// One raw endpoint (e.g. localhost) can be resolved to
		// multiple endpoints (e.g. IPv4 / IPv6 address).
		resolved_endpoints endpoints;
		auto i = r.resolve(q);
		for (decltype(i) end; i != end; ++i)
			// Convert from underlying_endpoint_type to endpoint_type.
			endpoints.push_back(convert_endpoint(*i));
		return endpoints;
	 */
	 // TODO: resolve using Poco::DNS
	 return resolved_endpoints{ toIPEndpoint(e.address(),
	 	static_cast<std::uint16_t>(Poco::NumberParser::parse(e.service()))) };
	}

	template<typename EndpointType>
	static MessageSocket ipv4(Poco::Net::SocketReactor& io_service, EndpointType const& e)
	{
		auto endpoints = resolve_endpoint(io_service, e);
		for (auto const& i : endpoints)
		{
			try
			{
				if (i.address_.isV4()) return MessageSocket{io_service, i};
			}
			catch (Poco::Net::NetException& ex)
			{
				throw std::runtime_error(ex.displayText());
			}
		}
		throw std::system_error{ make_error_code(INVALID_IPV4_ADDRESS) };
	}

	template<typename EndpointType>
	static MessageSocket ipv6(Poco::Net::SocketReactor& io_service, EndpointType const& e)
	{
		auto endpoints = resolve_endpoint(io_service, e);
		for (auto const& i : endpoints)
		{
			try
			{
				if (i.address_.isV6()) return MessageSocket{io_service, i};
			}
			catch (Poco::Net::NetException& ex)
			{
				throw std::runtime_error(ex.displayText());
			}
		}
		throw std::system_error{ make_error_code(INVALID_IPV6_ADDRESS) };
	}

	MessageSocket(MessageSocket&& o) = default;

	~MessageSocket()
	{
	}

	explicit MessageSocket(MessageSocket const& o) = delete;

	MessageSocket& operator = (MessageSocket const& o) = delete;
/*
	template<typename ReceiveCallback>
	void async_receive(ReceiveCallback const& callback)
	{
		auto on_completion = [ this, callback ]
			(boost::system::error_code const& failure
			, std::size_t bytes_received)
		{
#ifdef _MSC_VER
			// On Windows, an UDP socket may return connection_reset
			// to inform application that a previous send by this socket
			// has generated an ICMP port unreachable.
			// https://msdn.microsoft.com/en-us/library/ms740120.aspx
			// Ignore it and schedule another read.
			if (failure == boost::system::errc::connection_reset)
				return async_receive(callback);
#endif
			auto i = reception_buffer_.begin(), e = i;
			if (!failure) std::advance(e, bytes_received);
			callback(boost_to_std_error(failure), convert_endpoint(current_message_sender_), i, e);
		};
		assert(reception_buffer_.size() == INPUT_BUFFER_SIZE);
		_socket.async_receive_from(boost::asio::buffer(reception_buffer_), current_message_sender_, std::move(on_completion));
	}
*/
	template<typename SendCallback>
	void async_send(buffer const& message, endpoint_type const& to, SendCallback const& callback)
	{
		if (message.size() > INPUT_BUFFER_SIZE)
			callback(make_error_code(std::errc::value_too_large));
		else
		{
			// TODO: make buffer rvalue so it can be moved
			std::unique_lock<std::mutex> l(*_pMutex);
			_messageQueue.push_back(std::make_pair(Poco::Net::SocketAddress(to.address_, to.port_), message));
#if 0
			// Copy the buffer as it has to live past the end of this call.
			auto message_copy = std::make_shared<buffer>(message);
			auto on_completion = [ this, callback, message_copy ]
				(boost::system::error_code const& failure, std::size_t /* bytes_sent */)
			{
				callback(boost_to_std_error(failure));
			};

			_socket.async_send_to(boost::asio::buffer(*message_copy), convert_endpoint(to), std::move(on_completion));
#endif
		}
	}

	endpoint_type local_endpoint() const
	{
		return endpoint_type{ _socket.address().host(), _socket.address().port() };
	}

	void onReadable(Poco::Net::ReadableNotification* pNf)
	{
		pNf->release();
		char buffer[INPUT_BUFFER_SIZE];
		auto i = reception_buffer_.begin(), e = i;
		int n = _socket.receiveFrom(buffer, sizeof(buffer), current_message_sender_);
		if (n > 0) std::advance(e, n);
		//TODO: add onError handler
		//std::error_code err = std::make_error_code(std::errc(Poco::Error::last()));
		_onRecvCallback(convert_endpoint(current_message_sender_), i, e);
	}

	void onWritable(Poco::Net::WritableNotification* pNf)
	{
		pNf->release();
		std::unique_lock<std::mutex> l(*_pMutex);
		if (_messageQueue.size())
		{
			SendPacket &sp = _messageQueue.front();
			_socket.sendTo(&sp.second[0], sp.second.size(), sp.first);
			_messageQueue.pop_front();
		}
	}

private:
	MessageSocket(Poco::Net::SocketReactor& io_service, endpoint_type const& e):
			reception_buffer_(INPUT_BUFFER_SIZE),
			current_message_sender_(),
			_readHandler(*this, &MessageSocket<SocketType>::onReadable),
			_writeHandler(*this, &MessageSocket<SocketType>::onWritable),
			_socket(convert_endpoint(e)),
			_pMutex(new std::mutex())
	{
		io_service.addEventHandler(_socket, _readHandler);
		io_service.addEventHandler(_socket, _writeHandler);
	}

	static SocketType create_underlying_socket(Poco::Net::SocketReactor& io_service, endpoint_type const& endpoint)
	{

		auto const e = convert_endpoint(endpoint);
		SocketType new_socket(e);
		return new_socket;
	}

	static endpoint_type convert_endpoint(underlying_endpoint_type const& e)
	{
		return endpoint_type{ e.address().host(), e.address().port() };
	}

	static underlying_endpoint_type convert_endpoint(endpoint_type const& e)
	{
		return underlying_endpoint_type(Poco::Net::SocketAddress(e.address_, e.port_), false, true);
	}

	buffer reception_buffer_;
	Poco::Net::SocketAddress current_message_sender_;
	Poco::Observer<MessageSocket, Poco::Net::ReadableNotification> _readHandler;
	Poco::Observer<MessageSocket, Poco::Net::WritableNotification> _writeHandler;
	SocketType _socket;
	RecvCallbackType _onRecvCallback;
	std::unique_ptr<std::mutex> _pMutex = nullptr;
	std::deque<SendPacket> _messageQueue;
};


} // namespace detail
} // namespace kademlia

#endif

