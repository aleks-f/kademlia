#include <cstdint>
#include <cstdlib>

#include <future>
#include <iostream>
#include <iterator>
#include <thread>

#include <kademlia/endpoint.hpp>
#include "kademlia/Session.h"
#include <kademlia/error.hpp>
#define _CRTDBG_MAP_ALLOC
#include "Poco/LeakDetector.h"
#include "Poco/NumberParser.h"
#include "StackWalker.h"
#include <psapi.h>

using Session = Kademlia::Session;
using Endpoint = kademlia::endpoint;
using Poco::LeakDetector;

class MyStackWalker : public StackWalker
{
public:
	MyStackWalker() : StackWalker() {}
protected:
	virtual void OnOutput(LPCSTR szText) {
		printf(szText); StackWalker::OnOutput(szText);
	}
};
/*
std::map<void*, size_t>& alloc_map()
{
	static std::map<void*, size_t> _map;
	return _map;
}
*/
std::map<void*, size_t> alloc_map;
static bool ready;
static size_t allocated = 0;
void* operator new(size_t size)
{
	void* p = malloc(size);
	/*if (ready)
	{
		alloc_map[p] = size;
		allocated += size;
		std::cout << "+allocated=" << allocated << std::endl;
	
	*/
	/*if (size >= 64000)
	{
		std::cout << "================ [" << p << ' ' << size << "] ================" << std::endl;
		MyStackWalker sw; sw.ShowCallstack();
		std::cout << "================================" << std::endl;
	}*/
	return p;
}

void operator delete(void* p)
{
	/*if (ready)
	{
		auto it = alloc_map.find(p);
		if (it != alloc_map.end())
		{
			allocated -= it->second;
			alloc_map.erase(it);
			std::cout << "-allocated=" << allocated << std::endl;
		}
		else std::cerr << "deleting unknown pointer: " << p << std::endl;
	}*/
	//std::cout << "---------------- [" << p << "] ----------------" << std::endl;
	free(p);
}

namespace {

	void PrintMemoryInfo(DWORD processID)
	{
		HANDLE hProcess;
		PROCESS_MEMORY_COUNTERS pmc;

		// Print the process identifier.

		//printf("\nProcess ID: %u\n", processID);

		// Print information about the memory usage of the process.

		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION |
			PROCESS_VM_READ,
			FALSE, processID);
		if (NULL == hProcess)
			return;

		if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc)))
		{
			SIZE_T usage = (pmc.WorkingSetSize + pmc.PagefileUsage)/1024;
			static SIZE_T prevUsage, baseUsage;;
			printf("\tMemoryUsage: %d kb (%d (%d) kb change)\n", (int)usage, int(usage)-int(prevUsage), int(usage)-int(baseUsage));
			if (prevUsage == 0) baseUsage = usage;
			prevUsage = usage;
			//printf("\tPageFaultCount: 0x%08X\n", pmc.PageFaultCount);
			//printf("\tPeakWorkingSetSize: 0x%08X\n",
				//pmc.PeakWorkingSetSize);
			//printf("\tWorkingSetSize: %d kb\n", (int)pmc.WorkingSetSize/1024);
			/*printf("\tQuotaPeakPagedPoolUsage: 0x%08X\n",
				pmc.QuotaPeakPagedPoolUsage);
			printf("\tQuotaPagedPoolUsage: 0x%08X\n",
				pmc.QuotaPagedPoolUsage);
			printf("\tQuotaPeakNonPagedPoolUsage: 0x%08X\n",
				pmc.QuotaPeakNonPagedPoolUsage);
			printf("\tQuotaNonPagedPoolUsage: 0x%08X\n",
				pmc.QuotaNonPagedPoolUsage);*/
			//printf("\tPagefileUsage: %d kb\n", (int)pmc.PagefileUsage/1024);
			/*printf("\tPeakPagefileUsage: 0x%08X\n",
				pmc.PeakPagefileUsage);*/
		}

		CloseHandle(hProcess);
	}

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

void load(Session & session, std::string const& key)
{
	//_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE /* | _CRTDBG_MODE_WNDW*/);
	//_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	//std::shared_ptr<LeakDetector> ld(new LeakDetector);
	//ld->checkPoint();
	//PrintMemoryInfo(GetCurrentProcessId());
	std::vector<uint8_t> key_vec(key.begin(), key.end());
	auto on_load = [key/*, ld*/](std::error_code const& error, Session::DataType const& data)
	{
		if (error)
			std::cerr << "Failed to load \"" << key << "\", error: " << error.message() << std::endl;
		else
		{
			std::string const& str{ data.begin(), data.end() };
			std::cout << "Loaded \"" << key << /*"\" as \"" << str << "\"" <<*/ std::endl;
			//ld->checkPoint();
			//if (ld->hasLeaks())
			{
				PrintMemoryInfo(GetCurrentProcessId());
				//ld->dump();
			}
		}
	};

	session.asyncLoad(key_vec, std::move(on_load));
}

void save(Session & session, std::string const& key, std::string const& val)
{
	//_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE /* | _CRTDBG_MODE_WNDW*/);
	//_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	//std::shared_ptr<LeakDetector> ld(new LeakDetector);
	//ld->checkPoint();
	//PrintMemoryInfo(GetCurrentProcessId());
	std::vector<uint8_t> key_vec(key.begin(), key.end());
	std::vector<uint8_t> val_vec(val.begin(), val.end());
	auto on_save = [key/*, ld*/](std::error_code const& error)
	{
		if (error)
			std::cerr << "Failed to save \"" << key << "\", error: " << error.message() << std::endl;
		else
		{
			//std::cout << "Saved \"" << key << "\"" << std::endl;
			//ld->checkPoint();
			//if (ld->hasLeaks())
			{
				PrintMemoryInfo(GetCurrentProcessId());
				//ld->dump();
			}
		}
	};

	session.asyncSave(key_vec, std::move(val_vec), std::move(on_save));
}

void runSave(Session& session, int count)
{
	//EmptyWorkingSet(GetCurrentProcess());
	//PrintMemoryInfo(GetCurrentProcessId());
	for (int i = 0; i < count; ++i)
	{
		save(session, std::to_string(i), std::string(32000, '0'));
		Poco::Thread::sleep(20);
	}
	//EmptyWorkingSet(GetCurrentProcess());
	//PrintMemoryInfo(GetCurrentProcessId());
}

void print_interactive_help()
{
	std::cout << HELP << std::flush;
}

} // anonymous namespace

int main(int argc, char** argv)
{
	ready = true;
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
	Session session{ Endpoint{ boot_addr, boot_port }
					  , Endpoint{ "0.0.0.0", port }
					  , Endpoint{ "::", port } };

	
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
		else if (tokens[0] == "run")
		{
			if (tokens.size() != 2)
				print_interactive_help();
			else
				runSave(session, Poco::NumberParser::parse(tokens[1]));
		}
		else if (tokens[0] == "exit") break;
		else
			print_interactive_help();
	}

	// Stop the session loop
	session.abort();

	// Wait for the session termination
	auto failure = session.wait();
	if (failure != kademlia::RUN_ABORTED)
		std::cerr << failure.message() << std::endl;
	std::cout << "Goodbye!" << std::endl;
}
