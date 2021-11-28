# Poco Kademlia Specification

## Background
The Kademlia Distributed Hash Table (DHT) subsystem in libp2p is a DHT implementation largely based on the Kademlia [0] whitepaper.

This specification assumes the reader has prior knowledge of those systems. So rather than explaining DHT mechanics from scratch, we focus on differential areas:

1. DHT Operations: Specialisations and peculiarities of the Poco implementation.
2. Additional Features

## Definitions
- Replication parameter (k)
    
    The amount of replication is governed by the replication parameter k. The recommended value for k is 20.

- Distance

    In all cases, the distance between two keys is XOR(sha1(key1), sha1(key2)).

- Kademlia routing table

    An implementation of this specification must try to maintain k peers with shared key prefix of length L, for every L in [0..(keyspace-length - 1)], in its routing table. Given the keyspace length of 160 through the sha160 hash function, L can take values between 0 (inclusive) and 159 (inclusive). The local node shares a prefix length of 160 with its own key only.

- Alpha concurrency parameter (α)

    The concurrency of node and value lookups are limited by parameter α, with a default value of 3. This implies that each lookup process can perform no more than 3 inflight requests, at any given time.

## DHT operations
The Poco Kademlia DHT offers the following types of operations:

- Ping

    - Checking if a peer is online via PING_REQUEST.
- Peer routing

    - Finding the closest nodes to a given key via FIND_PEER_REQUEST.

- Value storage and retrieval

    - Storing a value on the nodes closest to the value's key by looking up the closest nodes via FIND_PEER_REQUEST and then putting the value to those nodes via STORE_REQUEST.

    - Getting a value by its key from the nodes closest to that key via FIND_VALUE_REQUEST.

- Bootstrapping

    - A peer can be started as a bootstrap peer. When a normal (non bootstrap) peer is started, a bootstrap peer's address must be specified, which is contacted on node startup. The bootstrap peer will send the α peers whose IDs have the least distance from the newly started peer.

## Additional Features
- SHA256 IDs instead of SHA1

- Bootstrapping with versioned routing tables
    -	We will have a routing table, with a sequence number (which will be a timestamp using UTC time).
    - Bootstrap maintains routing table, recording timestamps any time a peer joins.
    - When peer joins, they get entire routing table sent to them as part of the initial handshake.
    - Heartbeat command that a peer sends to bootstrap will include the sequence number of the current routing table it has.
    - Bootstrap will respond with route table updates since five minutes prior to the timestamp sent.

-	Backup bootstraps.
    - They can operate in either HA or failover mode.
    - Failover means one authoritative bootstrap with backups ready to take over if the primary fails.
    - Whichever bootstrap is the oldest will take over next.
    - Age is dictated by timestamp in routing table.
    - HA will constantly communicate with other HA bootstraps.
    - Each HA bootstrap can add a node to the network.
    - Nodes added will be propagated to other bootstraps.
    - Race conditions
        - With HA setup, you could have two peers being added to two different bootstraps at close to the same time, prior to them sending their peer to the other bootstrap. If we use sequential sequence numbers, this creates a collision. 
        - If we use timestamps, collisions realistically won’t happen (we’ll need to timestamp down to the millisecond most likely). This is also why we send a routing table updates to include 5 minutes prior to the previous timestamp sent. 
        - This allows us to get any updates that could have been missed by a race condition where the peer was added to HA bootstrap 2, we request a routing table update from HA bootstrap 1 seconds later, but before HA bootstrap 2 sends its new peer route to HA bootstrap 1. 
        - When we request the next update, even though that new peer is behind the last timestamp we have, it won’t be 5 minutes behind and we’ll still get it. 
        
        [DIAGRAM]

- NAT traversal using message relaying
- Per-key replication supplied with STORE_REQUEST
- Libp2p content provider
- Inter peer communication with transmission interface that can be plugged in (e.g. TCP, RS encoded)
- Send arbitrary data between peers