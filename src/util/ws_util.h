#ifndef WS_UTIL_H
#define WS_UTIL_H
#include <cstdint>
#include <cstddef>
#include <locale>
#include <utility>
#include <codecvt>
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <arpa/inet.h>
#endif
#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

namespace dooqu_service
{
namespace util
{
class ws_util
{
    template <int N, typename T>
    struct static_for
    {
        void operator()(uint32_t *a, uint32_t *b)
        {
            static_for<N - 1, T>()(a, b);
            T::template f<N - 1>(a, b);
        }
    };

    template <typename T>
    struct static_for<0, T>
    {
        void operator()(uint32_t *a, uint32_t *hash) {}
    };

    template <int state>
    struct Sha1Loop
    {
        static inline uint32_t rol(uint32_t value, size_t bits)
        {
            return (value << bits) | (value >> (32 - bits));
        }
        static inline uint32_t blk(uint32_t b[16], size_t i)
        {
            return rol(b[(i + 13) & 15] ^ b[(i + 8) & 15] ^ b[(i + 2) & 15] ^ b[i], 1);
        }

        template <int i>
        static inline void f(uint32_t *a, uint32_t *b)
        {
            switch (state)
            {
            case 1:
                a[i % 5] += ((a[(3 + i) % 5] & (a[(2 + i) % 5] ^ a[(1 + i) % 5])) ^ a[(1 + i) % 5]) + b[i] + 0x5a827999 + rol(a[(4 + i) % 5], 5);
                a[(3 + i) % 5] = rol(a[(3 + i) % 5], 30);
                break;
            case 2:
                b[i] = blk(b, i);
                a[(1 + i) % 5] += ((a[(4 + i) % 5] & (a[(3 + i) % 5] ^ a[(2 + i) % 5])) ^ a[(2 + i) % 5]) + b[i] + 0x5a827999 + rol(a[(5 + i) % 5], 5);
                a[(4 + i) % 5] = rol(a[(4 + i) % 5], 30);
                break;
            case 3:
                b[(i + 4) % 16] = blk(b, (i + 4) % 16);
                a[i % 5] += (a[(3 + i) % 5] ^ a[(2 + i) % 5] ^ a[(1 + i) % 5]) + b[(i + 4) % 16] + 0x6ed9eba1 + rol(a[(4 + i) % 5], 5);
                a[(3 + i) % 5] = rol(a[(3 + i) % 5], 30);
                break;
            case 4:
                b[(i + 8) % 16] = blk(b, (i + 8) % 16);
                a[i % 5] += (((a[(3 + i) % 5] | a[(2 + i) % 5]) & a[(1 + i) % 5]) | (a[(3 + i) % 5] & a[(2 + i) % 5])) + b[(i + 8) % 16] + 0x8f1bbcdc + rol(a[(4 + i) % 5], 5);
                a[(3 + i) % 5] = rol(a[(3 + i) % 5], 30);
                break;
            case 5:
                b[(i + 12) % 16] = blk(b, (i + 12) % 16);
                a[i % 5] += (a[(3 + i) % 5] ^ a[(2 + i) % 5] ^ a[(1 + i) % 5]) + b[(i + 12) % 16] + 0xca62c1d6 + rol(a[(4 + i) % 5], 5);
                a[(3 + i) % 5] = rol(a[(3 + i) % 5], 30);
                break;
            case 6:
                b[i] += a[4 - i];
            }
        }
    };

    static inline void sha1(uint32_t hash[5], uint32_t b[16])
    {
        uint32_t a[5] = { hash[4], hash[3], hash[2], hash[1], hash[0] };
        static_for<16, Sha1Loop<1>>()(a, b);
        static_for<4, Sha1Loop<2>>()(a, b);
        static_for<20, Sha1Loop<3>>()(a, b);
        static_for<20, Sha1Loop<4>>()(a, b);
        static_for<20, Sha1Loop<5>>()(a, b);
        static_for<5, Sha1Loop<6>>()(a, hash);
    }

    static inline void base64(unsigned char *src, char *dst)
    {
        const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 18; i += 3)
        {
            *dst++ = b64[(src[i] >> 2) & 63];
            *dst++ = b64[((src[i] & 3) << 4) | ((src[i + 1] & 240) >> 4)];
            *dst++ = b64[((src[i + 1] & 15) << 2) | ((src[i + 2] & 192) >> 6)];
            *dst++ = b64[src[i + 2] & 63];
        }
        *dst++ = b64[(src[18] >> 2) & 63];
        *dst++ = b64[((src[18] & 3) << 4) | ((src[19] & 240) >> 4)];
        *dst++ = b64[((src[19] & 15) << 2)];
        *dst++ = '=';
    }

public:
    static inline void generate(const char input[24], char output[28])
    {
        uint32_t b_output[5] =
        {
            0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0
        };
        uint32_t b_input[16] =
        {
            0, 0, 0, 0, 0, 0, 0x32353845, 0x41464135, 0x2d453931, 0x342d3437, 0x44412d39,
            0x3543412d, 0x43354142, 0x30444338, 0x35423131, 0x80000000
        };

        for (int i = 0; i < 6; i++)
        {
            b_input[i] = (input[4 * i + 3] & 0xff) | (input[4 * i + 2] & 0xff) << 8 | (input[4 * i + 1] & 0xff) << 16 | (input[4 * i + 0] & 0xff) << 24;
        }
        sha1(b_output, b_input);
        uint32_t last_b[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 480 };
        sha1(b_output, last_b);
        for (int i = 0; i < 5; i++)
        {
            uint32_t tmp = b_output[i];
            char *bytes = (char *)&b_output[i];
            bytes[3] = tmp & 0xff;
            bytes[2] = (tmp >> 8) & 0xff;
            bytes[1] = (tmp >> 16) & 0xff;
            bytes[0] = (tmp >> 24) & 0xff;
        }
        base64((unsigned char *)b_output, output);
    }

	static std::string encode_to_utf8(wchar_t* wstr)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
		return cvt.to_bytes(wstr);
	}

	template<typename START, typename END>
	static void wprint(START begin, END end)
	{
		//std::wcout.imbue(std::locale("chs"));
		std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
		std::wstring rep = conv.from_bytes(begin, end);

		std::wcout << "content:" << rep << std::endl;
	}

	static unsigned short get_int16_from_net_buf(char* error_buf)
	{
		unsigned short status;
		memcpy(&status, error_buf, 2);
		return ntohs(status);
	}

	static void fill_net_buf_by_int16(char* buffer, unsigned short error_code)
	{
		error_code = htons(error_code);
		memcpy(buffer, &error_code, 2);
	}

	static unsigned long get_int64_from_net_buf(char* error_buf)
	{
		unsigned short status;
		memcpy(&status, error_buf, 8);
		return ntohl(status);
	}

	static void fill_net_buf_by_int64(char* buffer, unsigned long error_code)
	{
		error_code = htonl(error_code);
		memcpy(buffer, &error_code, 8);
	}
};
}
}

#endif // WS_UTIL_H
