// Copyright (c) 2013-2014, David Keller
// All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the University of California, Berkeley nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
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


#include "kademlia/endpoint.hpp"
#include "kademlia/Session.h"
#include "kademlia/error.hpp"
#include "kademlia/detail/Util.h"
#include "Poco/Timestamp.h"
#include "Poco/Timespan.h"
#include "Poco/Thread.h"
#include "Poco/ThreadPool.h"
#include "Poco/LogStream.h"
#include "gtest/gtest.h"
#include "TaskFixture.h"

namespace k = kademlia;
namespace kd = kademlia::detail;
using Poco::Thread;
using Poco::ThreadPool;
using Poco::Timestamp;
using Poco::Timespan;
using Poco::Logger;
using Poco::LogStream;
using Poco::Net::SocketAddress;
using Session = Kademlia::Session;

namespace {

	std::atomic<int> _saved(0), _loaded (0), _errors(0);
	std::atomic<int> _savedBytes(0), _loadedBytes(0);
	std::atomic<int> _saveTime(0), _loadTime(0);

	int randomSession(int maxID)
	{
		std::random_device rd;
		std::default_random_engine eng(rd());
		std::uniform_int_distribution<int> distr(0, maxID);
		return distr(eng);
	}

	template <typename S>
	void load(S& session, std::string const& key)
	{
		std::vector<uint8_t> key_vec(key.begin(), key.end());
		Timestamp ts;
		auto on_load = [key, ts](std::error_code const& error, Session::DataType const& data)
		{
			++_loaded;
			if (error)
			{
				++_errors;
				std::cerr << "Failed to load \"" << key << "\", error: " << error.message() << std::endl;
			}
			else
			{
				Timespan elapsed = Timestamp() - ts;
				_loadTime += elapsed.microseconds();
				_loadedBytes += static_cast<int>(data.size());
				//std::cout << "Loaded \"" << key << "\" (" << data.size() << " bytes) in " << elapsed.microseconds() << " us" << std::endl;
			}
		};
		ts.update();
		session.asyncLoad(key_vec, std::move(on_load));
		//std::cout << "Async loading \"" << key << "\" ..." << std::endl;
	}

	template <typename S>
	void save(S& session, std::string const& key, std::string const& val)
	{
		std::vector<uint8_t> key_vec(key.begin(), key.end());
		std::vector<uint8_t> val_vec(val.begin(), val.end());
		std::size_t sz = val.size();
		Timestamp ts;
		auto on_save = [key, ts, sz](const std::error_code& error)
		{
			++_saved;
			if (error)
			{
				++_errors;
				std::cerr << "Failed to save \"" << key << "\", error: " << error.message() << std::endl;
			}
			else
			{
				Timespan elapsed = Timestamp() - ts;
				_saveTime += elapsed.microseconds();
				_savedBytes += static_cast<int>(sz);
				//std::cout << "Saved \"" << key << "\" (" << sz << " bytes) in " << elapsed.microseconds() << " us" << std::endl;
			}
		};
		ts.update();
		session.asyncSave(key_vec, std::move(val_vec), std::move(on_save));
		//std::cout << "Async saving \"" << key << /*"\": \"" << val <<*/ "\"" << std::endl;
	}

	template <typename S>
	void abortSession(S& sess)
	{
		// Stop the session loop
		sess.abort();

		// Wait for the session termination
		auto failure = sess.wait();
		if (failure != k::RUN_ABORTED)
			std::cerr << failure.message() << std::endl;
	}


TEST(IntegrationTest, integrationTest)
{
	try
	{
		ThreadPool& pool = ThreadPool::defaultPool();

		std::string bootAddr4 = "0.0.0.0";
		std::string bootAddr6 = "::";
		std::uint16_t bootPort4 = kd::getAvailablePort(SocketAddress::IPv4);
		std::uint16_t bootPort6 = kd::getAvailablePort(SocketAddress::IPv6);

		int peers = 3;
		pool.addCapacity(peers * 2);
		int chunks = 24, chunkSize = 50000;

		std::vector<Session*> sessions;
		sessions.push_back(new Session{ k::endpoint{bootAddr4, bootPort4}, k::endpoint{bootAddr6, bootPort6} });
		//std::cout << "bootstrap session listening on " << bootAddr4 << ':' << bootPort4 << ", " <<
		//	'[' << bootAddr6 << "]:" << bootPort6 << std::endl;

		uint16_t sessPort4 = kd::getAvailablePort(SocketAddress::IPv4, bootPort4 + 1);
		uint16_t sessPort6 = kd::getAvailablePort(SocketAddress::IPv6, bootPort6 + 1);

		for (int i = 0; i < peers; ++i)
		{
			Session* pSession = new Session{ k::endpoint{ "127.0.0.1", bootPort4 }
							, k::endpoint{ "127.0.0.1", sessPort4 }
							, k::endpoint{ "::1", sessPort6} };
			sessions.push_back(pSession);
			//std::cout << "peer session connected to 127.0.0.1:" << bootPort4 <<
			//	", listening on 127.0.0.1:" << sessPort4 << ", " <<
			//	"[::1]:" << sessPort6 << std::endl;
			sessPort4 = kd::getAvailablePort(SocketAddress::IPv4, ++sessPort4);
			sessPort6 = kd::getAvailablePort(SocketAddress::IPv6, ++sessPort6);
		}

		int i = 0;
		for (; i < chunks; ++i)
		{
			std::string k("k"), v;
			k += std::to_string(i);
			v += std::to_string(i);
			v.resize(chunkSize);
			save(*sessions[randomSession(peers - 1)], k, v);
		}
		// wait for all save ops to complete
		while (_saved < i) Thread::sleep(10);

		_loaded = 0;
		i = 0;
		for (; i < chunks; ++i)
		{
			std::string k("k");
			k += std::to_string(i);
			load(*sessions[randomSession(peers - 1)], k);
		}
		// wait for all load ops to complete
		while (_loaded < i) Thread::sleep(10);

		for (auto& pS : sessions)
		{
			abortSession(*pS);
			delete pS;
		}

		std::cout << std::endl << "Summary\n=======\n" << peers << " peers, " <<
			chunks << " chunks of " << chunkSize << " bytes\n"
			"saved " << _savedBytes << " bytes, loaded " << _loadedBytes << " bytes\n"
			"Save time:\t" << float(_saveTime) / 1000 << " [ms]\n"
			"Load time:\t" << float(_loadTime) / 1000 << " [ms]\n"
			"Total time:\t" << (float(_saveTime) + float(_loadTime)) / 1000 << " [ms]" << std::endl;

		EXPECT_EQ(_errors, 0);
		EXPECT_EQ(_savedBytes, chunks*chunkSize);
		EXPECT_EQ(_savedBytes, _loadedBytes);
	}
	catch (std::exception& ex)
	{
		std::cerr << ex.what() << std::endl;
		EXPECT_TRUE(false);
	}
	catch (...)
	{
		std::cerr << "unknown exception" << std::endl;
		EXPECT_TRUE(false);
	}
}


}

