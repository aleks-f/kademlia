#include "LookupTask.h"


namespace kademlia {
namespace detail {


void LookupTask::flag_candidate_as_valid(id const& candidate_id)
{
	auto i = find_candidate(candidate_id);
	if (i == candidates_.end()) return;

	-- in_flight_requests_count_;
	i->second.state_ = candidate::STATE_RESPONDED;
}


void LookupTask::flag_candidate_as_invalid(id const& candidate_id)
{
	auto i = find_candidate(candidate_id);
	if (i == candidates_.end())
		return;

	-- in_flight_requests_count_;
	i->second.state_ = candidate::STATE_TIMEOUTED;
}


std::vector<Peer> LookupTask::select_new_closest_candidates(std::size_t max_count)
{
	std::vector<Peer> candidates;

	// Iterate over all candidates until we picked
	// candidates_max_count not-contacted candidates.
	for (auto i = candidates_.begin(), e = candidates_.end()
		; i != e && in_flight_requests_count_ <max_count
		; ++ i)
	{
		if (i->second.state_ == candidate::STATE_UNKNOWN)
		{
			i->second.state_ = candidate::STATE_CONTACTED;
			++ in_flight_requests_count_;
			candidates.push_back(i->second.peer_);
		}
	}

	return candidates;
}


std::vector<Peer> LookupTask::select_closest_valid_candidates(std::size_t max_count)
{
	std::vector<Peer> candidates;

	// Iterate over all candidates until we picked
	// candidates_max_count responsive candidates.
	for (auto i = candidates_.begin(), e = candidates_.end()
		; i != e && candidates.size() <max_count
		; ++ i)
	{
		if (i->second.state_ == candidate::STATE_RESPONDED)
			candidates.push_back(i->second.peer_);
	}

	return candidates;
}


bool LookupTask::have_all_requests_completed() const
{
	return in_flight_requests_count_ == 0;
}


id const& LookupTask::get_key() const
{
	return key_;
}

void LookupTask::add_candidate(Peer const& p)
{
	LOG_DEBUG(LookupTask, this)
			<<"adding '" <<p <<"'." <<std::endl;

	auto const d = distance(p.id_, key_);
	candidate const c{ p, candidate::STATE_UNKNOWN };
	candidates_.emplace(d, c);
}


LookupTask::candidates_type::iterator LookupTask::find_candidate(id const& candidate_id)
{
	auto const d = distance(candidate_id, key_);
	return candidates_.find(d);
}

}
}
