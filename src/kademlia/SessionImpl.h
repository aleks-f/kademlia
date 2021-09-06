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

#ifndef KADEMLIA_SESSION_IMPL_H
#define KADEMLIA_SESSION_IMPL_H

#ifdef _MSC_VER
#   pragma once
#endif


#include <utility>
#include "SocketAdapter.h"
#include "Poco/Mutex.h"
#include "Poco/Net/DatagramSocket.h"
#include "Poco/Net/SocketProactor.h"
#include "MessageSocket.h"
#include "kademlia/endpoint.hpp"
#include "Engine.h"


namespace kademlia {
namespace detail {


class SessionImpl
{
public:
	using DataType = std::vector<std::uint8_t>;
	using KeyType = std::vector<std::uint8_t>;
	using SocketType = SocketAdapter<Poco::Net::DatagramSocket>;
	using EngineType = detail::Engine<SocketType>;

public:
	SessionImpl(endpoint const& listen_on_ipv4, endpoint const& listen_on_ipv6, int ms = 300);

	SessionImpl(endpoint const& initial_peer, endpoint const& listen_on_ipv4, endpoint const& listen_on_ipv6, int ms = 300);

	template<typename HandlerType>
	void async_save(KeyType const& key, DataType const& data, HandlerType && handler)
	{
		_engine.async_save(key, data, std::forward<HandlerType>(handler));
	}

	template<typename HandlerType>
	void async_load(KeyType const& key, HandlerType && handler)
	{
		_engine.async_load(key, std::forward<HandlerType>(handler));
	}

	std::error_code run();

	void abort();

private:
	Poco::Net::SocketProactor _ioService;
	EngineType _engine;
	Poco::FastMutex _mutex;
};

} // namespace detail
} // namespace kademlia

#endif

