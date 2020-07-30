#include "parser.hpp"
#include <cstdlib>
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
    last_rtp_session_checked = 0;
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
        std::vector<dataset_rtcp_t> vec_dataset_rtcp = {};
        if (try_parse_rtp(udp_p, dataset_rtp) == true)
        {
            dataset.type = "RTP";
            dataset.rtp = dataset_rtp;
        }
        else if (try_parse_rtcp(udp_p, vec_dataset_rtcp) > 0)
        {
            dataset.type = "RTCP";
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

    if (dataset.type == "RTP")
    {
        auto now = std::time(nullptr);
        std::string rtp_session_key = dataset.src_addr + ":" + dataset.src_port + "/" + dataset.rtp.rtp_ssrc;

        bool isValid = true;
        auto itr = _this->rtp_sessions.find(rtp_session_key);
        if (itr != _this->rtp_sessions.end())
        {
            if (std::abs(itr->second.timestamp - now) < 6)
            {
                long diff = std::strtol(dataset.rtp.rtp_sequence_number.c_str(), nullptr, 10) - std::strtol(itr->second.sequence.c_str(), nullptr, 10);
                if (diff <= 0 && diff >= -20)
                {
                    isValid = false;
                }
            }
        }

        if (isValid == true)
        {
            _this->safe_queue->push(dataset);
            _this->rtp_sessions[rtp_session_key] = {dataset.rtp.rtp_sequence_number, now};
            std::cout << dataset.type << " "
                      << dataset.src_addr + ":" + dataset.src_port
                      << " -> "
                      << dataset.dst_addr + ":" + dataset.dst_port
                      << " "
                      << dataset.rtp.rtp_sequence_number
                      << std::endl;
        }

        if (now - _this->last_rtp_session_checked > 10)
        {
            for (auto itr = _this->rtp_sessions.begin(); itr != _this->rtp_sessions.end(); itr)
            {
                if (std::abs(itr->second.timestamp - now) > 10)
                {
                    itr = _this->rtp_sessions.erase(itr);
                    std::cout << "delete" << std::endl;
                }
                else
                {
                    ++itr;
                }
            }
            _this->last_rtp_session_checked = now;
        }
    }
    else if (dataset.type != "")
    {
        _this->safe_queue->push(dataset);
        std::cout << dataset.type << " "
                  << dataset.src_addr + ":" + dataset.src_port
                  << " -> "
                  << dataset.dst_addr + ":" + dataset.dst_port
                  << std::endl;
    }

    return true;
}

uint8_t Parser::try_parse_rtcp(const Tins::UDP *p, std::vector<dataset_rtcp_t> &d)
{
    uint8_t result = 0;
    const uint32_t &udp_payload_size = p->find_pdu<Tins::RawPDU>()->payload_size();
    const std::vector<uint8_t> &payload = p->find_pdu<Tins::RawPDU>()->payload();
    uint32_t cur_pos = 0;

    while (cur_pos + 4 < udp_payload_size)
    {
        const uint8_t &rtcp_version = (payload[cur_pos + 0] & 0b11000000) >> 6;
        const uint8_t &rtcp_padding = (payload[cur_pos + 0] & 0b00100000) >> 5;
        const uint8_t &rtcp_report_count = payload[cur_pos + 0] & 0b00011111;
        const uint8_t &rtcp_packet_type = payload[cur_pos + 1];
        const uint16_t &rtcp_length = payload[cur_pos + 2] << 8 | payload[cur_pos + 3];
        uint32_t rtcp_ssrc = 0;

        if (rtcp_version != 2)
        {
            break;
        }

        if (rtcp_packet_type == 200)
        {
            // sender report
            dataset_rtcp_t rtcp;
            rtcp.rtcp_type = "SR";

            if (udp_payload_size < cur_pos + rtcp_length)
            {
                break;
            }

            rtcp_ssrc = payload[cur_pos + 4] << 24 | payload[cur_pos + 5] << 16 | payload[cur_pos + 6] << 8 | payload[cur_pos + 7];
            rtcp.rtcp_ssrc = std::to_string(rtcp_ssrc);

            d.push_back(rtcp);
            result++;
            cur_pos = cur_pos + 4 * (rtcp_length + 1);
        }
        else if (rtcp_packet_type == 201)
        {
            // receiver report
            dataset_rtcp_t rtcp;
            rtcp.rtcp_type = "RR";
            if (udp_payload_size < cur_pos + rtcp_length)
            {
                break;
            }

            rtcp_ssrc = payload[cur_pos + 4] << 24 | payload[cur_pos + 5] << 16 | payload[cur_pos + 6] << 8 | payload[cur_pos + 7];
            rtcp.rtcp_ssrc = std::to_string(rtcp_ssrc);

            d.push_back(rtcp);
            result++;
            cur_pos = cur_pos + 4 * (rtcp_length + 1);
        }
        else if (rtcp_packet_type == 202)
        {
            // SDES
            dataset_rtcp_t rtcp;
            rtcp.rtcp_type = "SDES";
            uint32_t sdes_pos = cur_pos + 4;

            // MORE DEBUG REQUIRED!
            for (int i = 0; i < rtcp_report_count; i++)
            {
                uint32_t sdes_ssrc = payload[sdes_pos] << 24 | payload[sdes_pos + 1] << 16 | payload[sdes_pos + 2] << 8 | payload[sdes_pos + 3];
                sdes_pos += 4;
                uint8_t sdes_type = payload[sdes_pos];
                while (sdes_type != 0 && sdes_pos < udp_payload_size)
                {
                    std::cout << "SDES_TYPE: " << std::to_string(sdes_type) << std::endl;
                    uint8_t sdes_len = payload[sdes_pos + 1];
                    sdes_pos = sdes_pos + 2 + sdes_len;
                    sdes_type = payload[sdes_pos];
                }
            }

            d.push_back(rtcp);
            result++;
            cur_pos = cur_pos + 4 * (rtcp_length + 1);
        }
        else if (rtcp_packet_type == 203)
        {
            // goodbye
            dataset_rtcp_t rtcp;
            rtcp.rtcp_type = "GOODBYE";

            d.push_back(rtcp);
            result++;
            cur_pos = cur_pos + 4 * (rtcp_length + 1);
        }
        else if (rtcp_packet_type == 204)
        {
            // app
            dataset_rtcp_t rtcp;
            rtcp.rtcp_type = "APP";

            d.push_back(rtcp);
            result++;
            cur_pos = cur_pos + 4 * (rtcp_length + 1);
        }
        else
        {
            break;
        }
    }

    return result;
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

    if (rtp_payload_type >= 72 && rtp_payload_type <= 76)
    {
        // rtp_payload_type: 72 - 76 is not RTP. This is RTCP.
        return false;
    }

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