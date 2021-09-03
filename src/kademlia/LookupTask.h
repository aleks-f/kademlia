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

#ifndef KADEMLIA_LOOKUP_TASK_H
#define KADEMLIA_LOOKUP_TASK_H

#ifdef _MSC_VER
#   pragma once
#endif

#include <cassert>
#include <map>
#include <vector>

#include "Peer.h"
#include "kademlia/log.hpp"

namespace kademlia {
namespace detail {


class LookupTask
{
public:
	void flag_candidate_as_valid(id const& candidate_id);

	void flag_candidate_as_invalid(id const& candidate_id);

	std::vector<Peer> select_new_closest_candidates(std::size_t max_count);

	std::vector<Peer> select_closest_valid_candidates(std::size_t max_count);

	template<typename Peers>
	void add_candidates(Peers const& peers)
	{
		for (auto const& p : peers)
			add_candidate(p);
	}

	std::size_t inFlightRequests() const;

	bool have_all_requests_completed() const;

	id const& get_key() const;

protected:
	~LookupTask() = default;

	template<typename Iterator>
	LookupTask(id const & key, Iterator i, Iterator e)
		: key_{ key },
		  in_flight_requests_count_{ 0 },
		  candidates_{}
	{
		for (; i != e; ++i)
			add_candidate(Peer{ i->first, i->second });
	}

private:
	struct candidate final
	{
		Peer peer_;
		enum
		{
			STATE_UNKNOWN,
			STATE_CONTACTED,
			STATE_RESPONDED,
			STATE_TIMEDOUT,
		} state_;
	};

	using candidates_type = std::map<id, candidate>;

private:
	void add_candidate(Peer const& p);

	candidates_type::iterator find_candidate(id const& candidate_id);

private:
	id key_;
	std::size_t in_flight_requests_count_;
	candidates_type candidates_;
};

inline std::size_t LookupTask::inFlightRequests() const
{
	return in_flight_requests_count_;
}

} // namespace detail
} // namespace kademlia

#endif
