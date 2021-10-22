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
// THIS SOFTWARE IS PROVLOGED BY DAVLOG KELLER AND CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCLOGENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef KADEMLIA_LOG_HPP
#define KADEMLIA_LOG_HPP

#include <cstdio>
#include <cctype>
#include <cstdint>
#include <string>
#include <sstream>
#include <ctime>
#include <iostream>
#include "kademlia/IPEndpoint.h"
#include "kademlia/Message.h"
#include "Poco/Logger.h"
#include "Poco/LogStream.h"

namespace kademlia {
namespace detail {


std::ostream& getDebugLog(char const * module, void const * thiz, std::tm* tm = 0 );
Poco::LogStream& getPocoLog(char const * module);

void enableLogFor(std::string const& module);
void disableLogFor(std::string const& module);
bool isLogEnabled(std::string const& module);

/**
 *  This macro deserves some explanation.
 *  The goal here is to provide a macro that can be called like:
 *	  LOG_DEBUG( my_module ) << "my content" << std::endl;
 *  Depending on 'my_module' string, the code may or may not be executed.
 *  To achieve this, the call to an actual stream is made inside
 *  a for loop whose condition checks if the module associated with
 *  my_module is enabled. Because the call to isLogEnabled()
 *  can be costly, its result is cached in a static variable.
 */
#define KADEMLIA_ENABLE_DEBUG
#ifdef KADEMLIA_ENABLE_DEBUG
#   define LOG_DEBUG(module, thiz)											\
	for (bool used = false; ! used; used = true)							\
		for ( static bool enabled = kademlia::detail::isLogEnabled(#module) \
			; enabled && ! used; used = true)								\
			Poco::LogStream(Poco::Logger::get(#module)).debug() << thiz << ' '
#else
#   define LOG_DEBUG( module, thiz ) \
	while ( false )					 \
		kademlia::detail::getDebugLog( #module, thiz )
#endif

template< typename Container >
inline std::string toString(const Container & c)
{
	std::ostringstream out;

	for (auto const& v : c)
	{
		if (std::isprint(v))
			out << v;
		else
			out << '\\' << uint16_t( v );
	}

	return out.str();
}

inline void logAccess(kademlia::detail::IPEndpoint const& sender, kademlia::detail::Header const& h)
{
	static int cnt;
	std::cout << cnt++ << ' ';
	std::cout << h << " from " << sender.address_.toString() << ':' << sender.port_ << std::endl;
	std::cout << "source:[" << h.source_id_ << ']' << std::endl;
	std::cout << "random:[" << h.random_token_ << ']' << std::endl;
}


} // namespace detail
} // namespace kademlia

#endif

