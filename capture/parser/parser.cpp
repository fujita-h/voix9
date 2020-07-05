#include "parser.hpp"
#include <iomanip>
#include <sstream>
#include <queue>
#include <tins/tins.h>

Parser *Parser::thisPtr;

Parser::Parser(config_t &c, SafeQueue<dataset_t> *s)
{
    Parser::thisPtr = this;
    config = c;
    safe_queue = s;
}

bool Parser::parse(Tins::PDU &pdu)
{
    Parser *_this = Parser::thisPtr;
    dataset_t dataset;

    // TCP?
    const Tins::TCP *tcp_p = pdu.find_pdu<Tins::TCP>();
    if (tcp_p != nullptr)
    {
        dataset.src_port = std::to_string(tcp_p->sport());
        dataset.dst_port = std::to_string(tcp_p->dport());
    }

    // UDP?
    const Tins::UDP *udp_p = pdu.find_pdu<Tins::UDP>();
    if (udp_p != nullptr)
    {
        dataset.src_port = std::to_string(udp_p->sport());
        dataset.dst_port = std::to_string(udp_p->dport());

        dataset_rtp_t dataset_rtp;
        bool x = try_parse_rtp(udp_p, dataset_rtp);
        if (x == true)
        {
            dataset.type = "RTP";
            dataset.rtp = dataset_rtp;
        }
    }

    // IPv4?
    const Tins::IP *ip_p = pdu.find_pdu<Tins::IP>();
    if (ip_p != nullptr)
    {
        dataset.src_addr = ip_p->src_addr().to_string();
        dataset.dst_addr = ip_p->dst_addr().to_string();
    }

    // IPv6?
    const Tins::IPv6 *ipv6_p = pdu.find_pdu<Tins::IPv6>();
    if (ipv6_p != nullptr)
    {
        dataset.src_addr = ipv6_p->src_addr().to_string();
        dataset.dst_addr = ipv6_p->dst_addr().to_string();
    }

    if (dataset.type != "")
    {
        _this->safe_queue->push(dataset);
        std::cout << dataset.type << ":" << ":" << dataset.rtp.rtp_sequence_number << ":" << dataset.rtp.rtp_payload << std::endl;

    }

    return true;
}

bool Parser::try_parse_rtp(const Tins::UDP *p, dataset_rtp_t &d)
{
    uint32_t rtp_header_length = 12;

    const uint32_t &udp_payload_size = p->find_pdu<Tins::RawPDU>()->payload_size();
    if (udp_payload_size < rtp_header_length)
    {
        return false;
    }
    const std::vector<uint8_t> &payload = p->find_pdu<Tins::RawPDU>()->payload();
    const uint8_t &rtp_version = (payload[0] & 0b11000000) >> 6;
    if (rtp_version != 2)
    {
        return false;
    }

    const uint8_t &rtp_padding = (payload[0] & 0b00100000) >> 5;
    const uint8_t &rtp_extension = (payload[0] & 0b00010000) >> 4;
    const uint8_t &rtp_csrc_count = payload[0] & 0b00001111;

    rtp_header_length += 4 * rtp_csrc_count + 4 * rtp_extension;
    if (udp_payload_size < rtp_header_length)
    {
        return false;
    }

    std::vector<uint8_t> rtp_csrc_payload(4 * rtp_csrc_count);
    if (rtp_csrc_count > 0)
    {
        copy(payload.begin() + 12, payload.begin() + 12 + 4 * rtp_csrc_count, rtp_csrc_payload.begin());
    }

    uint16_t rtp_extension_identifier = 0;
    uint16_t rtp_extension_length = 0;
    if (rtp_extension == 1)
    {
        rtp_extension_identifier = payload[rtp_header_length - 4] << 8 | payload[rtp_header_length - 3];
        rtp_extension_length = payload[rtp_header_length - 2] << 8 | payload[rtp_header_length - 1];
    }

    rtp_header_length += 4 * rtp_extension_length;
    if (udp_payload_size < rtp_header_length)
    {
        return false;
    }

    std::vector<uint8_t> rtp_extension_payload(4 * rtp_extension_length);
    if (rtp_extension_length > 0)
    {
        copy(payload.begin() + rtp_header_length - 4 * rtp_extension_length, payload.begin() + rtp_header_length, rtp_extension_payload.begin());
    }

    const uint8_t &rtp_marker = (payload[1] & 0b10000000) >> 7;
    const uint8_t &rtp_payload_type = payload[1] & 0b01111111;
    const uint16_t &rtp_sequence_number = payload[2] << 8 | payload[3];
    const uint32_t &rtp_timestamp = payload[4] << 24 | payload[5] << 16 | payload[6] << 8 | payload[7];
    const uint32_t &rtp_ssrc = payload[8] << 24 | payload[9] << 16 | payload[10] << 8 | payload[11];

    uint8_t rtp_padding_length = 0;
    if (rtp_padding == 1)
    {
        rtp_padding_length = payload[udp_payload_size - 1];
    }

    if (udp_payload_size < rtp_header_length + rtp_padding_length)
    {
        return false;
    }

    std::vector<uint8_t> rtp_payload(udp_payload_size - (rtp_header_length + rtp_padding_length));
    if (udp_payload_size > rtp_header_length + rtp_padding_length)
    {
        copy(payload.begin() + rtp_header_length, payload.end() - rtp_padding_length, rtp_payload.begin());
    }

    d.rtp_version = std::to_string(rtp_version);
    d.rtp_marker = std::to_string(rtp_marker);
    d.rtp_payload_type = std::to_string(rtp_payload_type);
    d.rtp_sequence_number = std::to_string(rtp_sequence_number);
    d.rtp_timestamp = std::to_string(rtp_timestamp);
    d.rtp_ssrc = std::to_string(rtp_ssrc);
    d.rtp_csrc_payload = uint8_vector_to_base64_string(rtp_csrc_payload);
    d.rtp_payload = uint8_vector_to_base64_string(rtp_payload);

    return true;
}

std::string Parser::pdutype_to_string(const Tins::PDU::PDUType p)
{
    switch (p)
    {
    case Tins::PDU::PDUType::RAW:
        return "RAW";
    case Tins::PDU::PDUType::ETHERNET_II:
        return "ETHERNET_II";
    case Tins::PDU::PDUType::IEEE802_3:
        return "IEEE802_3";
    case Tins::PDU::PDUType::RADIOTAP:
        return "RADIOTAP";
    case Tins::PDU::PDUType::DOT11:
        return "DOT11";
    case Tins::PDU::PDUType::DOT11_ACK:
        return "DOT11_ACK";
    case Tins::PDU::PDUType::DOT11_ASSOC_REQ:
        return "DOT11_ASSOC_REQ";
    case Tins::PDU::PDUType::DOT11_ASSOC_RESP:
        return "DOT11_ASSOC_RESP";
    case Tins::PDU::PDUType::DOT11_AUTH:
        return "DOT11_AUTH";
    case Tins::PDU::PDUType::DOT11_BEACON:
        return "DOT11_BEACON";
    case Tins::PDU::PDUType::DOT11_BLOCK_ACK:
        return "DOT11_BLOCK_ACK";
    case Tins::PDU::PDUType::DOT11_BLOCK_ACK_REQ:
        return "DOT11_BLOCK_ACK_REQ";
    case Tins::PDU::PDUType::DOT11_CF_END:
        return "DOT11_CF_END";
    case Tins::PDU::PDUType::DOT11_DATA:
        return "DOT11_DATA";
    case Tins::PDU::PDUType::DOT11_CONTROL:
        return "DOT11_CONTROL";
    case Tins::PDU::PDUType::DOT11_DEAUTH:
        return "DOT11_DEAUTH";
    case Tins::PDU::PDUType::DOT11_DIASSOC:
        return "DOT11_DIASSOC";
    case Tins::PDU::PDUType::DOT11_END_CF_ACK:
        return "DOT11_END_CF_ACK";
    case Tins::PDU::PDUType::DOT11_MANAGEMENT:
        return "DOT11_MANAGEMENT";
    case Tins::PDU::PDUType::DOT11_PROBE_REQ:
        return "DOT11_PROBE_REQ";
    case Tins::PDU::PDUType::DOT11_PROBE_RESP:
        return "DOT11_PROBE_RESP";
    case Tins::PDU::PDUType::DOT11_PS_POLL:
        return "DOT11_PS_POLL";
    case Tins::PDU::PDUType::DOT11_REASSOC_REQ:
        return "DOT11_REASSOC_REQ";
    case Tins::PDU::PDUType::DOT11_REASSOC_RESP:
        return "DOT11_REASSOC_RESP";
    case Tins::PDU::PDUType::DOT11_RTS:
        return "DOT11_RTS";
    case Tins::PDU::PDUType::DOT11_QOS_DATA:
        return "DOT11_QOS_DATA";
    case Tins::PDU::PDUType::LLC:
        return "LLC";
    case Tins::PDU::PDUType::SNAP:
        return "SNAP";
    case Tins::PDU::PDUType::IP:
        return "IP";
    case Tins::PDU::PDUType::ARP:
        return "ARP";
    case Tins::PDU::PDUType::TCP:
        return "TCP";
    case Tins::PDU::PDUType::UDP:
        return "UDP";
    case Tins::PDU::PDUType::ICMP:
        return "ICMP";
    case Tins::PDU::PDUType::BOOTP:
        return "BOOTP";
    case Tins::PDU::PDUType::DHCP:
        return "DHCP";
    case Tins::PDU::PDUType::EAPOL:
        return "EAPOL";
    case Tins::PDU::PDUType::RC4EAPOL:
        return "RC4EAPOL";
    case Tins::PDU::PDUType::RSNEAPOL:
        return "RSNEAPOL";
    case Tins::PDU::PDUType::DNS:
        return "DNS";
    case Tins::PDU::PDUType::LOOPBACK:
        return "LOOPBACK";
    case Tins::PDU::PDUType::IPv6:
        return "IPv6";
    case Tins::PDU::PDUType::ICMPv6:
        return "ICMPv6";
    case Tins::PDU::PDUType::SLL:
        return "SLL";
    case Tins::PDU::PDUType::DHCPv6:
        return "DHCPv6";
    case Tins::PDU::PDUType::DOT1Q:
        return "DOT1Q";
    case Tins::PDU::PDUType::PPPOE:
        return "PPPOE";
    case Tins::PDU::PDUType::STP:
        return "STP";
    case Tins::PDU::PDUType::PPI:
        return "PPI";
    case Tins::PDU::PDUType::IPSEC_AH:
        return "IPSEC_AH";
    case Tins::PDU::PDUType::IPSEC_ESP:
        return "IPSEC_ESP";
    case Tins::PDU::PDUType::PKTAP:
        return "PKTAP";
    case Tins::PDU::PDUType::MPLS:
        return "MPLS";
    case Tins::PDU::PDUType::UNKNOWN:
        return "UNKNOWN";
    case Tins::PDU::PDUType::USER_DEFINED_PDU:
        return "USER_DEFINED_PDU";
    default:
        return "unknown_type_" + std::to_string(p);
    }
}

std::string Parser::uint8_vector_to_hex_string(const std::vector<uint8_t> &v)
{
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    std::vector<uint8_t>::const_iterator it;

    for (it = v.begin(); it != v.end(); it++)
    {
        ss << std::setw(2) << static_cast<unsigned>(*it);
    }

    return ss.str();
}

std::string Parser::uint8_vector_to_base64_string(const std::vector<uint8_t> &v)
{
    const std::string table("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
    std::string cdst;

    for (std::size_t i = 0; i < v.size(); ++i)
    {
        switch (i % 3)
        {
        case 0:
            cdst.push_back(table[(v[i] & 0xFC) >> 2]);
            if (i + 1 == v.size())
            {
                cdst.push_back(table[(v[i] & 0x03) << 4]);
                cdst.push_back('=');
                cdst.push_back('=');
            }

            break;
        case 1:
            cdst.push_back(table[((v[i - 1] & 0x03) << 4) | ((v[i + 0] & 0xF0) >> 4)]);
            if (i + 1 == v.size())
            {
                cdst.push_back(table[(v[i] & 0x0F) << 2]);
                cdst.push_back('=');
            }

            break;
        case 2:
            cdst.push_back(table[((v[i - 1] & 0x0F) << 2) | ((v[i + 0] & 0xC0) >> 6)]);
            cdst.push_back(table[v[i] & 0x3F]);

            break;
        }
    }

    return cdst;
}

uint16_t Parser::uint8_vector_to_uint16(const std::vector<uint8_t> &v, int i)
{
    return (uint16_t)((v[i] << 8) | v[i + 1]);
}

uint32_t Parser::uint8_vector_to_uint32(const std::vector<uint8_t> &v, int i)
{
    return (uint32_t)((((((v[i] << 8) | v[i + 1]) << 8) | v[i + 2]) << 8) | v[i + 3]);
}