
#ifndef KADEMLIA_SOCKETADAPTER_H
#define KADEMLIA_SOCKETADAPTER_H


#include "Poco/Net/DatagramSocket.h"
#include "Poco/Net/SocketProactor.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/SocketNotification.h"
#include "Poco/Thread.h"
#include "kademlia/buffer.hpp"
#include "kademlia/log.hpp"
#include "Poco/ScopedLock.h"
#include "Poco/Logger.h"


namespace kademlia {
namespace detail {


template <typename SocketType>
class SocketAdapter
{
public:
	using Callback = std::function<void (std::error_code const& failure, std::size_t bytes_received)>;

	SocketAdapter() = delete;

	SocketAdapter(Poco::Net::SocketProactor* pIOService,
		const Poco::Net::SocketAddress& addr,
		bool reuseAddress, bool ipV6Only) :
		_socket(addr, reuseAddress, ipV6Only),
		_pIOService(pIOService)
	{
		LOG_DEBUG(SocketAdapter, this) << "Created SocketAdapter for " << _socket.address().toString() << std::endl;
		_pIOService->addSocket(_socket, 3);
	}

	SocketAdapter(const SocketAdapter& other) :
		_socket(other._socket),
		_pIOService(other._pIOService)
	{
	}

	SocketAdapter(SocketAdapter&& other) :
		_pIOService(other._pIOService)
	{
		LOG_DEBUG(SocketAdapter, this) << "Moving SocketAdapter for " << other._socket.address().toString() << std::endl;
		_socket = std::move(other._socket);
		other._pIOService = nullptr;
	}

	~SocketAdapter()
	{
		if (_socket.impl())
		{
			LOG_DEBUG(SocketAdapter, this) << "Destroyed SocketAdapter for " << _socket.address().toString() << std::endl;
		}
		else LOG_DEBUG(SocketAdapter, this) << "Destroyed SocketAdapter" << std::endl;
	}

	SocketAdapter &operator=(const SocketAdapter &other)
	{
		_socket = other._socket;
		_pIOService = other._pIOService;
		return *this;
	}

	SocketAdapter &operator=(SocketAdapter &&other)
	{
		_socket = std::move(other._socket);
		_pIOService = other._pIOService;
		other._pIOService = nullptr;
		return *this;
	}

	void asyncReceiveFrom(buffer& buf, Poco::Net::SocketAddress& addr, Callback&& onCompletion)
	{
		_pIOService->addReceiveFrom(_socket, buf, addr, std::move(onCompletion));
		_pIOService->wakeUp();
	}

	void asyncSendTo(const buffer& message, const Poco::Net::SocketAddress& addr, Callback&& onCompletion)
	{
		_pIOService->addSendTo(_socket, message, addr, std::move(onCompletion));
		_pIOService->wakeUp();
	}

	void asyncSendTo(buffer&& message, Poco::Net::SocketAddress&& addr, Callback&& onCompletion)
	{
		_pIOService->addSendTo(_socket, std::move(message), std::move(addr), std::move(onCompletion));
		_pIOService->wakeUp();
	}

	Poco::Net::SocketImpl* impl() const
	{
		return _socket.impl();
	}

	const Poco::Net::Socket& socket() const
	{
		return _socket;
	}

	Poco::Net::SocketAddress address() const
	{
		return _socket.address();
	}

private:
	Poco::Net::DatagramSocket _socket;
	Poco::Net::SocketProactor* _pIOService = nullptr;
};

} }

#endif //KADEMLIA_SOCKETADAPTER_H
