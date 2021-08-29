#include <cstdint>
#include <cstdlib>

#include <future>
#include <iostream>
#include <iterator>
#include <thread>

#include "kademlia/endpoint.hpp"
#include "kademlia/session.hpp"
#include "kademlia/first_session.hpp"
#include "kademlia/error.hpp"
#include "kademlia/detail/Util.h"
#include "Poco/NumberParser.h"
#include "Poco/Thread.h"
#include "Poco/ThreadPool.h"


namespace k = kademlia;
namespace kd = kademlia::detail;
using Poco::Thread;
using Poco::ThreadPool;


namespace {


int saved = 0, loaded = 0;

const char HELP[] =
"save <KEY> <VALUE>\n\tSave <VALUE> as <KEY>\n\n"
"load <KEY>\n\tLoad value associated with <KEY>\n\n"
"exit\n\tExit the program\n\n"
"help\n\tPrint this message\n\n";

std::vector<std::string> split(std::string const& line)
{
	std::istringstream in{ line };

	using iterator = std::istream_iterator<std::string>;
	return std::vector<std::string>{ iterator{ in }, iterator{} };
}

template <typename S>
void load(S& session, std::string const& key)
{
	std::vector<uint8_t> key_vec(key.begin(), key.end());
	auto on_load = [key] (std::error_code const& error, k::session::data_type const& data)
	{
		if (error)
			std::cerr << "Failed to load \"" << key << "\", error: " << error.message() << std::endl;
		else
		{
			++loaded;
			std::string const& str{ data.begin(), data.end() };
			std::cout << "Loaded \"" << key << "\" as \"" << str << "\"" << std::endl;
		}
	};

	session.async_load(key_vec, std::move(on_load));
	std::cout << "Async loading \"" << key << "\" ..." << std::endl;
}

template <typename S>
void save(S& session, std::string const& key, std::string const& val)
{
	std::vector<uint8_t> key_vec(key.begin(), key.end());
	std::vector<uint8_t> val_vec(val.begin(), val.end());
	auto on_save = [key] (std::error_code const& error)
	{
		if (error)
			std::cerr << "Failed to save \"" << key << "\", error: " << error.message() << std::endl;
		else
		{
			++saved;
			std::cout << "Saved \"" << key << "\"" << std::endl;
		}
	};

	session.async_save(key_vec, val_vec, std::move(on_save));
	std::cout << "Async saving \"" << key << "\": \"" << val << "\"" << std::endl;
}
/*
void print_interactive_help()
{
	std::cout << HELP << std::flush;
}
*/
template <typename S>
void abortSession(S& sess)
{
	// Stop the session loop
	sess.abort();

	// Wait for the session termination
	auto failure = sess.wait();
	if (failure != k::RUN_ABORTED)
		std::cerr << failure.message() << std::endl;
	std::cout << " session aborted" << std::endl;
}

} // anonymous namespace

int main(int argc, char** argv)
{
	ThreadPool& pool = ThreadPool::defaultPool();

	std::string bootAddr4 = "0.0.0.0";
	std::string bootAddr6 = "::";
	std::string port = "1234";
	if (argc >= 2) port = argv[1];
	int reps = 5;
	if (argc >= 3) reps = atoi(argv[1]);
	pool.addCapacity(reps);
	uint16_t bootPort = static_cast<uint16_t>(Poco::NumberParser::parse(port));
	k::first_session firstSession{k::endpoint{bootAddr4, bootPort}, k::endpoint{bootAddr6, bootPort}};
	std::cout << "bootstrap session listening on " << bootAddr4 << ':' << bootPort << ", " <<
		'[' << bootAddr6 << "]:" << bootPort << std::endl;
	uint16_t sessPort = bootPort + 1;

	std::vector<k::session*> sessions;
	for (int i = 0; i < reps; ++i)
	{
		sessions.push_back(new k::session { k::endpoint{ bootAddr4, bootPort }
					  , k::endpoint{ bootAddr4, sessPort }
					  , k::endpoint{ bootAddr6, sessPort } });
		std::cout << "peer session connected to " << bootAddr4 << ':' << bootPort <<
			", listening on " << bootAddr4 << ':' << sessPort << ", " <<
		'[' << bootAddr6 << "]:" << sessPort << std::endl;
		++sessPort;
	}
	Thread::sleep(100);
	for (int i = 0; i < reps; ++i)
	{
		std::string k("k"), v("v");
		k += std::to_string(i);
		v += std::to_string(i);
		save(*sessions[0], k, v);
		while (saved < i+1) Thread::sleep(10);
	}

	for (int i = 0; i < reps; ++i)
	{
		std::string k("k");
		k += std::to_string(i);
		load(*sessions[i], k);
		while (loaded < i+1) Thread::sleep(10);
	}

	abortSession(firstSession);
	loaded = 0;
	for (int i = 0; i < reps; ++i)
	{
		std::string k("k");
		k += std::to_string(i);
		load(*sessions[i], k);
		while (loaded < i+1) Thread::sleep(10);
	}

	for (auto& pS : sessions)
		abortSession(*pS);

	std::cout << "Goodbye!" << std::endl;
}
