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

#include <memory>
#include "Poco/Net/SocketProactor.h"
#include "kademlia/Session.h"
#include <kademlia/endpoint.hpp>
#include "kademlia/log.hpp"
#include "kademlia/buffer.hpp"
#include "kademlia/Engine.h"
#include "FakeSocket.h"

namespace kademlia {
namespace test {

using Session = Kademlia::Session;

class TestEngine final
{
public:
	TestEngine(Poco::Net::SocketProactor& service,endpoint const & ipv4,endpoint const & ipv6,
		detail::id const& new_id): engine_(service, ipv4, ipv6, new_id),
			listen_ipv4_(FakeSocket::get_last_allocated_ipv4(), Session::DEFAULT_PORT),
			listen_ipv6_(FakeSocket::get_last_allocated_ipv6(), Session::DEFAULT_PORT)
	{ }

	TestEngine(Poco::Net::SocketProactor& service, endpoint const & initial_peer
		, endpoint const & ipv4, endpoint const & ipv6, detail::id const& new_id)
			: engine_(service, initial_peer, ipv4, ipv6, new_id),
			listen_ipv4_(FakeSocket::get_last_allocated_ipv4(), Session::DEFAULT_PORT),
			listen_ipv6_(FakeSocket::get_last_allocated_ipv6(), Session::DEFAULT_PORT)
	{ }

	template< typename Callable >
	void asyncSave(std::string const& key, std::string&& data, Callable & callable)
	{
		impl::key_type const k{ key.begin(), key.end() };
		engine_.asyncSave(k, impl::data_type{data.begin(), data.end()}, callable);
	}

	template< typename Callable >
	void asyncLoad(std::string const& key, Callable & callable)
	{
		impl::key_type const k{ key.begin(), key.end() };
		auto c = [ callable ](std::error_code const& failure, impl::data_type const& data)
		{
			callable(failure, std::string{ data.begin(), data.end() });
		};

		engine_.asyncLoad(k, c);
	}

	endpoint
	ipv4() const
	{
		return endpoint(listen_ipv4_.host().toString(), listen_ipv4_.port());
	}

	endpoint
	ipv6() const
	{
		return endpoint(listen_ipv6_.host().toString(), listen_ipv6_.port());
	}

private:
	using impl = detail::Engine<FakeSocket>;

private:
	impl engine_;
	FakeSocket::endpoint_type listen_ipv4_;
	FakeSocket::endpoint_type listen_ipv6_;
};

class packet final
{
public:
	packet(endpoint const& from, endpoint const& to, detail::Header::type const& type): from_(from),
		to_(to), type_(type)
	{
	}

	endpoint const& from() const
	{
		return from_;
	}

	endpoint const& to() const
	{
		return to_;
	}

	detail::Header::type const& type() const
	{
		return type_;
	}

private:
	endpoint from_;
	endpoint to_;
	detail::Header::type type_;
};

inline detail::Header extract_kademlia_header(FakeSocket::packet const& p)
{
	detail::Header h;
	auto i = p.data_.begin(), e = p.data_.end();
	deserialize(i, e, h);
	return h;
}

inline packet pop_packet()
{
	auto & packets = FakeSocket::get_logged_packets();

	if (packets.empty())
		throw std::runtime_error{ "no packet left" };

	auto const& p = packets.front();

	packet const r{endpoint{ p.from_.host().toString(), std::to_string(p.from_.port()) },
					endpoint{ p.to_.host().toString(), std::to_string(p.to_.port()) },
					extract_kademlia_header(p).type_};
	packets.pop();
	return r;
}

inline std::size_t count_packets()
{
	return FakeSocket::get_logged_packets().size();
}

inline void clear_packets()
{ 
	while (count_packets() > 0ULL)
		pop_packet();
}

inline void forget_attributed_ip()
{
	FakeSocket::get_last_allocated_ipv4() = FakeSocket::get_first_ipv4();
	FakeSocket::get_last_allocated_ipv6() = FakeSocket::get_first_ipv6();
}

} // namespace test
} // namespace kademlia
