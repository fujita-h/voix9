#include <iostream>
#include <iomanip>
#include <fstream>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "cmdline.h"
#include <tins/tins.h>
#include <unistd.h>

uint32_t char_to_uint32_le(char *c, int i)
{
    return (uint32_t)((uint8_t)c[i] | (uint8_t)c[i + 1] << 8 | (uint8_t)c[i + 2] << 16 | (uint8_t)c[i + 3] << 24);
}

uint16_t char_to_uint16_le(char *c, int i)
{
    return (uint16_t)((uint8_t)c[i] | (uint8_t)c[i + 1] << 8);
}

int main(int argc, char *argv[])
{
    cmdline::parser cmdline_parser;
    cmdline_parser.add<std::string>("input-wave-file", 'f', "input wave file name", true, "");
    cmdline_parser.add<uint16_t>("payload-time-ms", 'p', "payload time [ms]", false, 20);
    cmdline_parser.add<std::string>("interface", 'i', "input wave file name", true, "");
    cmdline_parser.add<std::string>("destination", 'd', "destination ip", false, "192.168.0.1");
    cmdline_parser.add<int>("sport", '\0', "source port", false, 6000);
    cmdline_parser.add<int>("dport", '\0', "destination port", false, 6002);
    cmdline_parser.add<long>("sleep-adjustment-us", '\0', "sleep adjustment time [us]", false, 0);
    cmdline_parser.parse_check(argc, argv);

    std::ifstream ifs(cmdline_parser.get<std::string>("input-wave-file"), std::ios::binary);
    if (!ifs)
    {
        std::cout << "error" << std::endl;
        return -1;
    }

    ifs.seekg(0, std::ios::end);
    long long int size = ifs.tellg();
    std::cout << "file size: " << size << std::endl;

    if (size < 12)
    {
        std::cout << "file too small." << std::endl;
        ifs.close();
        return -1;
    }

    char *buffer2 = new char(2);
    char *buffer4 = new char(4);

    // RIFF
    ifs.seekg(0, std::ios::beg);
    ifs.read(buffer4, 4);
    const std::string riff_mark = buffer4;

    // RIFF Chunk size
    ifs.seekg(4, std::ios::beg);
    ifs.read(buffer4, 4);
    const uint32_t riff_chunk_size = char_to_uint32_le(buffer4, 0);

    // WAVE
    ifs.seekg(8, std::ios::beg);
    ifs.read(buffer4, 4);
    const std::string wave_mark = buffer4;

    if (riff_mark == "RIFF" && wave_mark == "WAVE")
    {
        uint32_t fmt_chunk_start_pos = 0;
        uint32_t fmt_chunk_size = 0;

        uint32_t data_chunk_start_pos = 0;
        uint32_t data_chunk_size = 0;

        uint16_t loop_guard = 0;
        const uint16_t loop_guard_max = 16;
        uint32_t peek = 12;
        std::string chunk_name = "";
        uint32_t chunk_size = 0;

        while (size > peek && loop_guard < loop_guard_max)
        {
            //std::cout << "peek: " << peek << std::endl;

            ifs.seekg(peek, std::ios::beg);
            ifs.read(buffer4, 4);
            chunk_name = buffer4;
            ifs.seekg(peek + 4, std::ios::beg);
            ifs.read(buffer4, 4);
            chunk_size = char_to_uint32_le(buffer4, 0);

            std::cout << "chunk_name: " << chunk_name << " chunk_size: " << chunk_size << " bytes" << std::endl;

            if (chunk_name == "fmt ")
            {
                fmt_chunk_start_pos = peek + 8;
                fmt_chunk_size = chunk_size;
            }
            else if (chunk_name == "data")
            {
                data_chunk_start_pos = peek + 8;
                data_chunk_size = chunk_size;
            }

            peek = peek + 8 + chunk_size;
            loop_guard++;

            //std::cout << "next-peek: " << peek << std::endl;
        }

        if (loop_guard >= loop_guard_max)
        {
            std::cout << "Error: LOOP GUARD." << peek << std::endl;
            return -1;
        }

        if (fmt_chunk_start_pos == 0 || data_chunk_start_pos == 0)
        {
            std::cout << "Error: chunk not found." << peek << std::endl;
            return -1;
        }

        // fmt chunk parse
        if (fmt_chunk_size < 16)
        {
            std::cout << "Error: fmt chunk error." << peek << std::endl;
            return -1;
        }

        std::cout << "-------------" << std::endl;

        // wave format
        ifs.seekg(fmt_chunk_start_pos, std::ios::beg);
        ifs.read(buffer2, 2);
        const uint16_t wave_format = char_to_uint16_le(buffer2, 0);

        // wave channel
        ifs.seekg(fmt_chunk_start_pos + 2, std::ios::beg);
        ifs.read(buffer2, 2);
        const uint16_t wave_channel = char_to_uint16_le(buffer2, 0);

        // wave sampling rates
        ifs.seekg(fmt_chunk_start_pos + 4, std::ios::beg);
        ifs.read(buffer4, 4);
        const uint32_t wave_rates = char_to_uint32_le(buffer4, 0);

        // wave bps
        ifs.seekg(fmt_chunk_start_pos + 8, std::ios::beg);
        ifs.read(buffer4, 4);
        const uint32_t wave_bps = char_to_uint32_le(buffer4, 0);

        // wave block size
        ifs.seekg(fmt_chunk_start_pos + 12, std::ios::beg);
        ifs.read(buffer2, 2);
        const uint16_t wave_block_size = char_to_uint16_le(buffer2, 0);

        // wave bits
        ifs.seekg(fmt_chunk_start_pos + 14, std::ios::beg);
        ifs.read(buffer2, 2);
        const uint16_t wave_bits = char_to_uint16_le(buffer2, 0);

        std::cout << "wave_format: " << wave_format << std::endl;
        std::cout << "wave_channel: " << wave_channel << std::endl;
        std::cout << "wave_rates: " << wave_rates << std::endl;
        std::cout << "wave_bps: " << wave_bps << std::endl;
        std::cout << "wave_block_size: " << wave_block_size << std::endl;
        std::cout << "wave_bits: " << wave_bits << std::endl;

        if (wave_format == 7 && wave_channel == 1 && wave_rates == 8000 &&
            wave_bps == 8000 && wave_block_size == 1 && wave_bits == 8)
        {
            std::cout << "-------------" << std::endl;
            std::cout << "8bit, 8000Hz, Mono, Mu-Law" << std::endl;

            uint16_t payload_time_ms = cmdline_parser.get<uint16_t>("payload-time-ms");
            uint16_t payload_bytes = 8000 / (1000 / payload_time_ms);
            uint32_t cnt = 0;
            uint16_t sequence = 1;
            uint32_t timestamp = 0;
            uint32_t ssrc = 1234567890;
            Tins::PacketSender sender;
            Tins::NetworkInterface iface(cmdline_parser.get<std::string>("interface"));

            struct timeval cur_time;
            struct timeval prev_time
            {
                0
            };
            struct tm *time_st;

            long start_sec = 0;
            long start_usec = 0;
            long delay_sum = 0;

            while (cnt < data_chunk_size)
            {
                gettimeofday(&cur_time, NULL);
                start_sec = (long)(cur_time.tv_sec);
                start_usec = (long)(cur_time.tv_usec);

                uint16_t read_size = cnt + payload_bytes > data_chunk_size ? data_chunk_size - cnt : payload_bytes;
                char data_buffer[read_size];
                ifs.seekg(data_chunk_start_pos + cnt, std::ios::beg);
                ifs.read(data_buffer, read_size);

                cnt += read_size;

                Tins::RawPDU::payload_type payload;
                payload.reserve(1024);
                payload = {
                    0b10000000, // version(2) = 2, padding(1) = 0, extension(1) = 0, CSRC Count(4) = 0
                    0b00000000, // Marker(1) = 0, Payload Type(7) = 0
                };
                payload.push_back((sequence >> 8) & 0xff);
                payload.push_back(sequence & 0xff);

                payload.push_back((timestamp >> 24) & 0xff);
                payload.push_back((timestamp >> 16) & 0xff);
                payload.push_back((timestamp >> 8) & 0xff);
                payload.push_back(timestamp & 0xff);

                payload.push_back((ssrc >> 24) & 0xff);
                payload.push_back((ssrc >> 16) & 0xff);
                payload.push_back((ssrc >> 8) & 0xff);
                payload.push_back(ssrc & 0xff);

                for (int i = 0; i < read_size; i++)
                {
                    payload.push_back(data_buffer[i]);
                }

                Tins::IP pkt = Tins::IP(cmdline_parser.get<std::string>("destination")) / Tins::UDP(cmdline_parser.get<int>("dport"), cmdline_parser.get<int>("sport")) / Tins::RawPDU(payload);
                sender.send(pkt, iface);

                gettimeofday(&cur_time, NULL);
                time_st = localtime(&cur_time.tv_sec);
                long process_time_usec = ((long)(cur_time.tv_sec) - start_sec) * 1000000 + ((long)(cur_time.tv_usec) - start_usec);
                long measured_payload_interval_us = (prev_time.tv_sec == 0) ? (long)payload_time_ms * (long)1000 : ((long)(cur_time.tv_sec) - (long)(prev_time.tv_sec)) * 1000000 + ((long)(cur_time.tv_usec) - (long)(prev_time.tv_usec));
                long send_delay_us = measured_payload_interval_us - (long)(payload_time_ms * 1000);
                std::cout << std::setfill('0') << std::right << std::setw(2) << time_st->tm_hour;
                std::cout << ":" << std::setfill('0') << std::right << std::setw(2) << time_st->tm_min;
                std::cout << ":" << std::setfill('0') << std::right << std::setw(2) << time_st->tm_sec;
                std::cout << "." << std::setfill('0') << std::right << std::setw(6) << cur_time.tv_usec;
                std::cout << std::setfill(' ');
                std::cout << " seq: " << std::right << std::setw(4) << sequence;
                std::cout << ", size: " << std::right << std::setw(3) << read_size;
                std::cout << ", delay: " << std::right << std::setw(4) << send_delay_us;
                std::cout << ", delay total: " << delay_sum << std::endl;
                delay_sum += send_delay_us;
                prev_time = cur_time;

                sequence++;
                timestamp += read_size;

                usleep(payload_time_ms * 1000 - process_time_usec + cmdline_parser.get<long>("sleep-adjustment-us"));
            }
        }
        else
        {
            std::cout << "-------------" << std::endl;
            std::cout << "Error: invalid format." << std::endl;
            std::cout << "Please use 8bit, 8000Hz, Mono, Mu-Law" << std::endl;
            return -1;
        }
    }

    return 0;
}
