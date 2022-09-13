#include "network_interface.hh"

#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "[raw: " << ip_address.ipv4_numeric() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    // DUMMY_CODE(dgram, next_hop, next_hop_ip);
    EthernetFrame frame;
    // if ip--arp table has entry, send this datagram...
    if (_ip2ethernet.find(next_hop_ip) != _ip2ethernet.end()) {
        frame.header().src = _ethernet_address;
        frame.header().dst = _ip2ethernet[next_hop_ip];
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.payload() = dgram.serialize();
        _frames_out.push(std::move(frame));
        // if don't, send arp to ask mac adress of this ip
    } else if (_ip2ethernet.find(next_hop_ip) == _ip2ethernet.end()) {
        // set clock if this ip don't has a clock.
        if (_timers.find(next_hop_ip) == _timers.end())
            _timers.insert(make_pair(next_hop_ip, Timer(false)));
        // push this dgram in buffer, bcz don't know where to send
        _timers[next_hop_ip]._dgrams.push_back(dgram);

        // if is not in pending state ( 5s after sending a arp request), send an arp request
        if (!_timers[next_hop_ip]._is_pending) {
            ARPMessage arp;
            arp.opcode = ARPMessage::OPCODE_REQUEST;
            arp.sender_ip_address = _ip_address.ipv4_numeric();  // local network interface's ip and mac adress
            arp.sender_ethernet_address = _ethernet_address;
            arp.target_ip_address = next_hop_ip;  // set the target ip adress of this dgram, ask it's mac adress

            frame.header().src = _ethernet_address;
            frame.header().dst = ETHERNET_BROADCAST;
            frame.header().type = EthernetHeader::TYPE_ARP;
            frame.payload() = arp.serialize();

            _frames_out.push(std::move(frame));

            reset_clock(next_hop_ip, PENDING);
        }
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // adress check!! if dst mac is not local network interface's mac adress or not a broadcast mac adress
    // frame is not our's cake, just ignore it
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST)
        return nullopt;

    optional<InternetDatagram> dgram(nullopt);
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        // it's an IPV4 datagram, receive
        InternetDatagram dgram_recv;
        dgram_recv.parse(frame.payload());
        dgram = dgram_recv;
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        // ARP request/reply
        dgram = nullopt;
        ARPMessage arp;
        arp.parse(frame.payload());
        if (arp.opcode == ARPMessage::OPCODE_REPLY) {
            // get our arp request's reply, set ip--mac entry and disalbe timer's pending
            _ip2ethernet.insert(make_pair(arp.sender_ip_address, arp.sender_ethernet_address));
            reset_clock(arp.sender_ip_address, NO_PENDING);
            // send this ip's all buffered datagrams
            send_dgrams_in_buffer(arp);
        } else if (arp.opcode == ARPMessage::OPCODE_REQUEST) {
            // see if target ip adress is this net interface's, if not, ignore
            if (arp.target_ip_address != _ip_address.ipv4_numeric())
                return nullopt;

            // record sender's ip--mac entry, bcz very possible, we'll talk with it later
            _ip2ethernet[arp.sender_ip_address] = arp.sender_ethernet_address;
            // set this ip's clock.
            // no pend! bcz we have its mac adress already... pending is used to avoid reask the same ip's mac adress
            // too frequently
            if (_timers.find(arp.sender_ip_address) == _timers.end())
                _timers.insert(make_pair(arp.sender_ip_address, Timer(false)));
            else
                reset_clock(arp.sender_ip_address, false);

            // send the arp rely back, so we are sender, and they are target...
            arp.target_ethernet_address = arp.sender_ethernet_address;
            arp.target_ip_address = arp.sender_ip_address;
            arp.sender_ethernet_address = _ethernet_address;
            arp.sender_ip_address = _ip_address.ipv4_numeric();
            arp.opcode = ARPMessage::OPCODE_REPLY;  // remember to set opcode to OPCODE_REPLY(2)

            EthernetFrame frame_reply;
            frame_reply.header().src = _ethernet_address;
            frame_reply.header().dst = arp.target_ethernet_address;
            frame_reply.header().type = EthernetHeader::TYPE_ARP;
            frame_reply.payload() = arp.serialize();
            _frames_out.push(std::move(frame_reply));

            send_dgrams_in_buffer(arp);
        }
    }
    return dgram;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    // DUMMY_CODE(ms_since_last_tick);

    // traverse timers
    for (auto itr = _timers.begin(); itr != _timers.end(); ++itr) {
        itr->second._timer += ms_since_last_tick;  // update timer

        if (itr->second._is_pending && itr->second._timer > 5000) {
            // check pending state, if > 5s, don't pend
            reset_clock(itr->first, NO_PENDING);
        } else if (!itr->second._is_pending && itr->second._timer >= 30000) {
            // if don't pend and clock time > 30s, delete this ip's ip--mac entry
            // note: a little tricky,
            //       if no pending and arp is not sent yet (this ip's dgram is not asked to sent afterward... but I
            //       believe upper protocal stack like TCP will ask to resend again if no ack is got )..., ip--mac entry
            //       is not exist, but erase will still be fine, bcz it just do nothing if not exisit...
            //
            //       if no pending and arp is sent, clock will be reset to [0ms, NO_PENDING], the timer's time is
            //       correct...
            _ip2ethernet.erase(itr->first);
        }
    }
}

void NetworkInterface::reset_clock(uint32_t next_hop_ip, bool is_pending) {
    _timers[next_hop_ip]._is_pending = is_pending;
    _timers[next_hop_ip]._timer = 0;
}

void NetworkInterface::send_dgrams_in_buffer(const ARPMessage &arp) {
    if (arp.opcode != ARPMessage::OPCODE_REPLY)
        return;
    // when get arp reply, ip--mac entry can be resolved,
    // thus send this ip's all datagram out
    while (!_timers[arp.sender_ip_address]._dgrams.empty()) {
        IPv4Datagram &dgram = _timers[arp.sender_ip_address]._dgrams.back();
        EthernetFrame frame_send;
        frame_send.header().src = _ethernet_address;
        frame_send.header().dst = arp.sender_ethernet_address;  // the arp reply sender is who we want to send datagrams
        frame_send.header().type = EthernetHeader::TYPE_IPv4;
        frame_send.payload() = dgram.serialize();
        _frames_out.push(std::move(frame_send));

        _timers[arp.sender_ip_address]._dgrams.pop_back();
    }
}