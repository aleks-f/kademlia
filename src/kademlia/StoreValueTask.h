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

#ifndef KADEMLIA_STORE_VALUE_TASK_H
#define KADEMLIA_STORE_VALUE_TASK_H

#ifdef _MSC_VER
#   pragma once
#endif

#include "Poco/Net/SocketAddress.h"
#include <memory>
#include <type_traits>
#include <system_error>

#include "LookupTask.h"
#include "kademlia/log.hpp"
#include "Message.h"
#include "kademlia/constants.hpp"

namespace kademlia {
namespace detail {

///
template< typename SaveHandlerType, typename TrackerType, typename DataType >
class StoreValueTask final : public LookupTask
{
public:
	template< typename RoutingTableType >
	static void
	start(detail::id const & key, DataType&& data, TrackerType & tracker
		, RoutingTableType & routing_table, SaveHandlerType handler)
	{
		std::shared_ptr< StoreValueTask > c;
		c.reset(new StoreValueTask(key, std::move(data), tracker, routing_table, std::move(handler)));
		try_to_store_value(c);
	}

private:
	template< typename RoutingTableType, typename HandlerType >
	StoreValueTask(detail::id const & key, DataType&& data, TrackerType & tracker
		, RoutingTableType & routing_table, HandlerType && save_handler):
			LookupTask(key, routing_table.find(key), routing_table.end())
			, tracker_(tracker)
			, data_(std::move(data))
			, save_handler_(std::forward< HandlerType >(save_handler))
	{
		LOG_DEBUG(StoreValueTask, this)
				<< "create store value task for '"
				<< key << "' value(" << toString(data)
				<< ")." << std::endl;
	}

	void notify_caller(std::error_code const& failure)
	{
		save_handler_(failure);
	}

	DataType const& get_data() const
	{
		return data_;
	}

	static void try_to_store_value(std::shared_ptr< StoreValueTask > task,
		std::size_t concurrent_requests_count = CONCURRENT_FIND_PEER_REQUESTS_COUNT)
	{
		LOG_DEBUG(StoreValueTask, task.get())
				<< "trying to find closer Peer to store '"
				<< task->get_key() << "' value." << std::endl;

		FindPeerRequestBody const request{ task->get_key() };

		auto const closest_candidates = task->select_new_closest_candidates(concurrent_requests_count);

		LOG_DEBUG(StoreValueTask, task.get()) << "inFlightRequests=" << task->inFlightRequests() <<
		", closest candidates count: " << closest_candidates.size() << std::endl;

		for (auto const& c : closest_candidates)
			send_find_peer_to_store_request(request, c, task);

		// If no more requests are in flight
		// we know the closest peers hence ask
		// them to store the value.
		if (task->have_all_requests_completed())
			send_store_requests(task);
	}

	static void send_find_peer_to_store_request(FindPeerRequestBody const& request,
		Peer const& current_candidate, std::shared_ptr< StoreValueTask > task)
	{
		LOG_DEBUG(StoreValueTask, task.get())
				<< "sending find Peer request to store '"
				<< task->get_key() << "' to '"
				<< current_candidate << "'." << std::endl;

		// On message received, process it.
		auto on_message_received = [ task ] (Poco::Net::SocketAddress const& s
			, Header const& h, buffer::const_iterator i, buffer::const_iterator e)
		{
			handle_find_peer_to_store_response(s, h, i, e, task);
		};

		// On error, retry with another endpoint.
		auto on_error = [ task, current_candidate ] (std::error_code const&)
		{
			// XXX: Can also flag candidate as invalid is
			// present in routing table.
			task->flag_candidate_as_invalid(current_candidate.id_);
			try_to_store_value(task);
		};

		task->tracker_.send_request(request, current_candidate.endpoint_,
			PEER_LOOKUP_TIMEOUT, on_message_received, on_error);
	}

	static void handle_find_peer_to_store_response(Poco::Net::SocketAddress const& s, Header const& h,
		buffer::const_iterator i, buffer::const_iterator e, std::shared_ptr< StoreValueTask > task)
	{
		LOG_DEBUG(StoreValueTask, task.get())
				<< "handle find Peer to store response from '"
				<< s.toString() << "'." << std::endl;

		if (h.type_ != Header::FIND_PEER_RESPONSE)
		{
			LOG_DEBUG(StoreValueTask, task.get())
					<< "unexpected find Peer response (type="
					<< int(h.type_) << ")" << std::endl;

			task->flag_candidate_as_invalid(h.source_id_);
			try_to_store_value(task);
			return;

		};

		FindPeerResponseBody response;
		if (auto failure = deserialize(i, e, response))
		{
			LOG_DEBUG(StoreValueTask, task.get())
					<< "failed to deserialize find Peer response ("
					<< failure.message() << ")" << std::endl;
			task->flag_candidate_as_invalid(h.source_id_);
		}
		else
		{
			task->flag_candidate_as_valid(h.source_id_);
			task->add_candidates(response.peers_);
		}
		try_to_store_value(task);
	}

	static void send_store_requests(std::shared_ptr<StoreValueTask> task)
	{
		auto const & candidates = task->select_closest_valid_candidates(REDUNDANT_SAVE_COUNT);

		LOG_DEBUG(StoreValueTask, task.get())
				<< "sending store request to "
				<< candidates.size() << " candidates" << std::endl;
		for (auto c : candidates)
			send_store_request(c, task);

		if (candidates.empty())
			task->notify_caller(make_error_code(MISSING_PEERS));
		else
			task->notify_caller(std::error_code{});
	}

	static void send_store_request(Peer const& current_candidate, std::shared_ptr<StoreValueTask> task)
	{
		LOG_DEBUG(StoreValueTask, task.get())
				<< "send store request of '"
				<< task->get_key() << "' to '"
				<< current_candidate << "'." << std::endl;

		StoreValueRequestBody const request{ task->get_key(), task->get_data() };
		task->tracker_.send_request(request, current_candidate.endpoint_);
	}

private:
	TrackerType & tracker_;
	DataType data_;
	SaveHandlerType save_handler_;
};

/**
 *
 */
template< typename DataType, typename TrackerType, typename RoutingTableType, typename HandlerType >
void start_store_value_task(id const& key, DataType&& data, TrackerType& tracker,
	RoutingTableType& routing_table, HandlerType&& save_handler)
{
	using handler_type = typename std::decay< HandlerType >::type;
	using task = StoreValueTask< handler_type, TrackerType, DataType >;

	task::start(key, std::move(data), tracker, routing_table, std::forward< HandlerType >(save_handler));
}

} // namespace detail
} // namespace kademlia

#endif

