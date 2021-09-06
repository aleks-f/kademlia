#include <cstdint>
#include <cstdlib>

#include <future>
#include <iostream>
#include <iterator>
#include <thread>

#include <kademlia/endpoint.hpp>
#include <kademlia/session.hpp>
#include <kademlia/error.hpp>

namespace k = kademlia;

namespace {

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

void load(k::session & session, std::string const& key)
{
	std::vector<uint8_t> key_vec(key.begin(), key.end());
	auto on_load = [key] (std::error_code const& error, k::session::data_type const& data)
	{
		if (error)
			std::cerr << "Failed to load \"" << key << "\", error: " << error.message() << std::endl;
		else
		{
			std::string const& str{ data.begin(), data.end() };
			std::cout << "Loaded \"" << key << "\" as \"" << str << "\"" << std::endl;
		}
	};

	session.asyncLoad(key_vec, std::move(on_load));
}

void save(k::session & session, std::string const& key, std::string const& val)
{
	std::vector<uint8_t> key_vec(key.begin(), key.end());
	std::vector<uint8_t> val_vec(val.begin(), val.end());
	auto on_save = [key] (std::error_code const& error)
	{
		if (error)
			std::cerr << "Failed to save \"" << key << "\", error: " << error.message() << std::endl;
		else
			std::cout << "Saved \"" << key << "\"" << std::endl;
	};

	session.asyncSave(key_vec, val_vec, std::move(on_save));
}

void print_interactive_help()
{
	std::cout << HELP << std::flush;
}

} // anonymous namespace

int main(int argc, char** argv)
{
	// Check command line arguments count
	if (argc != 3)
	{
		std::cerr << "usage: " << argv[0] << " <PORT> <INITIAL_PEER>" << std::endl;
		return EXIT_FAILURE;
	}

	// Parse command line arguments
	std::uint16_t const port = std::atoi(argv[1]);

	std::string boot_ep(argv[2]);
	auto sep_idx = boot_ep.find(':');
	if (sep_idx == std::string::npos)
	{
		std::cerr << "initial peer must be of the format IP:PORT (e.g. 1.2.3.4:5555)" << std::endl;
		return EXIT_FAILURE;
	}
	auto boot_addr = boot_ep.substr(0, sep_idx);
	auto boot_port = boot_ep.substr(sep_idx+1);

	// Create the session (runs in its own thread)
	k::session session{ k::endpoint{ boot_addr, boot_port }
					  , k::endpoint{ "0.0.0.0", port }
					  , k::endpoint{ "::", port } };

	// Parse stdin until EOF (CTRL-D in Unix, CTRL-Z-Enter on Windows))
	std::cout << "Enter \"help\" to see available actions" << std::endl;
	for (std::string line; std::getline(std::cin, line);)
	{
		auto const tokens = split(line);

		if (tokens.empty())
			continue;

		if (tokens[0] == "help")
		{
			print_interactive_help();
		}
		else if (tokens[0] == "save")
		{
			if (tokens.size() != 3)
				print_interactive_help();
			else
				save(session, tokens[1], tokens[2]);
		}
		else if (tokens[0] == "load")
		{
			if (tokens.size() != 2)
				print_interactive_help();
			else
				load(session, tokens[1]);
		}
		else if (tokens[0] == "exit") break;
		else
			print_interactive_help();
	}

	// Stop the session loop
	session.abort();

	// Wait for the session termination
	auto failure = session.wait();
	if (failure != k::RUN_ABORTED)
		std::cerr << failure.message() << std::endl;
	std::cout << "Goodbye!" << std::endl;
}
