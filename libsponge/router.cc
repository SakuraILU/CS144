#include "router.hh"

#include <iostream>

using namespace std;

// Note: if len is 0, meaning don't need to compare any bytes, just match!
//       this is also fixed a possible fault: n bytes data is supposed move (n-1) bytes right...
//       it's actually a complier error, but i don't know why this marco can prevent the complier from discovering it
//
//       this makes me misunderstand in the test case that this is always a deafault router in route_table,
//       but actually, it's default router bcz it's prefix length is 0......oh..
#define PREFIX(ip, len) (ip >> (sizeof(uint32_t) * 8 - len))
#define MATCH(ip1, ip2, len) ((len == 0) || (PREFIX(ip1, len) == PREFIX(ip2, len)))

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    DUMMY_CODE(route_prefix, prefix_length, next_hop, interface_num);
    // Your code here.
    _route_table.push_back(RouteEntry(route_prefix, prefix_length, next_hop, interface_num));
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    // if dgram's ttl (time to live) is <= 1, this datagram is gonna dead, just return.
    if ((dgram.header().ttl) <= 1)
        return;

    // decrease ttl to avoid possible infinite loop
    --dgram.header().ttl;

    // Entry: [router_prefix, prefix_length, next_hop(Address), interface_num ]
    //   e.g. [172.168.0.0  , 16           , 192.168.16.2     , 3             ]
    //       meaning: through interface 3 to 192.168.16.2 (next hop), we can send
    //                our datagram to network set with prefix 172.168 (aka 172.168.any.any)
    // find the longest prefix entry matched with dgram's dst ip adress...
    RouteEntry entry_matched;
    int16_t max_prefix_len = -1;  // default len is the smaller than any possible prefix length
    for (auto &entry : _route_table) {
        if (MATCH(dgram.header().dst, entry._route_prefix, entry._prefix_length) &&
            entry._prefix_length > max_prefix_len) {
            max_prefix_len = entry._prefix_length;
            entry_matched = entry;
        }
    }

    // no matching entry....ignore this datagram
    if (max_prefix_len == -1)
        return;

    // two case:   if entry's next_hop:
    //    1. have value: this inteface connect to a router, send datagram to this router;
    //    2. don't have value: the interface directly connects a host computer's network interface, thus next_hop is
    //    datagram's dst ip
    if (entry_matched._next_hop.has_value())
        _interfaces[entry_matched._interface_num].send_datagram(dgram, entry_matched._next_hop.value());
    else {
        _interfaces[entry_matched._interface_num].send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {  // traverse all interfaces
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {             // travese all datagram's received by this interface
            route_one_datagram(queue.front());  // route this datagram
            queue.pop();
        }
    }
}
