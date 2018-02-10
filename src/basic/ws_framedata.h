#ifndef __WS_FRAMEDATA_H__
#define __WS_FRAMEDATA_H__
namespace dooqu_service
{
namespace basic
{
#include <stddef.h>

struct ws_framedata
{
    enum opcode
    {
        CONTINUATION = 0x0,
        TEXT = 0x1,
        BINARY = 0x2,
        CLOSE = 0x8,
        PING = 0x9,
        PONG = 0xa
    };

    enum
    {
        BUFFER_SIZE = 1024
    };

    enum state
    {
        ready,
        fin_and_rsv_ok,
        mask_and_payload_len_ok,
        mask_key_ok,
        payload_data_ok
    } state_;

    ws_framedata()
    {
        reset();
    }

    unsigned char fin_;
    unsigned char rsv1_;
    unsigned char rsv2_;
    unsigned char rsv3_;
    unsigned char opcode_;
    unsigned char mask_;
    unsigned char masking_key_[4];
    unsigned long long payload_length_;
    unsigned long long data_pos_;
    unsigned long long pos_;
    unsigned long long length;
    unsigned long long start_pos_;
    unsigned short code;
    char *reason;

    char data[BUFFER_SIZE];

    void reset()
    {
        fin_ = 0;
        rsv1_ = 0;
        rsv2_ = 0;
        rsv3_ = 0;
        opcode_ = 0;
        mask_ = 0;
        payload_length_ = 0;
        data_pos_ = 0;
        pos_ = 0;
        length = 0;
        start_pos_ = 0;
        state_ = ready;
        code = 0;
        reason = NULL;
    }

    char *data_begin()
    {
        return &this->data[this->data_pos_];
    }

    char *data_end()
    {
        return &this->data[this->data_pos_] + this->payload_length_;
    }
};
}
}
#endif