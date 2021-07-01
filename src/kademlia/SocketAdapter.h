
#ifndef KADEMLIA_SOCKETADAPTER_H
#define KADEMLIA_SOCKETADAPTER_H


#include "Poco/Net/DatagramSocket.h"
#include "Poco/Net/SocketReactor.h"
#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/SocketNotification.h"
#include "Poco/Thread.h"
#include "kademlia/buffer.hpp"
#include "kademlia/boost_to_std_error.hpp"
#include "kademlia/log.hpp"
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
		kademlia::detail::enable_log_for("SocketAdapter");
		LOG_DEBUG(SocketAdapter, this) << "Created SocketAdapter for " << _socket.address().toString() << std::endl;
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
		_sendBuf(other._sendBuf),
		_sendAddr(other._sendAddr),
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
		_sendBuf(std::move(other._sendBuf)),
		_sendAddr(std::move(other._sendAddr)),
		_onSendCompletion(std::move(other._onSendCompletion))
	{
		LOG_DEBUG(SocketAdapter, this) << "Moving SocketAdapter for " << other._socket.address().toString() << std::endl;
		_pIOService->removeEventHandler(other._socket, other._writeHandler);
		_pIOService->removeEventHandler(other._socket, other._readHandler);
		_socket = std::move(other._socket);

		other._pRecvBuf = nullptr;
		other._pRecvAddr = nullptr;
		other._onRecvCompletion = nullptr;

		other._sendBuf.clear();
		other._sendAddr = Poco::Net::SocketAddress();
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
			LOG_DEBUG(SocketAdapter, this) << "Destroyed SocketAdapter for " << _socket.address().toString() << std::endl;
		}
		else LOG_DEBUG(SocketAdapter, this) << "Destroyed SocketAdapter" << std::endl;
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

		_sendBuf = other._sendBuf;
		_sendAddr = other._sendAddr;
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

		_sendBuf = std::move(other._sendBuf);
		_sendAddr = std::move(other._sendAddr);
		_onSendCompletion = std::move(other._onSendCompletion);

		other._pRecvBuf = nullptr;
		other._pRecvAddr = nullptr;
		other._onRecvCompletion = nullptr;

		return *this;
	}

	void onReadable(Poco::Net::ReadableNotification* pNf)
	{
		pNf->release();
		if (_pRecvBuf && _pRecvAddr)
		{
			int avail = _socket.available();
			int n = _socket.receiveFrom(&(*_pRecvBuf)[0], avail, *_pRecvAddr);
			LOG_DEBUG(SocketAdapter, this) << '[' << _socket.address().toString() << "]"
				"<===" << n << " bytes===[" << _pRecvAddr->toString() << ']' << std::endl;
			//TODO: pass error
			_onRecvCompletion(boost::system::error_code(), n);
		}
	}

	void onWritable(Poco::Net::WritableNotification* pNf)
	{
		pNf->release();
		if (_sendBuf.size()/* && _pSendAddr*/)
		{
			int n = _socket.sendTo(&_sendBuf[0], _sendBuf.size(), _sendAddr);
			LOG_DEBUG(SocketAdapter, this) << '[' << _socket.address().toString() << "]"
				"===" << n << " bytes===>[" << _sendAddr.toString() << ']' << std::endl;
			//TODO: pass error
			_onSendCompletion(boost::system::error_code(), n);
			_sendBuf.clear();
			//_sendAddr = nullptr;
		}
		else Poco::Thread::sleep(100);
	}

	void asyncReceiveFrom(buffer& buf, Poco::Net::SocketAddress& addr, Callback&& onCompletion)
	{
		_pRecvBuf = std::addressof(buf);
		_pRecvAddr = std::addressof(addr);
		_onRecvCompletion = std::move(onCompletion);
		LOG_DEBUG(SocketAdapter, this) << "asyncRecvFrom(" << _pRecvBuf << ", " <<
			_pRecvAddr->toString() << ", " << (_onRecvCompletion?1:0) << ')' <<
		std::endl;
	}

	void asyncSendTo(const buffer& message, const Poco::Net::SocketAddress& addr, Callback&& onCompletion)
	{
		_sendBuf = message;
		_sendAddr = addr;
		_onSendCompletion = std::move(onCompletion);
		LOG_DEBUG(SocketAdapter, this) << "asyncSendTo(" << _sendBuf.size() << ", " <<
			_sendAddr.toString() << ", " << (_onSendCompletion?1:0) << ')' <<
			std::endl;
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

	buffer* _pRecvBuf = nullptr;
	Poco::Net::SocketAddress* _pRecvAddr = nullptr;
	Callback _onRecvCompletion = nullptr;

	buffer _sendBuf;
	Poco::Net::SocketAddress _sendAddr;
	Callback _onSendCompletion = nullptr;
};

} }

#endif //KADEMLIA_SOCKETADAPTER_H
