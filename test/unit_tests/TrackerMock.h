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

#ifndef KADEMLIA_TEST_HELPERS_TRACKER_MOCK_H
#define KADEMLIA_TEST_HELPERS_TRACKER_MOCK_H


#include "Poco/Net/SocketProactor.h"
#include "Poco/Net/SocketAddress.h"
#include "kademlia/error_impl.hpp"
#include "kademlia/Message.h"
#include "kademlia/MessageSerializer.h"
#include "kademlia/log.hpp"
#include <queue>

namespace kademlia {
namespace test {

class TrackerMock
{
public:
	TrackerMock(Poco::Net::SocketProactor& io_service): io_service_(io_service),
		id_(),
		message_serializer_(id_),
		responses_to_receive_(),
		sent_messages_()
	{
		LOG_DEBUG(TrackerMock, this) << "create TrackerMock." << std::endl;
	}

	template<typename MessageType>
	void add_message_to_receive(Poco::Net::SocketAddress const& endpoint,
		detail::id const& source_id, MessageType const& message)
	{
		message_to_receive m{ endpoint, detail::message_traits<MessageType>::TYPE_ID, source_id };
		serialize(message, m.body);
		responses_to_receive_.push(std::move(m));
	}

	template<typename MessageType>
	bool has_sent_message(Poco::Net::SocketAddress const& endpoint, MessageType const& message)
	{
		if (sent_messages_.empty()) return false;
		auto const c = sent_messages_.front();
		sent_messages_.pop();
		auto const m  = message_serializer_.serialize(message, detail::id{});
		return c.endpoint == endpoint && c.message == m;
	}

	bool has_sent_message()
	{
		return ! sent_messages_.empty();
	}

	template<typename RequestType, typename EndpointType, typename TimeoutType
			, typename OnMessageReceiveCallback, typename OnErrorCallback>
	void send_request(RequestType const& request
		, EndpointType const& endpoint
		, TimeoutType const& timeout
		, OnMessageReceiveCallback const& on_message_received
		, OnErrorCallback const& on_error)
	{
		save_sent_message(request, endpoint);

		if (responses_to_receive_.empty() || responses_to_receive_.front().endpoint != endpoint)
		{
			LOG_DEBUG(TrackerMock, this) << "add on_error." << std::endl;
			io_service_.addWork([on_error](){ on_error(detail::make_error_code(UNIMPLEMENTED)); }, 0);
		}
		else
		{
			auto const r = responses_to_receive_.front();
			responses_to_receive_.pop();
			detail::Header h{ detail::Header::V1, r.message_type, r.source_id };
			auto forwarder = [ on_message_received, h, r ]()
			{
				on_message_received(r.endpoint, h, r.body.begin(), r.body.end());
			};
			LOG_DEBUG(TrackerMock, this) << "add on_message_received." << std::endl;
			io_service_.addWork(std::move(forwarder), 0);
		}
	}

	template<typename RequestType, typename EndpointType>
	void send_request(RequestType const& r, EndpointType const& e)
	{
		save_sent_message(r, e);
	}

	template<typename ResponseType, typename EndpointType>
	void send_response(detail::id const&, ResponseType const& r, EndpointType const& e)
	{
		save_sent_message(r, e);
	}

	Poco::Net::SocketAddress addressV4()
	{
		return Poco::Net::SocketAddress();// network_.addressV4();
	}

	Poco::Net::SocketAddress addressV6()
	{
		return Poco::Net::SocketAddress();// network_.addressV6();
	}

private:
	struct sent_message final
	{
		Poco::Net::SocketAddress endpoint;
		detail::buffer message;
	};

	struct message_to_receive final
	{
		Poco::Net::SocketAddress endpoint;
		detail::Header::type message_type;
		detail::id source_id;
		detail::buffer body;
	};

private:
	template<typename RequestType, typename EndpointType>
	void save_sent_message(RequestType const& request, EndpointType const& endpoint)
	{
		sent_message m{ endpoint, message_serializer_.serialize(request, detail::id{}) };
		sent_messages_.push(m);
	}

	Poco::Net::SocketProactor& io_service_;
	detail::id id_;
	detail::MessageSerializer message_serializer_;
	std::queue<message_to_receive> responses_to_receive_;
	std::queue<sent_message> sent_messages_;
};

} // namespace test
} // namespace kademlia

#endif // KADEMLIA_TEST_HELPERS_TRACKER_MOCK_H
