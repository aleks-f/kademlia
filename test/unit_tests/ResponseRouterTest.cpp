// Copyright (c) 2013-2014, David Keller
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

#include "common.hpp"
#include "Poco/Net/SocketReactor.h"
#include "kademlia/ResponseRouter.h"
#include "gtest/gtest.h"

namespace {

namespace k = kademlia;
namespace kd = k::detail;


TEST(ResponseRouterNoThrowTest, can_be_constructed_using_a_reactor)
{
    Poco::Net::SocketReactor io_service;
    EXPECT_NO_THROW( kd::ResponseRouter{ io_service } );
}


struct ResponseRouterTest: public ::testing::Test
{
    ResponseRouterTest()
        : io_service_{}
        , router_{ io_service_ }
        , messages_received_count_{}
        , error_count_{}
    { }

    Poco::Net::SocketReactor io_service_;
    kd::ResponseRouter router_;
    std::size_t messages_received_count_;
    std::size_t error_count_;

protected:
    ~ResponseRouterTest() override
    {
    }

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};


TEST_F(ResponseRouterTest, known_messages_are_forwarded)
{
    // Create the callbacks.
    auto on_message_received = [ this ]
            ( kd::ResponseCallbacks::endpoint_type const& s
            , kd::Header const& h
            , kd::buffer::const_iterator
            , kd::buffer::const_iterator )
    { ++ messages_received_count_; };

    auto on_error = [ this ]
        ( std::error_code const& failure )
    { ++ error_count_; };

    kd::Header const h1{ kd::Header::V1, kd::Header::PING_REQUEST
                       , kd::id{}, kd::id{ "1" } };

    router_.register_temporary_callback( h1.random_token_
                                       , std::chrono::hours{ 1 }
                                       , on_message_received
                                       , on_error );

    io_service_.poll();
    EXPECT_EQ(0ULL, messages_received_count_ );
    EXPECT_EQ(0ULL, error_count_ );

    kd::ResponseCallbacks::endpoint_type const s{};
    kd::buffer const b;

    // Send the expected message.
    router_.handle_new_response( s, h1, b.begin(), b.end() );

    io_service_.poll();
    EXPECT_EQ(1ULL, messages_received_count_ );
    EXPECT_EQ(0ULL, error_count_ );
}

TEST_F(ResponseRouterTest, known_messages_are_not_forwarded_when_late)
{
    // Create the callbacks.
    auto on_message_received = [ this ]
            ( kd::ResponseCallbacks::endpoint_type const& s
            , kd::Header const& h
            , kd::buffer::const_iterator
            , kd::buffer::const_iterator )
    { ++ messages_received_count_; };

    auto on_error = [ this ]
        ( std::error_code const& failure )
    { ++ error_count_; };

    kd::Header const h1{ kd::Header::V1, kd::Header::PING_REQUEST
                       , kd::id{}, kd::id{ "1" } };

    router_.register_temporary_callback( h1.random_token_
                                       , std::chrono::hours{ 0 }
                                       , on_message_received
                                       , on_error );

    while (0 == io_service_.poll());
    EXPECT_EQ(0ULL, messages_received_count_ );
    EXPECT_EQ(1ULL, error_count_ );

    kd::ResponseCallbacks::endpoint_type const s{};
    kd::buffer const b;

    // Send the expected message.
    router_.handle_new_response( s, h1, b.begin(), b.end() );

    io_service_.poll();
    EXPECT_EQ(0ULL, messages_received_count_ );
    EXPECT_EQ(1ULL, error_count_ );
}

}
