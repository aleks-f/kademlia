#include <cstdint>
#include <cstdlib>

#include <iostream>

#include "kademlia/endpoint.hpp"
#include "kademlia/Session.h"
#include "kademlia/error.hpp"
#include "kademlia/detail/Util.h"
#include "Poco/Timestamp.h"
#include "Poco/Timespan.h"
#include "Poco/Thread.h"
#include "Poco/ThreadPool.h"
#include "Poco/LogStream.h"


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


int _saved = 0, _loaded = 0;

LogStream _logger(Logger::get("kademlia_stress"));

template <typename S>
void load(S& session, std::string const& key)
{
	std::vector<uint8_t> key_vec(key.begin(), key.end());
	Timestamp ts;
	auto on_load = [key, ts] (std::error_code const& error, Session::DataType const& data)
	{
		if (error)
			_logger.error() << "Failed to load \"" << key << "\", error: " << error.message() << std::endl;
		else
		{
			Timespan elapsed = Timestamp() - ts;
			++_loaded;
			std::string const& str{ data.begin(), data.end() };
			_logger.information() << "Loaded \"" << key << /*"\" as \"" << str << */"\" in " << elapsed.microseconds() << " us" << std::endl;
		}
	};

	session.asyncLoad(key_vec, std::move(on_load));
	_logger.debug() << "Async loading \"" << key << "\" ..." << std::endl;
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
			_logger.error() << "Failed to save \"" << key << "\", error: " << error.message() << std::endl;
		else
		{
			Timespan elapsed = Timestamp() - ts;
			++_saved;
			_logger.information() << "Saved \"" << key << "\" in " << elapsed.microseconds() << " us" << std::endl;
		}
	};

	session.asyncSave(key_vec, std::move(val_vec), std::move(on_save));
	_logger.debug() << "Async saving \"" << key << /*"\": \"" << val <<*/ "\"" << std::endl;
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
	try
	{
		ThreadPool& pool = ThreadPool::defaultPool();

		std::string bootAddr4 = "0.0.0.0";
		std::string bootAddr6 = "::";
		std::uint16_t bootPort4 = kd::getAvailablePort(SocketAddress::IPv4);
		std::uint16_t bootPort6 = kd::getAvailablePort(SocketAddress::IPv6);

		int reps = 5;
		pool.addCapacity(reps*2);

		Session firstSession{ k::endpoint{bootAddr4, bootPort4}, k::endpoint{bootAddr6, bootPort6} };
		_logger.information() << "bootstrap session listening on " << bootAddr4 << ':' << bootPort4 << ", " <<
			'[' << bootAddr6 << "]:" << bootPort6 << std::endl;

		uint16_t sessPort4 = kd::getAvailablePort(SocketAddress::IPv4, bootPort4 + 1);
		uint16_t sessPort6 = kd::getAvailablePort(SocketAddress::IPv6, bootPort6 + 1);

		std::vector<Session*> sessions;
		for (int i = 0; i < reps; ++i)
		{
			sessions.push_back(new Session{ k::endpoint{ "127.0.0.1", bootPort4 }
						  , k::endpoint{ "127.0.0.1", sessPort4 }
						  , k::endpoint{ "::1", sessPort6} });
			_logger.information() << "peer session connected to 127.0.0.1:" << bootPort4 <<
				", listening on 127.0.0.1:" << sessPort4 << ", " <<
				"[::1]:" << sessPort6 << std::endl;
			sessPort4 = kd::getAvailablePort(SocketAddress::IPv4, ++sessPort4);
			sessPort6 = kd::getAvailablePort(SocketAddress::IPv6, ++sessPort6);
		}
		Thread::sleep(100);
		for (int i = 0; i < reps; ++i)
		{
			std::string k("k"), v(65000, 'v');
			k += std::to_string(i);
			v += std::to_string(i);
			save(*sessions[0], k, v);
			while (_saved < i + 1) Thread::sleep(1);
		}

		_loaded = 0;
		for (int i = 0; i < reps; ++i)
		{
			std::string k("k");
			k += std::to_string(i);
			load(*sessions[i], k);
			while (_loaded < i + 1) Thread::sleep(1);
		}

		_loaded = 0;
		for (int i = 0; i < reps; ++i)
		{
			std::string k("k");
			k += std::to_string(i);
			load(firstSession, k);
			while (_loaded < i + 1) Thread::sleep(1);
		}

		abortSession(firstSession);
		_loaded = 0;
		for (int i = 0; i < reps; ++i)
		{
			std::string k("k");
			k += std::to_string(i);
			load(*sessions[i], k);
			while (_loaded < i + 1) Thread::sleep(1);
		}

		for (auto& pS : sessions)
			abortSession(*pS);

		std::cout << "Goodbye!" << std::endl;
		return 0;
	}
	catch (std::exception& ex)
	{
		std::cerr << ex.what() << ", quitting." << std::endl;
	}
	catch (...)
	{
		std::cerr << "unknown exception, quitting" << std::endl;
	}
	return -1;
}
