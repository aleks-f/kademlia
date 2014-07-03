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

#ifndef KADEMLIA_ENGINE_HPP
#define KADEMLIA_ENGINE_HPP

#ifdef _MSC_VER
#   pragma once
#endif

#include <algorithm>
#include <stdexcept>
#include <chrono>
#include <random>
#include <memory>
#include <boost/asio/io_service.hpp>

#include <kademlia/error.hpp>

#include "timer.hpp"
#include "message_serializer.hpp"
#include "response_dispatcher.hpp"
#include "message_socket.hpp"
#include "message.hpp"
#include "routing_table.hpp"
#include "value_store.hpp"
#include "candidate.hpp"
#include "find_value_context.hpp"
#include "store_value_context.hpp"

namespace kademlia {
namespace detail {

namespace {

// k 
CXX11_CONSTEXPR std::size_t ROUTING_TABLE_BUCKET_SIZE{ 20 };
// a
CXX11_CONSTEXPR std::size_t CONCURRENT_FIND_NODE_REQUESTS_COUNT{ 3 };
// c
CXX11_CONSTEXPR std::size_t REDUNDANT_SAVE_COUNT{ 3 };

//
CXX11_CONSTEXPR std::chrono::milliseconds INITIAL_CONTACT_RECEIVE_TIMEOUT{ 1000 };
//
CXX11_CONSTEXPR std::chrono::milliseconds NODE_LOOKUP_TIMEOUT{ 20 };

} // anonymous namespace

/**
 *
 */
template< typename KeyType, typename DataType, typename SubnetType >
class engine final
{
public:
    ///
    using key_type = KeyType;
 
    ///
    using data_type = DataType;

    ///
    using subnet_type = SubnetType;
 
public:
    /**
     *
     */
    engine
        ( endpoint const& initial_peer
        , endpoint const& listen_on_ipv4
        , endpoint const& listen_on_ipv6 )
        : random_engine_{ std::random_device{}() }
        , my_id_( random_engine_ )
        , io_service_{}
        , response_dispatcher_{}
        , timer_{ io_service_ }
        , message_serializer_{ my_id_ }
        , subnet_ipv4_{ create_ipv4_subnet( io_service_, listen_on_ipv4 ) }
        , subnet_ipv6_{ create_ipv6_subnet( io_service_, listen_on_ipv6 ) }
        , initial_peer_{ initial_peer }
        , routing_table_{ my_id_ }
        , value_store_{}
        , main_failure_{}
    { }

    /**
     *
     */
    engine
        ( engine const& )
        = delete;

    /**
     *
     */
    engine &
    operator=
        ( engine const& )
        = delete;
    
    /**
     *
     */
    template< typename HandlerType >
    void
    async_save
        ( key_type const& key 
        , data_type const& data
        , HandlerType handler )
    { 
        auto context = create_store_value_context( id{ key }
                                                 , data
                                                 , std::move( handler ) );
        async_store_value( context );
    }

    /**
     *
     */
    template< typename HandlerType >
    void
    async_load
        ( key_type const& key
        , HandlerType handler )
    { 
        auto context = create_find_value_context( id{ key }
                                                , std::move( handler ) );
        async_find_value( context );
    }

    /**
     *
     */
    std::error_code
    run
        ( void ) 
    {
        main_failure_.clear();

        init();
        
        while ( ! main_failure_ && io_service_.run_one() )
            io_service_.poll();
        
        io_service_.stop();

        return main_failure_;
    }

    /**
     *
     */
    void
    abort
        ( void )
    { 
        auto set_abort_flag = [ this ] ( void )
        { main_failure_ = make_error_code( RUN_ABORTED ); };

        io_service_.post( set_abort_flag );
    }

private:
    ///
    using routing_table_type = routing_table< message_socket::endpoint_type >;

private:
    /**
     *
     */
    void
    init
        ( void )
    {
        io_service_.reset();

        schedule_receive_on_subnet( subnet_ipv4_ );
        schedule_receive_on_subnet( subnet_ipv6_ );

        async_discover_neighbors();
    }

    /**
     *
     */
    void
    schedule_receive_on_subnet
        ( subnet_type & current_subnet )
    {
        auto on_new_message = [ this, &current_subnet ]
            ( std::error_code const& failure
            , message_socket::endpoint_type const& sender
            , buffer const& message )
        {
            if ( ! failure )
                handle_new_message( sender, message );

            schedule_receive_on_subnet( current_subnet );
        };

        current_subnet.async_receive( on_new_message );
    }

    /**
     *
     */
    static subnet_type
    create_ipv4_subnet
        ( boost::asio::io_service & io_service
        , endpoint const& ipv4_endpoint )
    {
        auto endpoints = resolve_endpoint( io_service, ipv4_endpoint );

        for ( auto const& i : endpoints )
        {
            if ( i.address().is_v4() )
                return subnet_type{ create_socket( io_service, i ) };
        }

        throw std::system_error{ make_error_code( INVALID_IPV4_ADDRESS ) };
    }

    /**
     *
     */
    static subnet_type
    create_ipv6_subnet
        ( boost::asio::io_service & io_service
        , endpoint const& ipv6_endpoint )
    {
        auto endpoints = resolve_endpoint( io_service, ipv6_endpoint );

        for ( auto const& i : endpoints )
        {
            if ( i.address().is_v6() )
                return subnet_type{ create_socket( io_service, i ) };
        }

        throw std::system_error{ make_error_code( INVALID_IPV6_ADDRESS ) };
    }

    /**
     *
     */
    subnet_type &
    get_subnet_for
        ( message_socket::endpoint_type const& e )
    {
        if ( e.address().is_v4() )
            return subnet_ipv4_;

        return subnet_ipv6_;
    }


    /**
     *
     */
    template< typename HandlerType >
    std::shared_ptr< find_value_context< HandlerType, data_type > >
    create_find_value_context 
        ( id const& key
        , HandlerType load_handler )
    {
        using context = find_value_context< HandlerType, data_type >;

        auto i = routing_table_.find( key ), e = routing_table_.end();
        return std::make_shared< context >( key, i, e
                                          , std::move( load_handler ) );
    }

    /**
     *
     */
    template< typename HandlerType >
    std::shared_ptr< store_value_context< HandlerType, data_type > >
    create_store_value_context 
        ( id const& key
        , data_type const& data
        , HandlerType save_handler )
    {
        using context = store_value_context< HandlerType, data_type >;

        auto i = routing_table_.find( key ), e = routing_table_.end();
        return std::make_shared< context >( key, data, i, e
                                          , std::move( save_handler ) );
    }

    /**
     *
     */
    void
    handle_new_request
        ( message_socket::endpoint_type const& sender
        , header const& h
        , buffer::const_iterator i
        , buffer::const_iterator e )
    {
        switch ( h.type_ )
        {
            case header::PING_REQUEST: 
                handle_ping_request( sender, h );
                break;
            case header::STORE_REQUEST: 
                handle_store_request( sender, h, i, e );
                break;
            case header::FIND_NODE_REQUEST: 
                handle_find_node_request( sender, h, i, e );
                break;
            case header::FIND_VALUE_REQUEST:
                handle_find_value_request( sender, h, i, e );
                break;
            default:
                // Unknown request or unassociated responses
                // are discarded.
                break;
        }
    }

    /**
     *
     */
    void
    add_current_peer_to_routing_table
        ( id const& peer_id
        , message_socket::endpoint_type const& peer_endpoint )
    { routing_table_.push( peer_id, peer_endpoint ); }

    /**
     *
     */
    void
    handle_ping_request
        ( message_socket::endpoint_type const& sender
        , header const& h )
    {
        add_current_peer_to_routing_table( h.source_id_, sender );

        async_send_response( h.random_token_
                           , header::PING_RESPONSE
                           , sender ); 
    }

    /**
     *
     */
    void
    handle_store_request
        ( message_socket::endpoint_type const& sender
        , header const& h
        , buffer::const_iterator i
        , buffer::const_iterator e )
    {
        store_value_request_body request;
        if ( deserialize( i, e, request ) )
            return;

        add_current_peer_to_routing_table( h.source_id_, sender );

        value_store_[ request.data_key_hash_ ] 
                = std::move( request.data_value_ );
    }

    /**
     *
     */
    void
    handle_find_node_request
        ( message_socket::endpoint_type const& sender
        , header const& h
        , buffer::const_iterator i
        , buffer::const_iterator e )
    {
        // Ensure the request is valid.
        find_node_request_body request;
        if ( deserialize( i, e, request ) )
            return;

        add_current_peer_to_routing_table( h.source_id_, sender );

        send_find_node_response( sender
                               , h.random_token_
                               , request.node_to_find_id_ );
    }

    /**
     *
     */
    void
    send_find_node_response
        ( message_socket::endpoint_type const& sender
        , id const& random_token
        , id const& node_to_find_id )
    {
        // Find X closest peers and save
        // their location into the response..
        find_node_response_body response;

        auto remaining_node = ROUTING_TABLE_BUCKET_SIZE;
        for ( auto i = routing_table_.find( node_to_find_id )
                 , e = routing_table_.end()
            ; i != e && remaining_node > 0
            ; ++i, -- remaining_node )
            response.nodes_.push_back( { i->first, i->second } );

        // Now send the response.
        async_send_response( random_token, response, sender );
    }

    /**
     *
     */
    void
    handle_find_value_request
        ( message_socket::endpoint_type const& sender
        , header const& h
        , buffer::const_iterator i
        , buffer::const_iterator e )
    {
        find_value_request_body request;
        if ( deserialize( i, e, request ) )
            return;

        add_current_peer_to_routing_table( h.source_id_, sender );

        auto found = value_store_.find( request.value_to_find_ ); 
        if ( found == value_store_.end() )
            send_find_node_response( sender
                                   , h.random_token_
                                   , request.value_to_find_ );
        else
        {
            find_value_response_body const response{ found->second };
            async_send_response( h.random_token_
                               , response
                               , sender );
        }
    }

    /**
     *
     */
    void
    async_discover_neighbors
        ( void )
    {
        // Initial peer should know our neighbors, hence ask 
        // him which peers are close to our own id.
        auto endoints_to_query = std::make_shared< resolved_endpoints >
                ( resolve_endpoint( io_service_, initial_peer_ ) );

        async_search_ourselves( std::move( endoints_to_query ) );
    }

    /**
     *
     */
    void
    async_search_ourselves
        ( std::shared_ptr< resolved_endpoints > endpoints_to_query )
    { 
        if ( endpoints_to_query->empty() )
        {
            main_failure_ = make_error_code( INITIAL_PEER_FAILED_TO_RESPOND );
            return;
        }

        // Retrieve the next endpoint to query.
        auto const endpoint_to_query = endpoints_to_query->back();
        endpoints_to_query->pop_back();

        // On message received, process it.
        auto on_message_received = [ this ]
            ( message_socket::endpoint_type const& s
            , header const& h
            , buffer::const_iterator i
            , buffer::const_iterator e )
        { handle_initial_contact_response( s, h, i, e ); };

        // On error, retry with another endpoint.
        auto on_error = [ this, endpoints_to_query ]
            ( std::error_code const& ) 
        { async_search_ourselves( endpoints_to_query ); };

        async_send_request( id{ random_engine_ }
                          , find_node_request_body{ my_id_ }
                          , endpoint_to_query
                          , INITIAL_CONTACT_RECEIVE_TIMEOUT
                          , on_message_received
                          , on_error );
    }

    /**
     *
     */
    void
    handle_initial_contact_response
        ( message_socket::endpoint_type const& s
        , header const& h
        , buffer::const_iterator i
        , buffer::const_iterator e )
    { 
        if ( h.type_ != header::FIND_NODE_RESPONSE )
            return ;

        find_node_response_body response;
        if ( deserialize( i, e, response ) )
            return;

        // Add the initial peer to the routing_table.
        routing_table_.push( h.source_id_, s );

        // And its known peers.
        for ( auto const& node : response.nodes_ )
            routing_table_.push( node.id_, node.endpoint_ );
    }

    /**
     *
     */
    template< typename HandlerType >
    void
    async_store_value
        ( std::shared_ptr< store_value_context< HandlerType, data_type > > context )
    { 
        find_node_request_body const request{ context->get_key() };

        for ( auto const& c : context->select_new_closest_candidates( CONCURRENT_FIND_NODE_REQUESTS_COUNT ) )
            async_send_find_node_request( request, c, context ); 
    }

    /**
     *
     */
    template< typename HandlerType >
    void
    async_send_find_node_request
        ( find_node_request_body const& request
        , candidate const& current_candidate
        , std::shared_ptr< store_value_context< HandlerType, data_type > > context )
    { 
        // On message received, process it.
        auto on_message_received = [ this, context, current_candidate ]
            ( message_socket::endpoint_type const& s
            , header const& h
            , buffer::const_iterator i
            , buffer::const_iterator e )
        { 
            context->flag_candidate_as_valid( current_candidate.id_ );

            handle_find_node_response( s, h, i, e, context );
        };

        // On error, retry with another endpoint.
        auto on_error = [ this, context, current_candidate ]
            ( std::error_code const& ) 
        {
            // XXX: Can also flag candidate as invalid is
            // present in routing table.
            context->flag_candidate_as_invalid( current_candidate.id_ );

            // If no more requests are in flight
            // we know the closest nodes hence ask
            // them to store the value.
            if ( context->have_all_requests_completed() )
                send_store_requests( context );
        };

        async_send_request( id{ random_engine_ }
                          , request
                          , current_candidate.endpoint_
                          , NODE_LOOKUP_TIMEOUT 
                          , on_message_received
                          , on_error );
    }

    /**
     *
     */
    template< typename HandlerType >
    void
    async_find_value
        ( std::shared_ptr< find_value_context< HandlerType, data_type > > context )
    {
        find_value_request_body const request{ context->get_key() };

        for ( auto const& c : context->select_new_closest_candidates( CONCURRENT_FIND_NODE_REQUESTS_COUNT ) )
            async_send_find_value_request( request, c, context );
    }

    /**
     *
     */
    template< typename HandlerType >
    void
    async_send_find_value_request
        ( find_value_request_body const& request
        , candidate const& current_candidate
        , std::shared_ptr< find_value_context< HandlerType, data_type > > context )
    {
        // On message received, process it.
        auto on_message_received = [ this, context, current_candidate ]
            ( message_socket::endpoint_type const& s
            , header const& h
            , buffer::const_iterator i
            , buffer::const_iterator e )
        { 
            if ( context->is_caller_notified() )
                return;

            context->flag_candidate_as_valid( current_candidate.id_ );
            handle_find_value_response( s, h, i, e, context );
        };

        // On error, retry with another endpoint.
        auto on_error = [ this, context, current_candidate ]
            ( std::error_code const& ) 
        {
            if ( context->is_caller_notified() )
                return;

            // XXX: Current current_candidate must be flagged as stale.
            context->flag_candidate_as_invalid( current_candidate.id_ );
            async_find_value( context ); 
        };

        async_send_request( id{ random_engine_ }
                          , request
                          , current_candidate.endpoint_
                          , NODE_LOOKUP_TIMEOUT 
                          , on_message_received
                          , on_error );
    }

    /**
     *
     */
    template< typename HandlerType >
    void
    handle_find_value_response
        ( message_socket::endpoint_type const& s
        , header const& h
        , buffer::const_iterator i
        , buffer::const_iterator e
        , std::shared_ptr< find_value_context< HandlerType, data_type > > context )
    { 
        // Add the initial peer to the routing_table.
        add_current_peer_to_routing_table( h.source_id_, s );

        if ( h.type_ == header::FIND_NODE_RESPONSE )
            handle_find_node_response( i, e, context );
        else if ( h.type_ == header::FIND_VALUE_RESPONSE )
            handle_find_value_response( i, e, context );
    }

    /**
     *
     */
    template< typename HandlerType >
    void
    handle_find_node_response
        ( buffer::const_iterator i
        , buffer::const_iterator e
        , std::shared_ptr< find_value_context< HandlerType, data_type > > context )
    { 
        find_node_response_body response;
        if ( deserialize( i, e, response ) )
            return;

        if ( context->are_these_candidates_closest( response.nodes_ ) )
            async_find_value( context );
        
        if ( context->have_all_requests_completed() )
            context->notify_caller( make_error_code( VALUE_NOT_FOUND ) );
    }

    /**
     *
     */
    template< typename HandlerType >
    void
    handle_find_value_response
        ( buffer::const_iterator i
        , buffer::const_iterator e
        , std::shared_ptr< find_value_context< HandlerType, data_type > > context )
    { 
        find_value_response_body response;
        if ( deserialize( i, e, response ) )
            return;

        context->notify_caller( response.data_ );
    }

    /**
     *
     */
    template< typename HandlerType >
    void
    handle_find_node_response
        ( message_socket::endpoint_type const& s
        , header const& h
        , buffer::const_iterator i
        , buffer::const_iterator e
        , std::shared_ptr< store_value_context< HandlerType, data_type > > context )
    { 
        // Add the initial peer to the routing_table.
        add_current_peer_to_routing_table( h.source_id_, s );

        find_node_response_body response;
        if ( deserialize( i, e, response ) )
            return;

        // If new candidate have been discovered, ask them.
        if ( context->are_these_candidates_closest( response.nodes_ ) )
            async_store_value( context );
        // Else if all candidates have responded, 
        // we know the closest nodes hence ask them
        // to store the value.
        else if ( context->have_all_requests_completed() )
            send_store_requests( context );
    }
    
    /**
     *
     */
    template< typename HandlerType >
    void
    send_store_requests
        ( std::shared_ptr< store_value_context< HandlerType, data_type > > context )
    { 
        for ( auto c : context->select_closest_valid_candidates( REDUNDANT_SAVE_COUNT ) )
            send_store_request( c, context );

        context->notify_caller( std::error_code{} );
    }

    /**
     *
     */
    template< typename HandlerType >
    void
    send_store_request
        ( candidate const& current_candidate
        , std::shared_ptr< store_value_context< HandlerType, data_type > > context )
    {
        store_value_request_body const request{ context->get_key()
                                              , context->get_data() };
        async_send_request( id{ random_engine_ }
                          , request
                          , current_candidate.endpoint_ );
    }

    /**
     *
     */
    void
    handle_new_message
        ( message_socket::endpoint_type const& sender
        , buffer const& message )
    {
        auto i = message.begin(), e = message.end();

        detail::header h;
        // Try to deserialize header.
        if ( deserialize( i, e, h ) )
            return;

        // Try to forward the message to its associated callback.
        auto failure = response_dispatcher_.dispatch_message( sender
                                                            , h, i, e ); 

        // If no callback was associated, forward the message
        // to the request handler.
        if ( failure == UNASSOCIATED_MESSAGE_ID )
            handle_new_request( sender, h, i, e );
    }

    /**
     *
     */
    template< typename Request, typename OnResponseReceived, typename OnError >
    void
    async_send_request
        ( id const& response_id
        , Request const& request
        , message_socket::endpoint_type const& e
        , timer::duration const& timeout
        , OnResponseReceived const& on_response_received
        , OnError const& on_error )
    {
        // Generate the request buffer.
        auto message = message_serializer_.serialize( request, response_id );

        // This lamba will keep the request message alive.
        auto on_request_sent = [ this, response_id
                               , on_response_received, on_error
                               , timeout, message ] 
            ( std::error_code const& failure ) 
        {
            if ( failure )
                on_error( failure );
            else 
                register_temporary_association( response_id, timeout
                                              , on_response_received
                                              , on_error );
        };

        // Serialize the request and send it.
        get_subnet_for( e ).async_send( *message, e, on_request_sent );
    }

    /**
     *
     */
    template< typename Request >
    void
    async_send_request
        ( id const& response_id
        , Request const& request
        , message_socket::endpoint_type const& e )
    { async_send_response( response_id, request, e ); }

    /**
     *
     */
    template< typename Response >
    void
    async_send_response
        ( id const& response_id
        , Response const& response
        , message_socket::endpoint_type const& e )
    { 
        auto message = message_serializer_.serialize( response, response_id );

        // This lamba will keep the response message alive.
        auto on_response_sent = [ message ] 
            ( std::error_code const& failure ) 
        { };

        // Serialize the message and send it.
        get_subnet_for( e ).async_send( *message, e, on_response_sent );
    }

    /**
     *
     */
    template< typename OnResponseReceived, typename OnError >
    void
    register_temporary_association
        ( id const& response_id
        , timer::duration const& association_ttl
        , OnResponseReceived const& on_response_received
        , OnError const& on_error )
    {
        auto on_timeout = [ this, on_error, response_id ]
            ( void ) 
        {
            // If an association has been removed, that means
            // the message has never been received
            // hence report the timeout to the client.
            if ( response_dispatcher_.remove_association( response_id ) )
                on_error( make_error_code( std::errc::timed_out ) );
        };

        // Associate the response id with the 
        // on_response_received callback.
        response_dispatcher_.push_association( response_id
                                             , on_response_received );

        timer_.expires_from_now( association_ttl, on_timeout );
    }

private:
    ///
    std::default_random_engine random_engine_;
    ///
    id my_id_;
    ///
    boost::asio::io_service io_service_;
    ///
    response_dispatcher response_dispatcher_;
    ///
    timer timer_;
    ///
    message_serializer message_serializer_;
    ///
    subnet_type subnet_ipv4_;
    ///
    subnet_type subnet_ipv6_;
    ///
    endpoint initial_peer_;
    ///
    routing_table_type routing_table_;
    ///
    value_store< id, data_type > value_store_;
    ///
    std::error_code main_failure_;
};

} // namespace detail
} // namespace kademlia

#endif
