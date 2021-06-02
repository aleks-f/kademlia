// Copyright (c) 2014, David Keller
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
#include "kademlia/buffer.hpp"
#include "FakeSocket.h"
#include "gtest/gtest.h"
#include <numeric>

namespace {

namespace k = kademlia;
namespace kd = k::detail;
namespace a = Poco::Net;


TEST(FakeSocketTest, can_be_created)
{
    a::SocketReactor io_service;
    k::test::FakeSocket s(io_service
                           , a::SocketAddress().family());

    EXPECT_EQ(0ULL, io_service.poll());
}

TEST(FakeSocketTest, does_not_invoke_receive_callback_until_data_is_received)
{
    a::SocketReactor io_service;
    a::SocketAddress endpoint;
    k::test::FakeSocket s(io_service, endpoint.family());

    EXPECT_EQ(0ULL, io_service.poll());

    kd::buffer buffer(32);

    auto on_receive = []
        (boost::system::error_code const&
        , std::size_t)
    { FAIL() << "unexpected call"; };

    s.async_receive_from(boost::asio::buffer(buffer)
                        , endpoint
                        , on_receive);

    EXPECT_EQ(0ULL, io_service.poll());
}

TEST(FakeSocketTest, invokes_send_callback_when_host_is_unreachable)
{
    a::SocketReactor io_service;
    a::SocketAddress endpoint;
    k::test::FakeSocket s(io_service, endpoint.family());

    EXPECT_EQ(0ULL, io_service.poll());

    kd::buffer buffer(32);

    auto on_send = []
        (boost::system::error_code const& failure
        , std::size_t bytes_count)
    {
        EXPECT_TRUE(failure);
        EXPECT_EQ(0, bytes_count);
    };

    s.async_send_to(boost::asio::buffer(buffer)
                   , endpoint
                   , on_send);

    EXPECT_EQ(0ULL, io_service.poll());
}

TEST(FakeSocketTest, can_send_and_receive_messages)
{
    a::SocketReactor io_service;
    //a::SocketReactor::work work(io_service);
    a::SocketAddress endpoint(k::test::FakeSocket::FIXED_PORT);

    k::test::FakeSocket receiver(io_service, endpoint.family());
    EXPECT_FALSE(receiver.bind(endpoint));

    k::test::FakeSocket sender(io_service, endpoint.family());
    EXPECT_FALSE(sender.bind(endpoint));

    EXPECT_EQ(0ULL, io_service.poll());

    kd::buffer received(64);
    kd::buffer sent(32);

    bool receive_callback_called = false;
    auto on_receive = [ &receive_callback_called, &sent ]
        (boost::system::error_code const& failure
        , std::size_t bytes_count)
    {
        receive_callback_called = true;
        EXPECT_FALSE(failure);
        EXPECT_EQ(sent.size(), bytes_count);
    };

    receiver.async_receive_from(boost::asio::buffer(received)
                               , endpoint
                               , on_receive);

    std::iota(sent.begin(), sent.end(), 1);
    auto on_send = [ &sent, &received ]
        (boost::system::error_code const& failure
        , std::size_t bytes_count)
    {
        EXPECT_FALSE(failure);
        EXPECT_EQ(sent.size(), bytes_count);
        received.resize(bytes_count);
    };

    sender.async_send_to(boost::asio::buffer(sent)
                        , receiver.local_endpoint()
                        , on_send);

	//TODO: reactor poll() returns number of sockets signalled, see if that really matters
    /*EXPECT_LT(0ULL,*/ io_service.poll()/*)*/;
    EXPECT_TRUE(receive_callback_called);
    EXPECT_EQ(sent, received);
}

TEST(FakeSocketTest, can_detect_invalid_address)
{
    a::SocketReactor io_service;
    a::SocketAddress endpoint(k::test::FakeSocket::FIXED_PORT);

    k::test::FakeSocket s(io_service, endpoint.family());
    EXPECT_FALSE(s.bind(endpoint));

    EXPECT_EQ(0ULL, io_service.poll());

    kd::buffer sent(32);
    std::iota(sent.begin(), sent.end(), 1);

    bool send_callback_called = false;
    auto on_send = [ &send_callback_called ]
        (boost::system::error_code const& failure
        , std::size_t bytes_count)
    {
        EXPECT_TRUE(failure);
        EXPECT_EQ(0ULL, bytes_count);
        send_callback_called = true;
    };

    //endpoint.address(boost::asio::ip::address_v4::any());
	endpoint = a::SocketAddress("0.0.0.0", k::test::FakeSocket::FIXED_PORT);
    s.async_send_to(boost::asio::buffer(sent), endpoint, on_send);

    EXPECT_LE(0ULL, io_service.poll());
    EXPECT_TRUE(send_callback_called);
}

TEST(FakeSocketTest, can_detect_closed_socket)
{
    a::SocketReactor io_service;
    a::SocketAddress endpoint(k::test::FakeSocket::FIXED_PORT);

    k::test::FakeSocket receiver(io_service, endpoint.family());
    EXPECT_FALSE(receiver.bind(endpoint));

    k::test::FakeSocket sender(io_service, endpoint.family());
    EXPECT_FALSE(sender.bind(endpoint));

    EXPECT_EQ(0ULL, io_service.poll());

    kd::buffer received(64);
    kd::buffer sent(32);

    auto on_receive = []
        (boost::system::error_code const& 
        , std::size_t)
    { };

    receiver.async_receive_from(boost::asio::buffer(received)
                               , endpoint
                               , on_receive);

    std::iota(sent.begin(), sent.end(), 1);
    auto on_send = []
        (boost::system::error_code const& failure
        , std::size_t bytes_count)
    {
        EXPECT_TRUE(failure);
        EXPECT_EQ(0ULL, bytes_count);
    };

    boost::system::error_code failure;
    receiver.close(failure);
    EXPECT_FALSE(failure);

    sender.async_send_to(boost::asio::buffer(sent)
                        , receiver.local_endpoint()
                        , on_send);

    EXPECT_LE(0ULL, io_service.poll());
}

TEST(FakeSocketTest, can_send_and_receive_messages_to_self)
{
    a::SocketReactor io_service;
    a::SocketAddress endpoint(k::test::FakeSocket::FIXED_PORT);

    k::test::FakeSocket sender(io_service, endpoint.family());
    EXPECT_FALSE(sender.bind(endpoint));

    EXPECT_EQ(0ULL, io_service.poll());

    kd::buffer received(64);
    kd::buffer sent(32);

    bool receive_callback_called = false;
    auto on_receive = [ &receive_callback_called, &sent ]
        (boost::system::error_code const& failure
        , std::size_t bytes_count)
    {
        receive_callback_called = true;
        EXPECT_FALSE(failure);
        EXPECT_EQ(sent.size(), bytes_count);
    };

    sender.async_receive_from(boost::asio::buffer(received)
                             , endpoint
                             , on_receive);

    std::iota(sent.begin(), sent.end(), 1);
    auto on_send = [ &sent, &received ]
        (boost::system::error_code const& failure
        , std::size_t bytes_count)
    {
        EXPECT_FALSE(failure);
        EXPECT_EQ(sent.size(), bytes_count);
        received.resize(bytes_count);
    };

    sender.async_send_to(boost::asio::buffer(sent)
                        , sender.local_endpoint()
                        , on_send);
	//TODO: reactor poll() returns number of sockets signalled, see if that really matters
    /*EXPECT_LT(0ULL,*/ io_service.poll()/*)*/;
    EXPECT_TRUE(receive_callback_called);
    EXPECT_EQ(sent, received);
}

}

