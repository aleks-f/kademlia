#include <cstdint>
#include <cstdlib>

#include <future>
#include <iostream>
#include <iterator>
#include <sstream>

#include <kademlia/endpoint.hpp>
#include <kademlia/error.hpp>
#include "kademlia/Session.h"

using Session = Kademlia::Session;
using Endpoint = kademlia::endpoint;

int main(int argc, char** argv )
{
	// Check command line arguments count
	if ( argc != 2 )
	{
		std::cerr << "usage: " << argv[0] << " <PORT>" << std::endl;
		return EXIT_FAILURE;
	}

	// Parse command line arguments
	std::uint16_t const port = std::atoi( argv[1] );

	// Create the session
	Session session{Endpoint{"0.0.0.0", port}, Endpoint{"::", port}};

	// Wait for exit request
	std::cout << "Press any key to exit" << std::endl;
	std::cin.get();

	// Stop the main loop thread
	session.abort();

	// Wait for the main loop thread termination
	auto failure = session.wait();
	if ( failure != kademlia::RUN_ABORTED )
		std::cerr << failure.message() << std::endl;
}
