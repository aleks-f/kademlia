
#ifndef KADEMLIA_SOCKETADAPTER_H
#define KADEMLIA_SOCKETADAPTER_H


#include "Poco/Net/DatagramSocket.h"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/SocketNotification.h"
#include "kademlia/buffer.hpp"
#include "kademlia/boost_to_std_error.hpp"
#include <boost/asio/buffer.hpp>

namespace kademlia {
namespace detail {


template <typename SocketType>
class SocketAdapter
{
public:
	using ReadableObserver = Poco::Observer<SocketAdapter, Poco::Net::ReadableNotification>;
	using WritableObserver = Poco::Observer<SocketAdapter, Poco::Net::WritableNotification>;
	using Callback = std::function<void (boost::system::error_code const& failure, std::size_t bytes_received)>;

	SocketAdapter() = delete;

	SocketAdapter(Poco::Net::SocketReactor* pIOService,
		const Poco::Net::SocketAddress& addr,
		bool reuseAddress, bool ipV6Only) :
		_socket(addr, reuseAddress, ipV6Only),
		_pIOService(pIOService),
		_readHandler(*this, &SocketAdapter::onReadable),
		_writeHandler(*this, &SocketAdapter::onWritable)
	{
		_pIOService->addEventHandler(_socket, _readHandler);
		_pIOService->addEventHandler(_socket, _writeHandler);
	}

	SocketAdapter(const SocketAdapter& other) :
		_socket(other._socket),
		_pIOService(other._pIOService),
		_readHandler(*this, &SocketAdapter::onReadable),
		_writeHandler(*this, &SocketAdapter::onWritable),
		_pRecvBuf(other._pRecvBuf),
		_pRecvAddr(other._pRecvAddr),
		_onRecvCompletion(other._onRecvCompletion),
		_pSendBuf(other._pSendBuf),
		_pSendAddr(other._pSendAddr),
		_onSendCompletion(other._onSendCompletion)
	{
		_pIOService->addEventHandler(_socket, _readHandler);
		_pIOService->addEventHandler(_socket, _writeHandler);
	}

	SocketAdapter(SocketAdapter&& other) :
		_pIOService(other._pIOService),
		_readHandler(*this, &SocketAdapter::onReadable),
		_writeHandler(*this, &SocketAdapter::onWritable),
		_pRecvBuf(std::move(other._pRecvBuf)),
		_pRecvAddr(std::move(other._pRecvAddr)),
		_onRecvCompletion(std::move(other._onRecvCompletion)),
		_pSendBuf(std::move(other._pSendBuf)),
		_pSendAddr(std::move(other._pSendAddr)),
		_onSendCompletion(std::move(other._onSendCompletion))
	{
		_pIOService->removeEventHandler(other._socket, other._writeHandler);
		_pIOService->removeEventHandler(other._socket, other._readHandler);
		_socket = std::move(other._socket);

		other._pRecvBuf = nullptr;
		other._pRecvAddr = nullptr;
		other._onRecvCompletion = nullptr;

		other._pSendBuf = nullptr;
		other._pSendAddr = nullptr;
		other._onSendCompletion = nullptr;
		
		_pIOService->addEventHandler(_socket, _readHandler);
		_pIOService->addEventHandler(_socket, _writeHandler);
	}

	~SocketAdapter()
	{
		if (_socket.impl())
		{
			_pIOService->removeEventHandler(_socket, _writeHandler);
			_pIOService->removeEventHandler(_socket, _readHandler);
		}
	}

	SocketAdapter &operator=(const SocketAdapter &other)
	{
		_socket = other._socket;
		_pIOService = other._pIOService;

		_readHandler = ReadableObserver(*this, &SocketAdapter::onReadable);
		_writeHandler = WritableObserver(*this, &SocketAdapter::onWritable);

		_pRecvBuf = other._pRecvBuf;
		_pRecvAddr = other._pRecvAddr;
		_onRecvCompletion = other._onRecvCompletion;

		_pSendBuf = other._pSendBuf;
		_pSendAddr = other._pSendAddr;
		_onSendCompletion = other._onSendCompletion;

		return *this;
	}

	SocketAdapter &operator=(SocketAdapter &&other)
	{
		_pIOService->removeEventHandler(other._socket, other._writeHandler);
		_pIOService->removeEventHandler(other._socket, other._readHandler);
		_socket = std::move(other._socket);

		_pIOService = other._pIOService;
		_readHandler = ReadableObserver(*this, &SocketAdapter::onReadable);
		_writeHandler = WritableObserver(*this, &SocketAdapter::onWritable);

		_pRecvBuf = std::move(other._pRecvBuf);
		_pRecvAddr = std::move(other._pRecvAddr);
		_onRecvCompletion = std::move(other._onRecvCompletion);

		_pSendBuf = std::move(other._pSendBuf);
		_pSendAddr = std::move(other._pSendAddr);
		_onSendCompletion = std::move(other._onSendCompletion);

		other._pRecvBuf = nullptr;
		other._pRecvAddr = nullptr;
		other._onRecvCompletion = nullptr;

		return *this;
	}

	void onReadable(Poco::Net::ReadableNotification* pNf)
	{
		poco_check_ptr (_pRecvBuf);
		poco_check_ptr(_pRecvAddr);
		int n = _socket.receiveFrom(_pRecvBuf, _pRecvBuf->size(), *_pRecvAddr);
		//TODO: pass error
		_onRecvCompletion(boost::system::error_code(), n);
	}

	void onWritable(Poco::Net::WritableNotification* pNf)
	{
		poco_check_ptr (_pSendBuf);
		poco_check_ptr(_pSendAddr);
		int n = _socket.sendTo(_pSendBuf, _pSendBuf->size(), *_pSendAddr);
		//TODO: pass error
		_onSendCompletion(boost::system::error_code(), n);
	}

	void asyncReceiveFrom(boost::asio::mutable_buffer const& buf, Poco::Net::SocketAddress& addr, Callback&& onCompletion)
	{
		_pRecvBuf = const_cast<boost::asio::mutable_buffer*>(std::addressof(buf));
		_pRecvAddr = std::addressof(addr);
		_onRecvCompletion = std::move(onCompletion);
	}

	void asyncSendTo(boost::asio::const_buffer const& buf, const Poco::Net::SocketAddress& addr, Callback&& onCompletion)
	{
		_pSendBuf = const_cast<boost::asio::const_buffer*>(std::addressof(buf));
		_pSendAddr = std::addressof(addr);
		_onSendCompletion = std::move(onCompletion);
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
	Poco::Net::SocketReactor* _pIOService = nullptr;

	ReadableObserver _readHandler;
	WritableObserver _writeHandler;

	boost::asio::mutable_buffer* _pRecvBuf = nullptr;
	Poco::Net::SocketAddress* _pRecvAddr = nullptr;
	Callback _onRecvCompletion = nullptr;

	boost::asio::const_buffer* _pSendBuf = nullptr;
	const Poco::Net::SocketAddress* _pSendAddr = nullptr;
	Callback _onSendCompletion = nullptr;
};

} }

#endif //KADEMLIA_SOCKETADAPTER_H
