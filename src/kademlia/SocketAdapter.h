
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
#include "Poco/ScopedLock.h"
#include "Poco/Logger.h"


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
		_recvBuf(other._recvBuf),
		_recvAddr(other._recvAddr),
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
		_recvBuf(std::move(other._recvBuf)),
		_recvAddr(std::move(other._recvAddr)),
		_onRecvCompletion(std::move(other._onRecvCompletion)),
		_sendBuf(std::move(other._sendBuf)),
		_sendAddr(std::move(other._sendAddr)),
		_onSendCompletion(std::move(other._onSendCompletion))
	{
		LOG_DEBUG(SocketAdapter, this) << "Moving SocketAdapter for " << other._socket.address().toString() << std::endl;
		_pIOService->removeEventHandler(other._socket, other._writeHandler);
		_pIOService->removeEventHandler(other._socket, other._readHandler);
		_socket = std::move(other._socket);

		other._recvBuf.clear();
		other._recvAddr.clear();
		other._onRecvCompletion.clear();

		other._sendBuf.clear();
		other._sendAddr.clear();
		other._onSendCompletion.clear();
		
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

		_recvBuf = other._pRecvBuf;
		_recvAddr = other._pRecvAddr;
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
		other._pIOService = nullptr;

		_readHandler = ReadableObserver(*this, &SocketAdapter::onReadable);
		_writeHandler = WritableObserver(*this, &SocketAdapter::onWritable);

		_recvBuf = std::move(other._recvBuf);
		_recvAddr = std::move(other._recvAddr);
		_onRecvCompletion = std::move(other._onRecvCompletion);

		other._recvBuf.clear();
		other._recvAddr.clear();
		other._onRecvCompletion.clear();

		_sendBuf = std::move(other._sendBuf);
		_sendAddr = std::move(other._sendAddr);
		_onSendCompletion = std::move(other._onSendCompletion);

		other._sendBuf.clear();
		other._sendAddr.clear();
		other._onSendCompletion.clear();

		return *this;
	}

	void onReadable(Poco::Net::ReadableNotification* pNf)
	{
		pNf->release();
		Poco::ScopedLockWithUnlock<Poco::FastMutex> l(_recvMutex);
		if (_recvBuf.size())
		{
			buffer* pRecvBuf = _recvBuf.front();
			_recvBuf.pop_front();
			Poco::Net::SocketAddress* pRecvAddr = _recvAddr.front();
			_recvAddr.pop_front();
			Callback onRecvCompletion = _onRecvCompletion.front();
			_onRecvCompletion.pop_front();
			l.unlock();

			int avail = _socket.available();
			if (pRecvBuf->size() < avail) pRecvBuf->resize(avail);
			int n = _socket.receiveFrom(&(*pRecvBuf)[0], avail, *pRecvAddr);
			LOG_DEBUG(SocketAdapter, this) << '[' << _socket.address().toString() << "]"
				"<===" << n << " bytes===[" << pRecvAddr->toString() << ']' << std::endl;
/*
			std::string msg;
			Poco::Logger::formatDump(msg,&(*pRecvBuf)[0], n);
			std::cout << msg << std::endl;
*/
			//TODO: pass error
			onRecvCompletion(boost::system::error_code(), n);
		}
	}

	void onWritable(Poco::Net::WritableNotification* pNf)
	{
		pNf->release();
		Poco::ScopedLockWithUnlock<Poco::FastMutex> l(_sendMutex);
		if (_sendBuf.size())
		{
			buffer buf = _sendBuf.front();
			if (buf.size())
			{
				Poco::Net::SocketAddress addr = _sendAddr.front();
				Callback onSendCompletion = _onSendCompletion.front();
				_sendBuf.pop_front();
				_sendAddr.pop_front();
				_onSendCompletion.pop_front();
				l.unlock();

				int n = _socket.sendTo(&buf[0], buf.size(), addr);
				LOG_DEBUG(SocketAdapter, this) << '[' << _socket.address().toString() << "]"
					"===" << n << " bytes===>[" << addr.toString() << ']' << std::endl;
/*
				std::string msg;
				Poco::Logger::formatDump(msg, &buf[0], n);
				std::cout << msg << std::endl;
*/
				//TODO: pass error
				onSendCompletion(boost::system::error_code(), n);
				return;
			}
			else
			{
				// TODO: should there be a warning or error here?
				_sendBuf.pop_front();
				_sendAddr.pop_front();
				_onSendCompletion.pop_front();
				l.unlock();
			}
		}
		else Poco::Thread::sleep(100);
	}

	void asyncReceiveFrom(buffer& buf, Poco::Net::SocketAddress& addr, Callback&& onCompletion)
	{
		Poco::FastMutex::ScopedLock l(_recvMutex);
		_recvBuf.push_back(std::addressof(buf));
		_recvAddr.push_back(std::addressof(addr));
		_onRecvCompletion.push_back(std::move(onCompletion));
		LOG_DEBUG(SocketAdapter, this) << "asyncRecvFrom(" << &buf << ", " << addr.toString() << ')' <<
		std::endl;
	}

	void asyncSendTo(const buffer& message, const Poco::Net::SocketAddress& addr, Callback&& onCompletion)
	{
		Poco::FastMutex::ScopedLock l(_sendMutex);
		_sendBuf.push_back(message);
		_sendAddr.push_back(addr);
		_onSendCompletion.push_back(std::move(onCompletion));
		LOG_DEBUG(SocketAdapter, this) << "asyncSendTo(" << message.size() << ", " <<
			addr.toString() << ')' << std::endl;
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

	std::deque<buffer*> _recvBuf;
	std::deque<Poco::Net::SocketAddress*> _recvAddr;
	std::deque<Callback> _onRecvCompletion;

	std::deque<buffer> _sendBuf;
	std::deque<Poco::Net::SocketAddress> _sendAddr;
	std::deque<Callback> _onSendCompletion;

	Poco::FastMutex _sendMutex;
	Poco::FastMutex _recvMutex;
};

} }

#endif //KADEMLIA_SOCKETADAPTER_H
