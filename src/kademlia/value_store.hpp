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

#ifndef KADEMLIA_VALUE_STORE_HPP
#define KADEMLIA_VALUE_STORE_HPP

#ifdef _MSC_VER
#   pragma once
#endif

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>
#include "id.hpp"
#include "Poco/Hash.h"


namespace kademlia {
namespace detail {

template< typename Container >
struct value_store_key_hasher
{
    using argument_type = Container;
    using result_type = std::size_t;

    result_type operator()(argument_type const& key) const
    {
        return Poco::hashRange(key.begin(), key.end());
    }
};

///
template< typename Key, typename Value >
using value_store = std::unordered_map
        < Key
        , Value
        , value_store_key_hasher< Key > >;

using data_type = std::vector<std::uint8_t>;

using value_store_type = value_store<id, data_type>;

} // namespace detail
} // namespace kademlia

#endif

