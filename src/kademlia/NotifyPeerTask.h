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

#ifndef KADEMLIA_NOTIFY_PEER_TASK_H
#define KADEMLIA_NOTIFY_PEER_TASK_H

#ifdef _MSC_VER
#   pragma once
#endif

#include "Poco/Net/SocketAddress.h"
#include <memory>
#include <system_error>

#include "LookupTask.h"
#include "Message.h"
#include "Tracker.h"
#include "kademlia/constants.hpp"

namespace kademlia {
namespace detail {

///
template<typename TrackerType, typename OnFinishType>
class NotifyPeerTask final : public LookupTask
{
public:
	template<typename RoutingTableType>
	static void start(detail::id const & key, TrackerType & tracker, RoutingTableType & routing_table, OnFinishType on_finish)
	{
		std::shared_ptr<NotifyPeerTask> c;
		c.reset(new NotifyPeerTask(key, tracker, routing_table, on_finish));
		try_to_notify_neighbors(c);
	}

private:
	template<typename RoutingTableType>
	NotifyPeerTask(detail::id const & key, TrackerType & tracker, RoutingTableType & routing_table, OnFinishType on_finish):
		LookupTask(key,
			routing_table.find(key),
			routing_table.end(),
			tracker.addressV4(),
			tracker.addressV6()),
		tracker_(tracker),
		on_finish_( on_finish )
	{
		LOG_DEBUG(NotifyPeerTask, this) << "create notify Peer task for '"<< key << "' Peer." << std::endl;
	}

	static void try_to_notify_neighbors(std::shared_ptr<NotifyPeerTask> task)
	{
		LOG_DEBUG(NotifyPeerTask, task.get()) << "sending find Peer to notify '"
				<< task->get_key() << "' owner bucket." << std::endl;

		FindPeerRequestBody const request{ task->get_key() };

		auto closest_peers = task->select_new_closest_candidates(CONCURRENT_FIND_PEER_REQUESTS_COUNT);

		LOG_DEBUG(NotifyPeerTask, task.get()) << "sending find Peer to notify "
				<< closest_peers.size() << " owner buckets." << std::endl;
		for (auto const& c : closest_peers)
			send_notify_peer_request(request, c, task);
	}

	static void send_notify_peer_request(FindPeerRequestBody const& request,
		Peer const& current_peer, std::shared_ptr<NotifyPeerTask> task)
	{
		LOG_DEBUG(NotifyPeerTask, task.get()) << "sending peer notification to '"
				<< current_peer << "'." << std::endl;

		auto on_message_received = [ task, current_peer ] (Poco::Net::SocketAddress const& s, Header const& h
			, buffer::const_iterator i, buffer::const_iterator e)
		{
			LOG_DEBUG(NotifyPeerTask, task.get()) << "valid peer: '" << current_peer << "'." << std::endl;
			task->flag_candidate_as_valid(current_peer.id_);
			handle_notify_peer_response(s, h, i, e, task);
			task->check_for_completion();
		};

		auto on_error = [ task, current_peer ] (std::error_code const&)
		{
			LOG_DEBUG(NotifyPeerTask, task.get()) << "invalid peer: '" << current_peer << "'." << std::endl;
			task->flag_candidate_as_invalid( current_peer.id_ );
            task->check_for_completion();
		};

		task->tracker_.send_request(request, current_peer.endpoint_, PEER_LOOKUP_TIMEOUT, on_message_received, on_error);
	}

	void check_for_completion()
	{
		if (have_all_requests_completed())
			on_finish_();
    }

	static void handle_notify_peer_response(Poco::Net::SocketAddress const& s, Header const& h
		, buffer::const_iterator i, buffer::const_iterator e, std::shared_ptr<NotifyPeerTask> task)
	{
		LOG_DEBUG(NotifyPeerTask, task.get()) << "handle notify Peer response from '" << s.toString()
				<< "'." << std::endl;

		assert(h.type_ == Header::FIND_PEER_RESPONSE);
		FindPeerResponseBody response;

		if (auto failure = deserialize(i, e, response))
		{
			LOG_DEBUG(NotifyPeerTask, &task) << "failed to deserialize find Peer response ("
					<< failure.message() << ")" << std::endl;
			return;
		}

		task->add_candidates(response.peers_);
		// If new candidate have been discovered, notify them.
		task->add_candidates(response.peers_);
		try_to_notify_neighbors(task);
	}

private:
	TrackerType & tracker_;
	OnFinishType on_finish_;
};


template<typename TrackerType, typename RoutingTableType, typename OnFinishType>
void start_notify_peer_task(id const& key, TrackerType & tracker, RoutingTableType & routing_table, OnFinishType on_finish)
{
	using task = NotifyPeerTask<TrackerType, OnFinishType>;
	task::start(key, tracker, routing_table, std::forward<OnFinishType>(on_finish));
}

} // namespace detail
} // namespace kademlia

#endif
