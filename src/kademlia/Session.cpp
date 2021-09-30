//
// Session.h
//
// Library: Kademlia
// Package: DHT
// Module:  Session
//
// Definition of the Session class.
//
// Copyright (c) 2021, Aleph ONE Software Engineering and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#include "kademlia/Session.h"
#include "kademlia/error.hpp"
#include "error_impl.hpp"
#include "SocketAdapter.h"
#include "Engine.h"
#include "Poco/Timespan.h"

namespace Kademlia {

using Endpoint = Session::Endpoint;

class EngineImpl
{
public:
	using SocketType = kademlia::detail::SocketAdapter<Poco::Net::DatagramSocket>;
	using EngineType = kademlia::detail::Engine<SocketType>;

	EngineImpl(Poco::Net::SocketProactor& ioService, Endpoint const& ipv4, Endpoint const& ipv6):
		_engine(ioService, ipv4, ipv6)
	{}

	EngineImpl(Poco::Net::SocketProactor& ioService, Endpoint const& initPeer, Endpoint const& ipv4, Endpoint const& ipv6):
		_engine(ioService, initPeer, ipv4, ipv6)
	{}

	EngineType& engine()
	{
		return _engine;
	}

private:
	EngineType _engine;
};


const std::uint16_t Session::DEFAULT_PORT = 27980;


Session::Session(Endpoint const& ipv4, Endpoint const& ipv6, int ms)
try:
	_runMethod(this, &Kademlia::Session::run),
	_ioService(Poco::Timespan(Poco::Timespan::TimeDiff(ms)*1000)),
	_pEngine(new EngineImpl(_ioService, ipv4, ipv6))
	{
		result();
	}
catch (std::exception& ex)
{
	std::cerr << ex.what() << std::endl;
	throw;
}
catch (...)
{
	std::cerr << "unknown exception" << std::endl;
	throw;
}


Session::Session(Endpoint const& initPeer, Endpoint const& ipv4, Endpoint const& ipv6, int ms)
try:
	_runMethod(this, &Kademlia::Session::run),
	_ioService(Poco::Timespan(Poco::Timespan::TimeDiff(ms)*1000)),
	_pEngine(new EngineImpl(_ioService, initPeer, ipv4, ipv6))
	{
		result();
	}
catch (std::exception& ex)
{
	std::cerr << ex.what() << std::endl;
	throw;
}
catch (...)
{
	std::cerr << "unknown exception" << std::endl;
	throw;
}


Session::~Session()
{
	abort();
	wait();
}


std::error_code Session::run()
{
	Poco::FastMutex::ScopedLock l(_mutex);
	try
	{
		_ioService.run();
	}
	catch (std::exception& ex)
	{
		std::cerr << ex.what() << std::endl;
	}
	return kademlia::detail::make_error_code(kademlia::RUN_ABORTED);
}


void Session::abort()
{
	_ioService.stop();
	_ioService.wakeUp();
}


std::error_code Session::wait()
{
	result().wait();
	return result().data();
}

void Session::asyncSave(KeyType const& key, DataType&& data, SaveHandlerType&& handler)
{
	_pEngine->engine().asyncSave(key, std::move(data), std::move(handler));
}

void Session::asyncLoad(KeyType const& key, LoadHandlerType handler )
{
	_pEngine->engine().asyncLoad(key, std::move(handler));
}


} // namespace kademlia
