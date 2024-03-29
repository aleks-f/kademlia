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
#include "kademlia/log.hpp"
#include "gtest/gtest.h"
#include <iostream>

namespace {

namespace kd = kademlia::detail;

struct rdbuf_saver
{
    rdbuf_saver
        (std::ostream & stream
        , std::streambuf * buffer)
            : stream_(stream)
            , old_buffer_(stream.rdbuf(buffer))
    { }

    ~rdbuf_saver
        (void)
    { stream_.rdbuf(old_buffer_); }

    std::ostream & stream_;
    std::streambuf * old_buffer_;
};

class LogTest: public ::testing::Test
{
protected:
    LogTest()
    {
    }
    
    ~LogTest() override
    {
    }

    void SetUp() override
    {
        kd::enableLogFor("test");
    }

    void TearDown() override
    {
        kd::disableLogFor("*");
    }
};
    
TEST_F(LogTest, can_write_to_debug_log)
{
    std::ostringstream out;
    std::ostringstream ostr;
    {
        rdbuf_saver const s{ std::cout, out.rdbuf() };
        auto const ptr = reinterpret_cast< void *>(0x12345678);
        std::time_t t = std::time(nullptr);
    	std::tm* pTM = std::localtime(&t);
        kd::getDebugLog("test", ptr, pTM)<< "message" << std::endl;
        ostr << "[debug] " << std::put_time(pTM, "%F %H:%M:%S") << " (test @ 345678) message\n";
    }

    EXPECT_EQ(out.str(), ostr.str());
}
/*
TEST_F(LogTest, can_write_to_debug_log_using_macro)
{
    std::ostringstream out;
    {
        rdbuf_saver const s{ std::cout, out.rdbuf() };
        auto const ptr = reinterpret_cast< void *>(0x12345678);
        LOG_DEBUG(test, ptr)<< "message" << std::endl;
    }

#ifdef KADEMLIA_ENABLE_DEBUG
    EXPECT_TRUE(out.str().substr(0, 8) == "[debug] ");
    EXPECT_TRUE(out.str().substr(out.str().length()-24) == "(test @ 345678) message\n");
#else
    EXPECT_TRUE(out.str().empty());
#endif
}
*/
TEST_F(LogTest, can_enable_log_module)
{
    // By default, unit tests enable log on all modules.
    kd::disableLogFor("*");

    EXPECT_TRUE(! kd::isLogEnabled("test1"));
    EXPECT_TRUE(! kd::isLogEnabled("test2"));

    kd::enableLogFor("test1");
    EXPECT_TRUE(kd::isLogEnabled("test1"));
    EXPECT_TRUE(! kd::isLogEnabled("test2"));

    kd::enableLogFor("*");
    EXPECT_TRUE(kd::isLogEnabled("test1"));
    EXPECT_TRUE(kd::isLogEnabled("test2"));

}

TEST_F(LogTest, can_convert_container_to_string)
{
    {
        std::vector< std::uint8_t > const c{ 'a', 'b', 'c' };
        auto const r = kd::toString(c);
        EXPECT_EQ("abc", r);
    }
    {
        std::vector< std::uint8_t > const c{ 1, 2, 3 };
        auto const r = kd::toString(c);
        EXPECT_EQ("\\1\\2\\3", r);
    }
}

}

