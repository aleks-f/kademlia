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

#ifndef KADEMLIA_FIND_VALUE_TASK_H
#define KADEMLIA_FIND_VALUE_TASK_H

#ifdef _MSC_VER
#   pragma once
#endif

#include <system_error>
#include <memory>
#include <type_traits>

#include "kademlia/error_impl.hpp"

#include "LookupTask.h"
#include "kademlia/log.hpp"
#include "kademlia/constants.hpp"
#include "Message.h"

namespace kademlia {
namespace detail {

/**
 *  @brief This class represents a find value task.
 *  @details
 *  Its purpose is to perform network request
 *  to find the Peer response of storing a value.
 *
 *  @dot
 *  digraph algorithm {
 *	  fontsize=12
 *  
 *	  node [shape=diamond];
 *	  "is any closer Peer ?";
 *	  "has Peer responded ?";
 *	  "does Peer has data ?";
 *	  "is any pending request ?";
 *  
 *	  node [shape=box];
 *	  "query Peer(s)";
 *	  "add Peer(s) from response"
 *  
 *	  node [shape=ellipse];
 *	  "data not found";
 *	  "data found";
 *	  "start";
 *  
 *	  "start" -> "is any closer Peer ?" [style=dotted]
 *	  "is any closer Peer ?":e -> "is any pending request ?" [label=no]
 *	  "is any closer Peer ?":s -> "query Peer(s)" [label=yes]
 *	  "query Peer(s)" -> "has Peer responded ?"
 *	  "has Peer responded ?":e -> "is any closer Peer ?" [label=no]
 *	  "has Peer responded ?":s -> "does Peer has data ?" [label=yes]
 *	  "does Peer has data ?":s -> "data found" [label=yes]
 *	  "does Peer has data ?":e -> "add Peer(s) from response" [label=no]
 *	  "add Peer(s) from response":s -> "is any closer Peer ?"
 *	  "is any pending request ?":e -> "data not found" [label=no]
 *  }
 *  @enddot
 */
template<typename LoadHandlerType, typename TrackerType, typename DataType>
class FindValueTask final : public LookupTask
{
public:
	template<typename RoutingTableType>
	static void start(detail::id const & key, TrackerType & tracker, RoutingTableType & routing_table, LoadHandlerType handler)
	{
		std::shared_ptr<FindValueTask> t;
		t.reset(new FindValueTask(key, tracker, routing_table, std::move(handler)));
		try_candidates(t);
	}

private:
	template<typename RoutingTableType>
	FindValueTask(id const & searched_key, TrackerType & tracker,
		RoutingTableType & routing_table, LoadHandlerType load_handler):
			LookupTask(searched_key,
				routing_table.find(searched_key),
				routing_table.end()),
			tracker_(tracker),
			load_handler_(std::move(load_handler)),
			is_finished_()
	{
		LOG_DEBUG(FindValueTask, this) << "create find value task for '"
			<< searched_key << "' value." << std::endl;
	}

	void notify_caller(DataType const& data)
	{
		assert(! is_caller_notified());
		load_handler_(std::error_code(), data);
		is_finished_ = true;
	}

	void notify_caller(std::error_code const& failure)
	{
		assert(! is_caller_notified());
		load_handler_(failure, DataType{});
		is_finished_ = true;
	}

	bool is_caller_notified() const
	{
		return is_finished_;
	}

	static void try_candidates(std::shared_ptr<FindValueTask> task,
		std::size_t concurrent_requests_count = CONCURRENT_FIND_PEER_REQUESTS_COUNT)
	{
		auto const closest_candidates = task->select_new_closest_candidates
				(concurrent_requests_count);

		FindValueRequestBody const request{ task->get_key() };
		for (auto const& c : closest_candidates)
			send_find_value_request(request, c, task);

		if (task->have_all_requests_completed())
			task->notify_caller(make_error_code(VALUE_NOT_FOUND));
	}

	static void send_find_value_request(FindValueRequestBody const& request
		, Peer const& current_candidate, std::shared_ptr<FindValueTask> task)
	{
		LOG_DEBUG(FindValueTask, task.get()) << "sending find '" << task->get_key()
				<< "' value request to '"
				<< current_candidate << "'." << std::endl;

		// On message received, process it.
		auto on_message_received = [ task, current_candidate ] (IPEndpoint const& s,
			Header const& h, buffer::const_iterator i, buffer::const_iterator e)
		{
			if (task->is_caller_notified())
				return;

			task->flag_candidate_as_valid(current_candidate.id_);
			handle_find_value_response(s, h, i, e, task);
		};

		// On error, retry with another endpoint.
		auto on_error = [ task, current_candidate ] (std::error_code const&)
		{
			if (task->is_caller_notified()) return;

			// XXX: Current current_candidate must be flagged as stale.
			task->flag_candidate_as_invalid(current_candidate.id_);
			try_candidates(task);
		};

		task->tracker_.send_request(request, current_candidate.endpoint_,
			PEER_LOOKUP_TIMEOUT, on_message_received, on_error);
	}

	/**
	 *  @brief This method is called while searching for
	 *		 the Peer owner of the value.
	 */
	static void handle_find_value_response(IPEndpoint const& s, Header const& h
		, buffer::const_iterator i, buffer::const_iterator e, std::shared_ptr<FindValueTask> task)
	{
		LOG_DEBUG(FindValueTask, task.get())
				<< "handling response type '"
				<< int(h.type_) << "' to find '"
				<< task->get_key() << "' value." << std::endl;

		if (h.type_ == Header::FIND_PEER_RESPONSE)
			// The current Peer didn't know the value
			// but provided closest peers.
			send_find_value_requests_on_closer_peers(i, e, task);
		else if (h.type_ == Header::FIND_VALUE_RESPONSE)
			// The current Peer knows the value.
			process_found_value(i, e, task);
	}

	/**
	 *  @brief This method is called when closest peers
	 *		 to the value we are looking are discovered.
	 *		 It recursively query new discovered peers
	 *		 or report an error to the use handler if
	 *		 all peers have been tried.
	 */
	static void send_find_value_requests_on_closer_peers(buffer::const_iterator i
		, buffer::const_iterator e, std::shared_ptr<FindValueTask> task)
	{
		LOG_DEBUG(FindValueTask, task.get()) << "checking if found closest peers to '"
				<< task->get_key() << "' value from closer peers."
				<< std::endl;

		FindPeerResponseBody response;
		if (auto failure = deserialize(i, e, response))
		{
			LOG_DEBUG(FindValueTask, task.get())
					<< "failed to deserialize find Peer response '"
					<< task->get_key() << "' because ("
					<< failure.message() << ")." << std::endl;
			return;
		}
		task->add_candidates(response.peers_);
		try_candidates(task);
	}

	/**
	 *  @brief This method is called once the searched value
	 *		 has been found. It forwards the value to
	 *		 the user handler.
	 */
	static void process_found_value(buffer::const_iterator i, buffer::const_iterator e
		, std::shared_ptr<FindValueTask> task)
	{
		LOG_DEBUG(FindValueTask, task.get())
				<< "found '" << task->get_key()
				<< "' value." << std::endl;

		FindValueResponseBody response;
		if (auto failure = deserialize(i, e, response))
		{
			LOG_DEBUG(FindValueTask, task.get())
					<< "failed to deserialize find value response ("
					<< failure.message() << ")" << std::endl;
			return;
		}

		task->notify_caller(response.data_);
	}

private:
	TrackerType & tracker_;
	LoadHandlerType load_handler_;
	bool is_finished_;
};

/**
 *
 */
template<typename DataType, typename TrackerType, typename RoutingTableType, typename HandlerType>
void start_find_value_task(id const& key, TrackerType & tracker
	, RoutingTableType & routing_table, HandlerType && handler)
{
	using handler_type = typename std::decay<HandlerType>::type;
	using task = FindValueTask<handler_type, TrackerType, DataType>;

	task::start(key, tracker, routing_table, std::forward<HandlerType>(handler));
}

} // namespace detail
} // namespace kademlia

#endif

