#ifndef __WS_FRAMEDATA_PARSER__
#define __WS_FRAMEDATA_PARSER__

#include <iostream>
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif
#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif
#include "../basic/ws_framedata.h"
#include "../util/ws_util.h"

namespace dooqu_service
{
namespace net
{
using namespace dooqu_service::util;
using namespace dooqu_service::basic;

enum framedata_parse_result
{
    framedata_ok,
    framedata_error,
    framedata_indeterminate
};

class ws_framedata_parser
{
public:
    framedata_parse_result parse(ws_framedata& frame, size_t data_len)
    {
        //assert(frame.state_ == ws_framedata::ready);
        frame.length += data_len;

        switch (frame.state_)
        {
        case ws_framedata::ready:
            fetch_fin(frame);
            fetch_opcode(frame);
            ++frame.pos_;
            frame.state_ = ws_framedata::fin_and_rsv_ok;

        case ws_framedata::fin_and_rsv_ok:
            if (frame.pos_ >= data_len)
                return framedata_indeterminate;
            fetch_mask(frame);

            fetch_payload_length(frame);

            frame.state_ = ws_framedata::mask_and_payload_len_ok;

        case ws_framedata::mask_and_payload_len_ok:
            if (frame.mask_ == 1)
            {
                if (frame.pos_ + 4 > data_len)
                {
                    return framedata_indeterminate;
                }
                fetch_masking_key(frame);
                frame.pos_ += 4;                
            }
            else
            {
                //return framedata_error;
            }
            frame.data_pos_ = frame.pos_;
            frame.state_ = ws_framedata::mask_key_ok;

        case ws_framedata::mask_key_ok:
            if ((frame.pos_ + frame.payload_length_) > data_len)
            {
                return framedata_indeterminate;
            }
            fetch_payload(frame);
            frame.pos_ += frame.payload_length_;
            frame.start_pos_ = frame.pos_;
            frame.state_ = ws_framedata::payload_data_ok;
            return framedata_ok;

        default:
            return framedata_error;
            break;
        }
        return framedata_error;
    }

    int fetch_fin(ws_framedata& frame)
    {
        frame.fin_ = (unsigned char)frame.data[frame.pos_] >> 7;
        frame.rsv1_ = (unsigned char)frame.data[frame.pos_] & 64;
        frame.rsv2_ = (unsigned char)frame.data[frame.pos_] & 32;
        frame.rsv3_ = (unsigned char)frame.data[frame.pos_] & 16;
        return 0;
    }

    int fetch_opcode(ws_framedata& frame)
    {
        frame.opcode_ = (unsigned char)frame.data[frame.pos_] & 0x0f;
        return 0;
    }

    int fetch_mask(ws_framedata& frame)
    {
        frame.mask_ = (unsigned char)frame.data[frame.pos_] >> 7;
        return 0;
    }

    int fetch_masking_key(ws_framedata& frame)
    {
        for (int i = 0; i < 4; i++)
            frame.masking_key_[i] = frame.data[frame.pos_ + i];

        return 0;
    }

    int fetch_payload_length(ws_framedata& frame)
    {
        frame.payload_length_ = frame.data[frame.pos_] & 0x7f;
        ++frame.pos_;

        if (frame.payload_length_ == 126)
        {
            frame.payload_length_ = ws_util::get_int16_from_net_buf(frame.data + frame.pos_);
            frame.pos_ += 2;

            //std::cout << "126:payload_length:" << frame.payload_length_ << std::endl;
        }
        else if (frame.payload_length_ == 127)
        {
            frame.payload_length_ = ws_util::get_int64_from_net_buf(frame.data + frame.pos_);
            frame.pos_ += 8;
            //std::cout << "127:payload_length:" << frame.payload_length_ << std::endl;
        }

        //std::cout << "payload_length: " << frame.payload_length_ << std::endl;
        return 0;
    }

    int fetch_payload(ws_framedata& frame)
    {
        if (frame.mask_)
        {
            for (int i = 0; i < frame.payload_length_; i++)
            {
                int j = i % 4;
                frame.data[frame.pos_ + i] = frame.data[frame.pos_ + i] ^ frame.masking_key_[j];
            }
        }

        if (frame.opcode_ == 8)
        {
            frame.code = 1005;
            if (frame.payload_length_ >= 2)
            {
                frame.code = dooqu_service::util::ws_util::get_int16_from_net_buf(frame.data + frame.pos_);
                //std::cout << "close code:" << frame.code << std::endl;

                //fill the close reason.
                if (frame.payload_length_ > 2)
                {
                    frame.data[frame.data_pos_ + frame.payload_length_] = 0;
                    frame.reason = &frame.data[frame.data_pos_ + 2];
                    //std::cout << "close reason:" << frame.reason << std::endl;
                }
            }
        }
        return 0;
    }
};
}
}

#endif

