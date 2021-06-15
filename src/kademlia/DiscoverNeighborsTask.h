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

#ifndef KADEMLIA_DISCOVER_NEIGHBORS_TASK_H
#define KADEMLIA_DISCOVER_NEIGHBORS_TASK_H

#ifdef _MSC_VER
#   pragma once
#endif

#include <system_error>
#include <memory>
#include <type_traits>

#include "kademlia/error_impl.hpp"

#include "kademlia/log.hpp"
#include "kademlia/constants.hpp"
#include "Message.h"

namespace kademlia {
namespace detail {


template<typename TrackerType, typename RoutingTableType, typename EndpointsType, typename OnCompleteType>
class DiscoverNeighborsTask final
{
public:
	static void start(id const & my_id, TrackerType & tracker, RoutingTableType & routing_table,
		EndpointsType const& endpoints_to_query, OnCompleteType const& on_complete)
	{
		std::shared_ptr<DiscoverNeighborsTask> d;
		d.reset(new DiscoverNeighborsTask(my_id, tracker, routing_table, endpoints_to_query, on_complete));
		search_ourselves(d);
	}

private:
	DiscoverNeighborsTask(id const & my_id, TrackerType & tracker, RoutingTableType & routing_table,
		EndpointsType const& endpoints_to_query, OnCompleteType const& on_complete): my_id_(my_id),
			tracker_(tracker),
			routing_table_(routing_table),
			endpoints_to_query_(endpoints_to_query),
			on_complete_(on_complete)
	{
		//kademlia::detail::enable_log_for("DiscoverNeighborsTask");
		LOG_DEBUG(DiscoverNeighborsTask, this) << "create discover neighbors task." << std::endl;
	}

	static void search_ourselves(std::shared_ptr<DiscoverNeighborsTask> task)
	{
		if (task->endpoints_to_query_.empty())
		{
			auto f = make_error_code(INITIAL_PEER_FAILED_TO_RESPOND);
			task->on_complete_(f);
			return;
		}

		// Retrieve the next endpoint to query.
		auto const endpoint_to_query = task->endpoints_to_query_.back();
		task->endpoints_to_query_.pop_back();

		// On message received, process it.
		auto on_message_received = [task] (IPEndpoint const& s, Header const& h, buffer::const_iterator i, buffer::const_iterator e)
		{
			handle_initial_contact_response(task, s, h, i, e);
		};

		// On error, retry with another endpoint.
		auto on_error = [task] (std::error_code const&)
		{
			search_ourselves(task);
		};

		LOG_DEBUG(DiscoverNeighborsTask, task.get()) << "query '" << endpoint_to_query 
				<< "'." << std::endl;

		task->tracker_.send_request(FindPeerRequestBody{ task->my_id_ }, endpoint_to_query,
			INITIAL_CONTACT_RECEIVE_TIMEOUT, on_message_received, on_error);
	}

	static void handle_initial_contact_response(std::shared_ptr<DiscoverNeighborsTask> task, IPEndpoint const& s,
		Header const& h, buffer::const_iterator i, buffer::const_iterator e)
	{
		LOG_DEBUG(DiscoverNeighborsTask, task.get()) << "handling initial contact response." << std::endl;

		if (h.type_ != Header::FIND_PEER_RESPONSE)
		{
			LOG_DEBUG(DiscoverNeighborsTask, task.get()) << "unexpected find peer response (type="
					<< int(h.type_) << ")" << std::endl;
			search_ourselves(task);
			return;

		};

		FindPeerResponseBody response;
		if (auto failure = deserialize(i, e, response))
		{
			LOG_DEBUG(DiscoverNeighborsTask, task.get())
					<< "failed to deserialize find peer response ("
					<< failure.message() << ")" << std::endl;

			search_ourselves(task);
			return;
		}

		// Add discovered peers.
		for (auto const& peer : response.peers_)
			task->routing_table_.push(peer.id_, peer.endpoint_);

		LOG_DEBUG(DiscoverNeighborsTask, task.get())
				<< "added '" << response.peers_.size()
				<< "' initial peer(s)." << std::endl;

		task->on_complete_(std::error_code{});
	}

private:
	id const& my_id_;
	TrackerType & tracker_;
	RoutingTableType & routing_table_;
	EndpointsType endpoints_to_query_;
	OnCompleteType on_complete_;
};

/**
 *
 */
template<typename TrackerType
		, typename RoutingTableType
		, typename EndpointsType
		, typename OnCompleteType>
void start_discover_neighbors_task(id const& my_id, TrackerType & tracker, RoutingTableType & routing_table,
	EndpointsType const& endpoints_to_query, OnCompleteType const& on_complete)
{
	using task = DiscoverNeighborsTask<TrackerType, RoutingTableType, EndpointsType, OnCompleteType>;
	task::start(my_id, tracker, routing_table, endpoints_to_query, on_complete);
}

} // namespace detail
} // namespace kademlia

#endif

