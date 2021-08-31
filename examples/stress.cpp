#include <cstdint>
#include <cstdlib>

#include <iostream>

#include "kademlia/endpoint.hpp"
#include "kademlia/session.hpp"
#include "kademlia/first_session.hpp"
#include "kademlia/error.hpp"
#include "kademlia/detail/Util.h"
#include "Poco/Timestamp.h"
#include "Poco/Timespan.h"
#include "Poco/Thread.h"
#include "Poco/ThreadPool.h"


namespace k = kademlia;
namespace kd = kademlia::detail;
using Poco::Thread;
using Poco::ThreadPool;
using Poco::Timestamp;
using Poco::Timespan;
using Poco::Net::SocketAddress;


namespace {


int _saved = 0, _loaded = 0;


template <typename S>
void load(S& session, std::string const& key)
{
	std::vector<uint8_t> key_vec(key.begin(), key.end());
	Timestamp ts;
	auto on_load = [key, ts] (std::error_code const& error, k::session::data_type const& data)
	{
		if (error)
			std::cerr << "Failed to load \"" << key << "\", error: " << error.message() << std::endl;
		else
		{
			Timespan elapsed = Timestamp() - ts;
			++_loaded;
			std::string const& str{ data.begin(), data.end() };
			std::cout << "Loaded \"" << key << "\" as \"" << str << "\" in " << elapsed.microseconds() << " us" << std::endl;
		}
	};

	session.async_load(key_vec, std::move(on_load));
	std::cout << "Async loading \"" << key << "\" ..." << std::endl;
}

template <typename S>
void save(S& session, std::string const& key, std::string const& val)
{
	std::vector<uint8_t> key_vec(key.begin(), key.end());
	std::vector<uint8_t> val_vec(val.begin(), val.end());
	Timestamp ts;
	auto on_save = [key, ts] (std::error_code const& error)
	{
		if (error)
			std::cerr << "Failed to save \"" << key << "\", error: " << error.message() << std::endl;
		else
		{
			Timespan elapsed = Timestamp() - ts;
			++_saved;
			std::cout << "Saved \"" << key << "\" in " << elapsed.microseconds() << " us" << std::endl;
		}
	};

	session.async_save(key_vec, val_vec, std::move(on_save));
	std::cout << "Async saving \"" << key << "\": \"" << val << "\"" << std::endl;
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

} // anonymous namespace

int main(int argc, char** argv)
{
	ThreadPool& pool = ThreadPool::defaultPool();

	std::string bootAddr4 = "0.0.0.0";
	std::string bootAddr6 = "::";
	std::uint16_t bootPort4 = kd::getAvailablePort(SocketAddress::IPv4);
	std::uint16_t bootPort6 = kd::getAvailablePort(SocketAddress::IPv6);

	int reps = 5;
	pool.addCapacity(reps);

	k::first_session firstSession{k::endpoint{bootAddr4, bootPort4}, k::endpoint{bootAddr6, bootPort6}};
	std::cout << "bootstrap session listening on " << bootAddr4 << ':' << bootPort4 << ", " <<
		'[' << bootAddr6 << "]:" << bootPort6 << std::endl;

	uint16_t sessPort4 = kd::getAvailablePort(SocketAddress::IPv4, bootPort4 + 1);
	uint16_t sessPort6 = kd::getAvailablePort(SocketAddress::IPv4, bootPort6 + 1);

	std::vector<k::session*> sessions;
	for (int i = 0; i < reps; ++i)
	{
		sessions.push_back(new k::session { k::endpoint{ bootAddr4, bootPort4 }
					  , k::endpoint{ bootAddr4, sessPort4 }
					  , k::endpoint{ bootAddr6, sessPort6 } });
		std::cout << "peer session connected to " << bootAddr4 << ':' << bootPort4 <<
			", listening on " << bootAddr4 << ':' << sessPort4 << ", " <<
		'[' << bootAddr6 << "]:" << sessPort6 << std::endl;
		sessPort4 = kd::getAvailablePort(SocketAddress::IPv4, sessPort4 + 1);
		sessPort6 = kd::getAvailablePort(SocketAddress::IPv4, sessPort6 + 1);
	}
	Thread::sleep(100);
	for (int i = 0; i < reps; ++i)
	{
		std::string k("k"), v("v");
		k += std::to_string(i);
		v += std::to_string(i);
		save(*sessions[0], k, v);
		while (_saved < i+1) Thread::sleep(1);
	}

	_loaded = 0;
	for (int i = 0; i < reps; ++i)
	{
		std::string k("k");
		k += std::to_string(i);
		load(*sessions[i], k);
		while (_loaded < i+1) Thread::sleep(1);
	}

	_loaded = 0;
	for (int i = 0; i < reps; ++i)
	{
		std::string k("k");
		k += std::to_string(i);
		load(firstSession, k);
		while (_loaded < i+1) Thread::sleep(1);
	}

	abortSession(firstSession);
	_loaded = 0;
	for (int i = 0; i < reps; ++i)
	{
		std::string k("k");
		k += std::to_string(i);
		load(*sessions[i], k);
		while (_loaded < i+1) Thread::sleep(1);
	}

	for (auto& pS : sessions)
		abortSession(*pS);

	std::cout << "Goodbye!" << std::endl;
}
