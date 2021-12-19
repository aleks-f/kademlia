// Copyright (c) 2014, David Keller
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

#include "kademlia/log.hpp"
#include "Poco/AutoPtr.h"
#include "Poco/ConsoleChannel.h"
#include "Poco/Message.h"
#include "Poco/PatternFormatter.h"
#include "Poco/FormattingChannel.h"

#include <iostream>
#include <iomanip>
#include <set>

using Poco::Logger;
using Poco::LogStream;
using Poco::ColorConsoleChannel;
using Poco::AutoPtr;
using Poco::Message;
using Poco::PatternFormatter;
using Poco::FormattingChannel;

namespace kademlia {
namespace detail {


namespace {

using enabled_modules_log_type = std::set<std::string>;

enabled_modules_log_type& get_enabled_modules()
{
	static enabled_modules_log_type enabled_modules_;
	return enabled_modules_;
}

} // anonymous namespace


std::ostream& getDebugLog(const char* module, const void* thiz, std::tm* pTM)
{
	if (!pTM)
	{
		std::time_t t = std::time(nullptr);
		pTM = std::localtime(&t);
	}
	return std::cout << "[debug] " << std::put_time(pTM, "%F %H:%M:%S") << " (" << module << " @ "
					 << std::hex << ( std::uintptr_t( thiz ) & 0xffffff )
					 << std::dec << ") ";
}

Poco::LogStream& getPocoLog(char const * module)
{
	static std::vector<Poco::LogStream*> logStream;
	logStream.emplace_back(new Poco::LogStream(Poco::Logger::get(module)));
	return *logStream.back();
}


void enableLogFor(std::string const& module)
{
	get_enabled_modules().insert( module );
}


void disableLogFor(std::string const& module)
{
	get_enabled_modules().erase( module );
}


bool isLogEnabled(std::string const& module)
{
	return get_enabled_modules().count("*") > 0
			|| get_enabled_modules().count(module) > 0;
}

struct LogInitializer
{
	AutoPtr<FormattingChannel> _pChannel;
	LogInitializer():
		_pChannel(new FormattingChannel(new PatternFormatter("%Y-%m-%d %H:%M:%S.%i [%p] %s<%I>: %t")))
	{
		_pChannel->setChannel(new ColorConsoleChannel);
		Logger& root = Logger::root();
		root.setChannel(_pChannel);
		root.setLevel(Message::PRIO_DEBUG);
		//enableLogFor("DiscoverNeighborsTask");
		//enableLogFor("DiscoverNeighborsTaskTest");
		//enableLogFor("Engine");
		//enableLogFor("EngineTest");
		//enableLogFor("FakeSocket");
		//enableLogFor("FindValueTask");
		//enableLogFor("LookupTask");
		//enableLogFor("MessageSocket");
		//enableLogFor("Network");
		//enableLogFor("NotifyPeerTask");
		//enableLogFor("ResponseCallbacks");
		//enableLogFor("ResponseRouter");
		//enableLogFor("routing_table");
		//enableLogFor("Session");
		//enableLogFor("SessionImpl");
		//enableLogFor("SocketAdapter");
		//enableLogFor("SocketAdapterTest");
		//enableLogFor("StoreValueTask");
		//enableLogFor("Timer");
		//enableLogFor("TimerTest");
		//enableLogFor("Tracker");
		//enableLogFor("TrackerMock");
	}
};

const LogInitializer logInitializer;

} // namespace detail
} // namespace kademlia

