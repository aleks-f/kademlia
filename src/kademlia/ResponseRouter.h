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

#ifndef KADEMLIA_RESPONSE_ROUTER_H
#define KADEMLIA_RESPONSE_ROUTER_H


#include "Poco/Net/SocketProactor.h"
#include "kademlia/error.hpp"
#include "Poco/Net/SocketAddress.h"
#include "ResponseCallbacks.h"
#include "kademlia/Timer.h"
#include "kademlia/log.hpp"
#include "Poco/Mutex.h"


namespace kademlia {
namespace detail {


class ResponseRouter final
{
public:
	explicit ResponseRouter(Poco::Net::SocketProactor& io_service);

	ResponseRouter(ResponseRouter const&) = delete;

	ResponseRouter& operator = (ResponseRouter const&) = delete;

	void handle_new_response(Poco::Net::SocketAddress const& sender, Header const& h,
		buffer::const_iterator i, buffer::const_iterator e );

	template< typename OnResponseReceived, typename OnError >
	void register_temporary_callback(id const& response_id, Timer::duration const& callback_ttl
		, OnResponseReceived const& on_response_received, OnError const& on_error )
	{
		auto on_timeout = [ this, on_error, response_id ] ()
		{
			// If a callback is removed, that means
			// the message has never been received
			// hence report the timeout to the client.
			Poco::Mutex::ScopedLock l(_mutex);
			if (response_callbacks_.remove_callback( response_id ))
				on_error(make_error_code(std::errc::timed_out));
		};

		// Associate the response id with the
		// on_response_received callback.
		{
			Poco::Mutex::ScopedLock l(_mutex);
			response_callbacks_.push_callback(response_id, on_response_received);
		}
		timer_.expires_from_now(callback_ttl, on_timeout);
	}

private:
	ResponseCallbacks response_callbacks_;
	Timer timer_;
	Poco::Mutex _mutex;
};

} // namespace detail
} // namespace kademlia

#endif
