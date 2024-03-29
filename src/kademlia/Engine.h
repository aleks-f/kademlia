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

#ifndef KADEMLIA_ENGINE_HPP
#define KADEMLIA_ENGINE_HPP

#ifdef _MSC_VER
#   pragma once
#endif

#include <algorithm>
#include <stdexcept>
#include <queue>
#include <chrono>
#include <random>
#include <memory>
#include <utility>
#include <type_traits>
#include <functional>
#include "Poco/Net/SocketProactor.h"
#include "kademlia/endpoint.hpp"
#include "kademlia/error_impl.hpp"
#include "kademlia/log.hpp"
#include "Poco/Net/SocketAddress.h"
#include "MessageSerializer.h"
#include "ResponseRouter.h"
#include "Network.h"
#include "Message.h"
#include "MessageSocket.h"
#include "kademlia/routing_table.hpp"
#include "kademlia/value_store.hpp"
#include "FindValueTask.h"
#include "StoreValueTask.h"
#include "DiscoverNeighborsTask.h"
#include "NotifyPeerTask.h"
#include "Tracker.h"
#include "Message.h"
#include "Poco/Mutex.h"
#include "Poco/ScopedLock.h"

namespace kademlia {
namespace detail {


template<typename UnderlyingSocketType>
class Engine final
{
public:
	using key_type = std::vector<std::uint8_t>;
	using routing_table_type = routing_table<Poco::Net::SocketAddress>;

public:
	Engine(Poco::Net::SocketProactor& io_service, endpoint const& ipv4, endpoint const& ipv6, id const& new_id = id{}, bool initialized = true):
			random_engine_(std::random_device{}()),
			my_id_(new_id == id{} ? id{ random_engine_ } : new_id),
			_initialized(initialized),
			network_(io_service,
				MessageSocketType::ipv4(io_service, ipv4),
				MessageSocketType::ipv6(io_service, ipv6),
				std::bind(&Engine::handle_new_message, this,
					std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
			tracker_(io_service, my_id_, network_, random_engine_),
			routing_table_(my_id_),
			value_store_(),
			pending_notifications_count_()
	{
		LOG_DEBUG(Engine, this) << "Peerless Engine (" << my_id_ << ") created(" <<
			ipv4.address() << ':' << ipv4.service() << ", " <<
			ipv6.address() << ':' << ipv6.service() << ')' << std::endl;
	}

	Engine(Poco::Net::SocketProactor& io_service, endpoint const& initial_peer,
		endpoint const& ipv4, endpoint const& ipv6, id const& new_id = id{}):
			Engine(io_service, ipv4, ipv6, new_id, false)
	{
		LOG_DEBUG(Engine, this) << "Engine bootstrapping using peer '" << initial_peer << "'." << std::endl;
		auto on_initialized = [this]
		{
			_initialized = true;
		};

		discover_neighbors(initial_peer, on_initialized);

		while (!_initialized)
			io_service.poll();
	}

	Engine(Engine const&) = delete;

	Engine & operator = (Engine const&) = delete;

	bool initialized() const
	{
		return _initialized;
	}

	void start()
	{
		network_.start();
	}

	template<typename HandlerType>
	void asyncSave(key_type const& key, data_type&& data, HandlerType&& handler)
	{
		LOG_DEBUG(Engine, this) << "executing async save of key '" << toString(key) << "'." << std::endl;
		id valID(key);
		Poco::Mutex::ScopedLock l(_mutex);
		value_store_[valID] = data;
		start_store_value_task(id(key), std::move(data), tracker_, routing_table_, std::forward<HandlerType>(handler));
	}

	template<typename HandlerType>
	void asyncLoad(key_type const& key, HandlerType&& handler)
	{
		LOG_DEBUG(Engine, this) << "executing async load of key '" << toString( key ) << "'." << std::endl;
		id valID(key);
		Poco::Mutex::ScopedLock l(_mutex);
		auto it = value_store_.find(valID);
		if (it != value_store_.end())
		{
			handler(std::error_code(), it->second);
			return;
		}
		start_find_value_task< data_type >(id(key), tracker_, routing_table_, std::forward<HandlerType>(handler));
	}

	const value_store_type& data() const
	{
		return value_store_;
	}

private:
	using pending_task_type = std::function<void ()>;
	using MessageSocketType = MessageSocket<UnderlyingSocketType>;
	using NetworkType = Network<MessageSocketType>;
	using random_engine_type = std::default_random_engine;
	using TrackerType = Tracker<random_engine_type, NetworkType>;

	void process_new_message(Poco::Net::SocketAddress const& sender, Header const& h,
		buffer::const_iterator i, buffer::const_iterator e)
	{
		switch (h.type_)
		{
			case Header::PING_REQUEST:
				handle_ping_request(sender, h);
				break;
			case Header::STORE_REQUEST:
				handle_store_request(sender, h, i, e);
				break;
			case Header::FIND_PEER_REQUEST:
				handle_find_peer_request(sender, h, i, e);
				break;
			case Header::FIND_VALUE_REQUEST:
				handle_find_value_request(sender, h, i, e);
				break;
			default:
				tracker_.handle_new_response(sender, h, i, e);
				break;
		}
	}

	void handle_ping_request(Poco::Net::SocketAddress const& sender, Header const& h)
	{
		LOG_DEBUG(Engine, this) << "handling ping request." << std::endl;
		//logAccess(sender, h);
		tracker_.send_response(h.random_token_, Header::PING_RESPONSE, sender);
	}

	void handle_store_request(Poco::Net::SocketAddress const& sender, Header const& h,
		buffer::const_iterator i, buffer::const_iterator e)
	{
		LOG_DEBUG(Engine, this) << "handling store request." << std::endl;
		//logAccess(sender, h);
		StoreValueRequestBody request;
		if (auto failure = deserialize(i, e, request))
		{
			LOG_DEBUG(Engine, this) << "failed to deserialize store value request ("
					<< failure.message() << ")." << std::endl;
			return;
		}
		Poco::Mutex::ScopedLock l(_mutex);
		value_store_[request.data_key_hash_] = std::move(request.data_value_);
	}

	void handle_find_peer_request(Poco::Net::SocketAddress const& sender, Header const& h,
		buffer::const_iterator i, buffer::const_iterator e)
	{
		LOG_DEBUG(Engine, this) << "handling find peer request." << std::endl;
		//logAccess(sender, h);

		// Ensure the request is valid.
		FindPeerRequestBody request;
		if (auto failure = deserialize(i, e, request))
		{
			LOG_DEBUG(Engine, this) << "failed to deserialize find peer request ("
					<< failure.message() << ")" << std::endl;
			return;
		}
		send_find_peer_response(sender, h.random_token_, request.peer_to_find_id_);
	}

	void send_find_peer_response(Poco::Net::SocketAddress const& sender, id const& random_token, id const& peer_to_find_id)
	{
		// Find X closest peers and save
		// their location into the response..
		FindPeerResponseBody response;

		auto remaining_peer = ROUTING_TABLE_BUCKET_SIZE;
		auto i = routing_table_.find(peer_to_find_id);
		auto e = routing_table_.end();
		for (;i != e && remaining_peer > 0; ++i, -- remaining_peer)
			response.peers_.push_back({i->first, i->second});

		LOG_DEBUG(Engine, this) << "found " << response.peers_.size() << " peers:" << std::endl;
		for (auto& p : response.peers_)
		{
			LOG_DEBUG(Engine, this) << p.endpoint_.toString() << std::endl;
		}
		// Now send the response.
		tracker_.send_response(random_token, response, sender);
	}

	void handle_find_value_request(Poco::Net::SocketAddress const& sender, Header const& h,
		buffer::const_iterator i, buffer::const_iterator e)
	{
		LOG_DEBUG(Engine, this) << "handling find value request." << std::endl;
		//logAccess(sender, h);

		FindValueRequestBody request;
		if (auto failure = deserialize(i, e, request))
		{
			LOG_DEBUG(Engine, this) << "failed to deserialize find value request ("
				<< failure.message() << ")" << std::endl;
			return;
		}

		Poco::ScopedLockWithUnlock<Poco::Mutex> l(_mutex);
		auto found = value_store_.find(request.value_to_find_);
		if (found == value_store_.end())
		{
			l.unlock();
			send_find_peer_response(sender, h.random_token_, request.value_to_find_);
		}
		else
		{
			FindValueResponseBody const response{ found->second };
			tracker_.send_response(h.random_token_, response, sender);
		}
	}

	template<typename OnInitialized>
	void discover_neighbors(endpoint const& initial_peer, OnInitialized on_initialized)
	{
		// Initial peer should know our neighbors, hence ask
		// him which peers are close to our own id.

		auto endpoints_to_query = network_.resolve_endpoint(initial_peer);

		auto on_discovery = [this, on_initialized] (std::error_code const& failure)
		{
			if (failure) throw std::system_error{failure};
			notify_neighbors(on_initialized);
		};

		start_discover_neighbors_task(my_id_, tracker_, routing_table_, std::move(endpoints_to_query), on_discovery);
	}

	id get_closest_neighbor_id()
	{
		// Find our closest neighbor.
		auto closest_neighbor = routing_table_.find(my_id_);
		if (closest_neighbor->first == my_id_) ++ closest_neighbor;
		assert(closest_neighbor != routing_table_.end() && "at least one peer is known");
		return closest_neighbor->first;
	}

	template<typename OnInitialized>
	void notify_neighbors(OnInitialized on_initialized)
		/// Refresh each bucket.
	{
		auto closest_neighbor_id = get_closest_neighbor_id();
		auto i = id::BIT_SIZE - 1;

		// Skip empty buckets.
		while (i && closest_neighbor_id[i] == my_id_[i]) --i;

		pending_notifications_count_ += i;
		auto on_notification_complete = [ this, on_initialized ]
		{
			if (pending_notifications_count_) --pending_notifications_count_;
			if (!pending_notifications_count_)
				on_initialized();
		};

		// Send refresh from the closest neighbor bucket to the farthest bucket.
		auto refresh_id = my_id_;
		while (i)
		{
			refresh_id[i] = ! refresh_id[i];
			start_notify_peer_task(refresh_id, tracker_, routing_table_, on_notification_complete);
			--i;
		}
	}

	void handle_new_message(Poco::Net::SocketAddress const& sender, buffer::const_iterator i, buffer::const_iterator e )
	{
		LOG_DEBUG(Engine, this) << "received new message from '" << sender.toString() << "'." << std::endl;

		detail::Header h;
		// Try to deserialize Header.
		if (auto failure = deserialize(i, e, h))
		{
			LOG_DEBUG(Engine, this) << "failed to deserialize message Header (" << failure.message() << ")" << std::endl;
			return;
		}

		routing_table_.push(h.source_id_, sender);

		process_new_message(sender, h, i, e);
	}

private:
	random_engine_type random_engine_;
	id my_id_;
	std::atomic<bool> _initialized;
	NetworkType network_;
	TrackerType tracker_;
	routing_table_type routing_table_;
	value_store_type value_store_;
	std::size_t pending_notifications_count_;
	Poco::Mutex _mutex;
};

} // namespace detail
} // namespace kademlia

#endif
