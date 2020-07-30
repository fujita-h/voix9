#ifndef INCLUDE_GUARD_PARSER_HPP
#define INCLUDE_GUARD_PARSER_HPP

#include <iostream>
#include <unordered_map>
#include <vector>
#include <tins/tins.h>
#include "safe-queue.hpp"

class Parser
{
public:
    typedef struct Config
    {
        std::string payload_convert_method = "base64";
    } config_t;

    typedef struct Datagram
    {
        std::string layer_2_type = "";
        std::string layer_2_src_addr = "";
        std::string layer_2_dst_addr = "";

        std::string layer_3_type = "";
        std::string layer_3_src_addr = "";
        std::string layer_3_dst_addr = "";

        std::string layer_4_type = "";
        std::string layer_4_src_port = "";
        std::string layer_4_dst_port = "";

        std::string payload_type = "";
        std::string payload_size = "";
        std::string payload_encoding_type;
        std::string payload = "";
    } datagram_t;

    typedef struct DatasetRtcp
    {
        std::string rtcp_type = "";
        std::string rtcp_ssrc = "";
    } dataset_rtcp_t;

    typedef struct DatasetRtp
    {
        std::string rtp_version = "";
        std::string rtp_marker = "";
        std::string rtp_payload_type = "";
        std::string rtp_sequence_number = "";
        std::string rtp_timestamp = "";
        std::string rtp_ssrc = "";
        std::string rtp_csrc_payload = "";
        std::string rtp_payload = "";

    } dataset_rtp_t;

    typedef struct Dataset
    {
        std::string type = "";
        std::string src_addr = "";
        std::string src_port = "";
        std::string dst_addr = "";
        std::string dst_port = "";

        dataset_rtp_t rtp;

    } dataset_t;

    typedef struct RtpSessionInfo {
        std::string sequence = "";
        long timestamp = 0;
    } rtp_session_info_t;

    Parser(config_t &c, SafeQueue<dataset_t> *s);
    static bool parse(Tins::PDU &pdu);

private:
    config_t config;
    long last_rtp_session_checked;
    std::unordered_map<std::string, rtp_session_info_t> rtp_sessions;
    SafeQueue<dataset_t> *safe_queue;
    static Parser *thisPtr;
    static uint8_t try_parse_rtcp(const Tins::UDP *p, std::vector<dataset_rtcp_t> &d);
    static bool try_parse_rtp(const Tins::UDP *p, dataset_rtp_t &d);
    static std::string pdutype_to_string(const Tins::PDU::PDUType p);
    static std::string uint8_vector_to_base64_string(const std::vector<uint8_t> &v);
    static std::string uint8_vector_to_hex_string(const std::vector<uint8_t> &v);
    static uint16_t uint8_vector_to_uint16(const std::vector<uint8_t> &v, int i);
    static uint32_t uint8_vector_to_uint32(const std::vector<uint8_t> &v, int i);
};

#endif // INCLUDE_GUARD_PARSER_HPP