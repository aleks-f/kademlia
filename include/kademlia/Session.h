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

#ifndef KADEMLIA_SESSION_H
#define KADEMLIA_SESSION_H


#include <utility>
#include "Poco/ActiveMethod.h"
#include "Poco/Mutex.h"
#include "Poco/Net/DatagramSocket.h"
#include "Poco/Net/SocketProactor.h"
#include "kademlia/endpoint.hpp"


namespace Kademlia {

class EngineImpl;

class KADEMLIA_SYMBOL_VISIBILITY Session
{
public:
	using DataType = std::vector<std::uint8_t>;
	using KeyType = std::vector<std::uint8_t>;
	using Endpoint = kademlia::endpoint;
	using SaveHandlerType = std::function<void (const std::error_code&)>;
	using LoadHandlerType = std::function<void (const std::error_code&, const DataType& data)>;

	static const std::uint16_t DEFAULT_PORT;

	Session(Endpoint const& ipv4 = {"0.0.0.0", DEFAULT_PORT},
		Endpoint const& ipv6 = {"::", DEFAULT_PORT}, int ms = 300);

	Session(Endpoint const& initPeer, Endpoint const& ipv4, Endpoint const& ipv6, int ms = 300);

	~Session();

	void asyncSave(KeyType const& key, DataType const& data, SaveHandlerType handler);

	template<typename K, typename D>
	void asyncSave(K const& key, D const& data, SaveHandlerType && handler)
	{
		asyncSave(KeyType(std::begin(key), std::end(key)),
			DataType(std::begin(data), std::end(data)), std::move(handler));
	}

	void asyncLoad(KeyType const& key, LoadHandlerType handler );

	template<typename K>
	void asyncLoad(K const& key, LoadHandlerType && handler)
	{
		asyncLoad(KeyType(std::begin(key), std::end(key)), std::move(handler));
	}

	std::error_code run();

	void abort();
	std::error_code wait();

private:
	using RunType = Poco::ActiveMethod<std::error_code, void, Session>;
	using Result = Poco::ActiveResult<std::error_code>;
	using ResultPtr = std::unique_ptr<Result>;

	Result& result()
	{
		if (!_pResult)
			_pResult.reset(new Result(this->_runMethod()));
		return *_pResult;
	}

	RunType _runMethod;
	Poco::Net::SocketProactor _ioService;
	std::unique_ptr<EngineImpl> _pEngine;
	ResultPtr _pResult;
	Poco::FastMutex _mutex;
};

} // namespace kademlia

#endif

