#include <cstdint>
#include <random>
#include <atomic>
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


std::atomic<int> _saved(0), _loaded(0),
	_saveErrors(0), _loadErrors(0),
	_savedBytes(0), _loadedBytes(0),
	_saveTime(0), _loadTime(0);

LogStream _logger(Logger::get("kademlia_stress"));

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
	auto on_load = [key, ts] (std::error_code const& error, Session::DataType const& data)
	{
		++_loaded;
		if (error)
		{
			++_loadErrors;
			_logger.error() << "Failed to load \"" << key << "\", error: " << error.message() << std::endl;
		}
		else
		{
			Timespan elapsed = Timestamp() - ts;
			_loadTime += elapsed.microseconds();
			_loadedBytes += static_cast<int>(data.size());
			_logger.information() << "Loaded \"" << key << "\" (" << data.size() << " bytes) in " << elapsed.microseconds() << " us" << std::endl;
		}
	};
	ts.update();
	session.asyncLoad(key_vec, std::move(on_load));
	_logger.debug() << "Async loading \"" << key << "\" ..." << std::endl;
}

template <typename S>
void save(S& session, std::string const& key, std::string const& val)
{
	std::vector<uint8_t> key_vec(key.begin(), key.end());
	std::vector<uint8_t> val_vec(val.begin(), val.end());
	std::size_t sz = val.size();
	Timestamp ts;
	auto on_save = [key, ts, sz] (const std::error_code& error)
	{
		++_saved;
		if (error)
		{
			++_saveErrors;
			_logger.error() << "Failed to save \"" << key << "\", error: " << error.message() << std::endl;
		}
		else
		{
			Timespan elapsed = Timestamp() - ts;
			_saveTime += elapsed.microseconds();
			_savedBytes += static_cast<int>(sz);
			_logger.information() << "Saved \"" << key << "\" (" << sz << " bytes) in " << elapsed.microseconds() << " us" << std::endl;
		}
	};
	ts.update();
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

		int peers = 3;
		pool.addCapacity(peers * 2);
		int chunks = 24, chunkSize = 50000;

		std::vector<Session*> sessions;
		sessions.push_back(new Session{ k::endpoint{bootAddr4, bootPort4}, k::endpoint{bootAddr6, bootPort6} });
		_logger.information() << "bootstrap session listening on " << bootAddr4 << ':' << bootPort4 << ", " <<
			'[' << bootAddr6 << "]:" << bootPort6 << std::endl;

		uint16_t sessPort4 = kd::getAvailablePort(SocketAddress::IPv4, bootPort4 + 1);
		uint16_t sessPort6 = kd::getAvailablePort(SocketAddress::IPv6, bootPort6 + 1);

		for (int i = 0; i < peers; ++i)
		{
			Session* pSession = new Session{ k::endpoint{ "127.0.0.1", bootPort4 }
							, k::endpoint{ "127.0.0.1", sessPort4 }
							, k::endpoint{ "::1", sessPort6} };
			sessions.push_back(pSession);
			_logger.information() << "peer session connected to 127.0.0.1:" << bootPort4 <<
				", listening on 127.0.0.1:" << sessPort4 << ", " <<
				"[::1]:" << sessPort6 << std::endl;
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
			save(*sessions[randomSession(peers-1)], k, v);
		}
		// wait for all save ops to complete
		while (_saved < i) Thread::sleep(10);

		i = 0;
		for (; i < chunks; ++i)
		{
			std::string k("k");
			k += std::to_string(i);
			load(*sessions[randomSession(peers - 1)], k);
		}
		// wait for all load ops to complete
		while (_loaded < i) Thread::sleep(10);


		i = 0;
		for (; i < peers; ++i)
		{
			for (int j = 0; j < chunks; ++j)
			{
				std::string k("k");
				k += std::to_string(j);
				load(*sessions[i], k);
			}
		}
		while (_loaded < i) Thread::sleep(1);

		for (auto& pS : sessions)
		{
			abortSession(*pS);
			delete pS;
		}

		std::ostringstream ostr;
		ostr << std::endl << "Summary\n=======\n" << peers << " peers, " <<
							chunks << " chunks of " << chunkSize << " bytes\n"
							"saved " << _savedBytes << " bytes, loaded " << _loadedBytes << " bytes\n"<<
							_saveErrors << " saving errors, " << _loadErrors << " load errors\n"
							"Save time: " << float(_saveTime)/1000 << " [ms]\n"
							"Load time: " << float(_loadTime)/1000 << " [ms]\n"
							"Total time:" << (float(_saveTime)+float(_loadTime))/1000 << " [ms]" << std::endl;
		_logger.information(ostr.str());

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
