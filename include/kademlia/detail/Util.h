#ifndef KADEMLIA_DETAIL_UTIL_H
#define KADEMLIA_DETAIL_UTIL_H


#include "Poco/Net/SocketAddress.h"
#include "Poco/Net/DatagramSocket.h"

namespace kademlia {
namespace detail {


std::uint16_t getAvailablePort(Poco::Net::SocketAddress::Family family = Poco::Net::SocketAddress::IPv4,
	std::uint16_t port = 1234);
	/// Returns first available port, starting with the specified one.


} } // kademlia::detsil


#endif
