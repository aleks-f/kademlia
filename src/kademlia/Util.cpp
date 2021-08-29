#include "kademlia/detail/Util.h"
#include "Poco/Net/DatagramSocket.h"
#include "Poco/Net/NetException.h"


using Poco::Net::IPAddress;
using Poco::Net::SocketAddress;
using Poco::Net::DatagramSocket;
using Poco::Net::Socket;
using Poco::Net::NetException;


namespace kademlia {
namespace detail {


std::uint16_t getAvailablePort(SocketAddress::Family family, std::uint16_t port)
{
	bool failed = false;
	poco_assert_dbg(port > 0);
	--port;
	DatagramSocket sock(family);
	do
	{
		failed = false;
		SocketAddress sa(family, ++port);
		try
		{
			if (family == SocketAddress::IPv4)
				sock.bind(sa, false);
			else if (family == SocketAddress::IPv6)
				sock.bind6(sa, false, false, true);
			else
				throw Poco::InvalidArgumentException("getTemporaryListeningPort: unknown address family");
		}
		catch(NetException&)
		{
			failed = true;
		}
	}
	while (failed &&
		(
			(sock.getError() == POCO_EADDRINUSE) ||
			(sock.getError() == POCO_EACCES) ||
			(Socket::lastError() == POCO_EADDRINUSE) ||
			(Socket::lastError() == POCO_EACCES)
		)
	);
	return port;
}


} // namespace detail
} // namespace kademlia
