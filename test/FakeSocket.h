// Copyright (c) 2014, David Keller
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

#ifndef KADEMLIA_FAKE_SOCKET_H
#define KADEMLIA_FAKE_SOCKET_H

#ifdef _MSC_VER
#   pragma once
#endif

#include <functional>
#include <cstdlib>
#include <deque>
#include <queue>
#include <vector>
#include <cstdint>

#include <boost/asio/buffer.hpp>
#include <boost/system/error_code.hpp>
#include "Poco/Net/IPAddress.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/SocketReactor.h"

#include "kademlia/log.hpp"
#include "kademlia/error_impl.hpp"
#include "kademlia/Message.h"
#include "kademlia/buffer.hpp"

namespace kademlia {
namespace test {


class FakeSocket
{
public:
	using protocol_type = Poco::Net::SocketAddress::Family;
	using endpoint_type = Poco::Net::SocketAddress;

	enum { FIXED_PORT = 27980 };

	struct packet final
	{
		endpoint_type from_;
		endpoint_type to_;
		detail::buffer data_;
	};

	using packets = std::queue<packet>;

public:
	FakeSocket(Poco::Net::SocketReactor* io_service,
		const Poco::Net::SocketAddress& address, bool reuseAddress = true, bool ipV6Only = true):
		io_service_(io_service), local_endpoint_(), pending_reads_()
	{
		//kademlia::detail::enable_log_for("FakeSocket");
		bind(address);
	}

	FakeSocket(Poco::Net::SocketReactor* io_service):
			io_service_(io_service), local_endpoint_(), pending_reads_()
	{
		//kademlia::detail::enable_log_for("FakeSocket");
	}

	FakeSocket(const FakeSocket& o)=delete;

	FakeSocket(FakeSocket && o):
		io_service_(o.io_service_),
		local_endpoint_(std::move(o.local_endpoint_)),
		pending_reads_(std::move(o.pending_reads_)),
		pending_writes_(std::move(o.pending_writes_))
	{
		//kademlia::detail::enable_log_for("FakeSocket");
		add_route_to_socket(local_endpoint(), this);
	}

	FakeSocket& operator = (FakeSocket&& o)
	{
		io_service_ = o.io_service_;
		local_endpoint_ = std::move(o.local_endpoint_);
		pending_reads_ = std::move(o.pending_reads_);
		pending_writes_ = std::move(o.pending_writes_);
		add_route_to_socket(local_endpoint(), this);
		return *this;
	}

	FakeSocket& operator = (FakeSocket const& o) = delete;

	~FakeSocket()
	{
		boost::system::error_code ignored;
		close(ignored);
	}

	const endpoint_type& address() const
	{
		return local_endpoint_;
	}

	template<typename Option>
	void set_option(Option const&)
	{
	}

	const endpoint_type& local_endpoint() const
	{
		return local_endpoint_;
	}

	boost::system::error_code bind(endpoint_type const& e)
	{
		// Only fixed port is handled right now.
		if (e.port() != FIXED_PORT)
			return make_error_code(boost::system::errc::invalid_argument);

		// Generate our local address.
		if (e.host().isV4())
			local_endpoint(generate_unique_ipv4_endpoint(e.port()));
		else
			local_endpoint(generate_unique_ipv6_endpoint(e.port()));

		add_route_to_socket(local_endpoint(), this);
		return boost::system::error_code();
	}

	boost::system::error_code close(boost::system::error_code& failure)
	{
		try
		{
			// This socket no longer reads messages.
			if (get_socket(local_endpoint_) == this)
			{
				add_route_to_socket(local_endpoint(), nullptr);
				failure.clear();
			}
			else
				failure = make_error_code(boost::system::errc::not_connected);

			pending_reads_.clear();
			pending_writes_.clear();
		}
		catch(Poco::NullPointerException&)
		{
			failure = make_error_code(boost::system::errc::not_connected);
		}
		return failure;
	}

	template<typename Callback>
	void asyncReceiveFrom(boost::asio::mutable_buffer const& buffer, endpoint_type & from, Callback && callback)
	{
		// Check if there is packets waiting.
		if (pending_writes_.empty())
		{
			LOG_DEBUG(FakeSocket, this) << "saving pending read from " << from.toString() << std::endl;

			// No packet are waiting, hence register that
			// the current socket is waiting for packet.
			pending_reads_.push_back({ buffer, from, std::forward<Callback>(callback) });
		}
		else
		{
			LOG_DEBUG(FakeSocket, this) << "execute read." << std::endl;

			// A packet is waiting to be read, so read it asynchronously.
			async_execute_read(buffer, from, std::forward<Callback>(callback));
		}
	}

	template<typename Callback>
	void asyncSendTo(boost::asio::const_buffer const& buffer, endpoint_type const& to, Callback && callback)
	{
		LOG_DEBUG(FakeSocket, this) << "asyncSendTo()" << std::endl;
		// Ensure the destination socket is listening.
		auto target = get_socket(to);
		if (! target)
		{
			LOG_DEBUG(FakeSocket, this) << "network unreachable." << std::endl;
			callback(make_error_code(boost::system::errc::network_unreachable), 0ULL);
		}
		// Check if it's not waiting for any packet.
		else if (target->pending_reads_.empty())
		{
			LOG_DEBUG(FakeSocket, this) << "saving pending write." << std::endl;

			target->pending_writes_.push_back({ buffer, local_endpoint_, std::forward<Callback>(callback) });
		}
		else
		{
			LOG_DEBUG(FakeSocket, this) << "execute write." << std::endl;

			// It's already waiting for the current packet.
			async_execute_write(target, buffer, std::forward<Callback>(callback));
		}

		log_packet(buffer, to);
	}

	static packets& get_logged_packets()
	{ 
		static packets logged_packets_;
		return logged_packets_;
	}

	static Poco::Net::IPAddress get_first_ipv4()
	{
		return Poco::Net::IPAddress("10.0.0.0");
	}

	static Poco::Net::IPAddress& get_last_allocated_ipv4()
	{
		static Poco::Net::IPAddress ipv4_ = get_first_ipv4();
		return ipv4_;
	}

	static Poco::Net::IPAddress get_first_ipv6()
	{
		return Poco::Net::IPAddress("fc00::");
	}

	static Poco::Net::IPAddress&get_last_allocated_ipv6()
	{
		static Poco::Net::IPAddress ipv6_ = get_first_ipv6();
		return ipv6_;
	}

private:
	using callback_type = std::function<void (boost::system::error_code const&, std::size_t)>;

	struct pending_read
	{
		boost::asio::mutable_buffer buffer_;
		endpoint_type & source_;
		callback_type callback_;
	};

	struct pending_write
	{
		boost::asio::const_buffer buffer_;
		endpoint_type const& source_;
		callback_type callback_;
	};

	class router
	{
	public:
		using socket_ptr = FakeSocket*;

	public:
		socket_ptr &
		operator [] (endpoint_type const& e)
		{
			return get_socket_from(ipv4_sockets_, e.host());
		}

	private:
		using sockets_pool = std::vector<socket_ptr>;

	private:
		template<typename IpAddress>
		static std::size_t address_as_index(IpAddress const& address)
		{
			std::size_t index = 0ULL;
			auto const bytes = address.toBytes();
			// Skip the first byte as it's used to
			// mark the address as private (e.g. 10.x.x.x or fc00::).
			for (std::size_t i = 1, e = bytes.size(); i != e; ++ i)
			{
				poco_assert(index>> 56 == 0 /* no bytes are lost by next shift */);
				index <<= 8;
				index |= bytes[ i ];
			}
			return index;
		}

		template<typename IpAddress>
		socket_ptr& get_socket_from(sockets_pool & sockets, IpAddress const& address)
		{
			auto const index = address_as_index(address);
			if (index>= sockets.size()) sockets.resize(index + 1);
			return sockets[ index ];
		}

	private:
		sockets_pool ipv4_sockets_;
		sockets_pool ipv6_sockets_;
	};

private:
	void log_packet(boost::asio::const_buffer const& buffer, endpoint_type const & to)
	{
		auto i = boost::asio::buffer_cast<uint8_t const *>(buffer);
		auto e = i + boost::asio::buffer_size(buffer);
		packet p{ local_endpoint_, to, { i, e } };
		get_logged_packets().push(std::move(p));
	}

	void local_endpoint(endpoint_type const& e)
	{
		local_endpoint_ = e;
	}

	template<typename IpAddress>
	static IpAddress& increment_address(IpAddress & address)
	{
		auto bytes = address.toBytes();

		// While current byte overflows on increment, increment next byte.
		auto i = bytes.rbegin(), e = bytes.rend();
		for (; i != e && (++ *i) == 0; ++i) continue;
		poco_assert(i != e /* all ip address have been allocated */);
		address = IpAddress(&bytes[0], bytes.size());
		return address;
	}

	/**
	 *  @note This function is not thread safe.
	 */
	static endpoint_type generate_unique_ipv4_endpoint(std::uint16_t const& port)
	{
		auto & last_allocated_ipv4 = get_last_allocated_ipv4();
		increment_address(last_allocated_ipv4);
		return endpoint_type(last_allocated_ipv4, port);
	}

	/**
	 *  @note This function is not thread safe.
	 */
	static endpoint_type generate_unique_ipv6_endpoint(std::uint16_t const& port)
	{
		auto & last_allocated_ipv6 = get_last_allocated_ipv6();
		increment_address(last_allocated_ipv6);

		return endpoint_type(last_allocated_ipv6, port);
	}

	static router& get_router()
	{
		static router router_;
		return router_;
	}

	static void add_route_to_socket(endpoint_type const& e, FakeSocket * s)
	{
		get_router()[ e ] = s;
	}

	static FakeSocket* get_socket(endpoint_type const& e)
	{
		return get_router()[ e ];
	}

	static std::size_t copy_buffer(boost::asio::const_buffer const& from
		, boost::asio::mutable_buffer const& to)
	{
		auto const source_size = boost::asio::buffer_size(from);
		poco_assert(source_size <= boost::asio::buffer_size(to)
			  && "can't store message into target buffer");

		auto source_data = boost::asio::buffer_cast<uint8_t const *>(from);
		auto target_data = boost::asio::buffer_cast<uint8_t *>(to);

		std::memcpy(target_data, source_data, source_size);

		return source_size;
	}

	template<typename Callback>
	void async_execute_write(FakeSocket * target
		, boost::asio::const_buffer const& buffer
		, Callback && callback)
	{
		auto perform_write = [ this, target, buffer, callback ] ()
		{
			// Retrieve the read task of the packet.
			poco_assert(! target->pending_reads_.empty());
			pending_read & p = target->pending_reads_.front();

			// Fill the read task buffer and endpoint.
			auto const copied_bytes_count = copy_buffer(buffer, p.buffer_);
			p.source_ = local_endpoint_;

			// Inform the writer that data has been writeen.
			callback(boost::system::error_code(), copied_bytes_count);

			// Inform the reader that data has been read.
			p.callback_(boost::system::error_code(), copied_bytes_count);

			target->pending_reads_.pop_front();
		};

		io_service_->addCompletionHandler(std::move(perform_write), 0);
	}

	template<typename Callback>
	void async_execute_read(boost::asio::mutable_buffer const& buffer, endpoint_type & from, Callback && callback)
	{
		auto perform_read = [ this, buffer, &from, callback ] ()
		{
			// Retrieve the write task of the packet.
			poco_assert(! pending_writes_.empty());
			pending_write & w = pending_writes_.front();

			// Fill the provided buffer and endpoint.
			from = w.source_;
			auto const copied_bytes_count = copy_buffer(w.buffer_, buffer);

			// Now inform the writeer that data has been sent.
			w.callback_(boost::system::error_code(), copied_bytes_count);

			// Inform the reader that data has been read.
			callback(boost::system::error_code(), copied_bytes_count);

			// The current task has been consumed.
			pending_writes_.pop_front();
		};

		io_service_->addCompletionHandler(std::move(perform_read), 0);
	}

private:
	Poco::Net::SocketReactor* io_service_;
	endpoint_type local_endpoint_;
	std::deque<pending_read> pending_reads_;
	std::deque<pending_write> pending_writes_;
};


} // namespace test
} // namespace kademlia

#endif
