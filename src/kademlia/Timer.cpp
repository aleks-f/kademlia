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

#include "Timer.h"
#include "kademlia/error_impl.hpp"

using namespace std::chrono;
using namespace Poco;
using namespace Poco::Net;

namespace kademlia {
namespace detail {

Timer::Timer(SocketProactor& ioService): _ioService(ioService),
	timeouts_{}
{
	LOG_DEBUG(Timer, this) << "Timer created." << std::endl;
}

void Timer::schedule_next_tick(time_point const& expiration_time)
{
	LOG_DEBUG(Timer, this) << "\tscheduling next tick ..." << std::endl;
	auto on_fire = [this]()
	{
		Poco::Mutex::ScopedLock l(_mutex);
		if (!timeouts_.empty())
		{
			LOG_DEBUG(Timer, this) << "\ttimeouts=" << timeouts_.size() << std::endl;
			// The callbacks to execute are the first
			// n callbacks with the same keys.
			auto begin = timeouts_.begin();
			auto end = timeouts_.upper_bound(begin->first);
			// Call the user callbacks.
			for (auto i = begin; i != end; ++i)
			{
				LOG_DEBUG(Timer, this) << "\tcallback()" << std::endl;
				i->second();
			}

			// And remove the timeout.
			timeouts_.erase(begin, end);
		}
		// If there is a remaining timeout, schedule it.
		if (!timeouts_.empty())
		{
			LOG_DEBUG(Timer, this) << "\tschedule remaining timers" << std::endl;
			schedule_next_tick(timeouts_.begin()->first);
		}
	};

	int tout = static_cast<int>(getTimeout(expiration_time));
	if (tout < 0)
		LOG_DEBUG(Timer, this) << "\ttimer expired: " << tout << " [ms]" << std::endl;

	_ioService.addWork(std::move(on_fire), tout);
	LOG_DEBUG(Timer, this) << "\tscheduled timer in " << tout << " [ms]" << std::endl;
}


Poco::Timestamp::TimeDiff Timer::getTimeout(time_point const& expiration_time)
{
	auto t = duration_cast<microseconds>(expiration_time.time_since_epoch());
	Poco::Timestamp ts(t.count() - duration_cast<microseconds>(clock().now().time_since_epoch()).count());
	return ts.epochMicroseconds()/1000;
}


} // namespace detail
} // namespace kademlia

