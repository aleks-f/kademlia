// Copyright (c) 2010, David Keller
// All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the University of California, Berkeley nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY DAVID KELLER AND CONTRIBUTORS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#if defined(_MSC_VER)
#   pragma warning(push)
#   pragma warning(disable:4996)
#endif

#include <kademlia/session.hpp>

#include <iostream>
#include <algorithm>
#include <stdexcept>
#ifdef HAS_CXX11_THREAD
#   include <thread>
#else
#   include <boost/thread.hpp>
#endif
#include <functional>
#include <boost/throw_exception.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "socket.hpp"

namespace ao = boost::asio;
using ao::ip::udp;
using ao::ip::tcp;

namespace kademlia {

/**
 *
 */
class session::impl
{
public:
    /**
     *
     */
    explicit 
    impl
        ( std::vector<endpoint> const& endpoints
        , endpoint const& initial_peer )
        : io_service_()
        , message_sockets_( create_sockets<udp>( io_service_, endpoints ) )
		, content_sockets_( create_sockets<tcp>( io_service_, endpoints ) )
        , main_thread_()
    {
		init( initial_peer );

        // Run the main loop on a dedicated thread.
#ifdef HAS_CXX11_THREAD
        main_thread_ = std::thread( &impl::run_loop, this );
#else
        main_thread_ = boost::thread( std::bind( &impl::run_loop, this ) );
#endif
    }

    /**
     *
     */
    ~impl
        ( void )
    {
        graceful_close_sockets( message_sockets_ );
		graceful_close_sockets( content_sockets_ );
        // Stop is_service, this will break the run_loop.
        io_service_.stop();
        // Wait for the main_thread to exit.
        main_thread_.join();
    }

    /**
     *
     */
	void
	init
		( endpoint const& initial_peer  )
	{

	}

    /**
     *
     */
    void
    async_save
        ( std::string const& key 
        , std::string const& data
        , save_handler_type handler )
    { }

    /**
     *
     */
    void
    async_load
        ( std::string const& key
        , load_handler_type handler )
    { }

private:
    /**
     *
     */
    void
    run_loop
        ( void )
    {
        while ( io_service_.run_one() > 0 ) {

        }
    }

private:
    ao::io_service io_service_;
    std::vector<udp::socket> message_sockets_;
    std::vector<tcp::socket> content_sockets_;
#ifdef HAS_CXX11_THREAD
    std::thread main_thread_;
#else
    boost::thread main_thread_;
#endif
};

/**
 *
 */
session::session
    ( std::vector<endpoint> const& endpoints
    , endpoint const& initial_peer )
    : impl_( new impl( endpoints, initial_peer ) )
{ }

/**
 *
 */
session::~session
    ( void )
{ }

/**
 *
 */
void
session::async_save
    ( std::string const& key 
    , std::string const& data
    , save_handler_type handler )
{ impl_->async_save( key, data, handler ); }

/**
 *
 */
void
session::async_load
    ( std::string const& key
    , load_handler_type handler )
{ impl_->async_load( key, handler ); }


} // namespace kademlia

#if defined(_MSC_VER)
#   pragma warning(pop)
#endif
