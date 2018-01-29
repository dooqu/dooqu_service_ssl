#ifndef WS_REQUEST_PARSER_H
#define WS_REQUEST_PARSER_H

//#include "ws_request.h"

namespace dooqu_service
{
namespace net
{
class ws_request_parser
{
public:
/*
    enum parse_result
    {
        parse_result_ok,
        parse_result_error,
        parse_result_indeterminate
    };
    enum state
    {
        method_start,
        method,
        uri,
        http_version_h,
        http_version_t_1,
        http_version_t_2,
        http_version_p,
        http_version_slash,
        http_version_major_start,
        http_version_major,
        http_version_minor_start,
        http_version_minor,
        expecting_newline_1,
        header_line_start,
        header_lws,
        header_name,
        space_before_header_value,
        header_value,
        expecting_newline_2,
        expecting_newline_3
    } state_;
*/
    ws_request_parser();


    void reset();
/*
    template <typename InputIterator>
    ws_request_parser::parse_result parse(ws_request& req,
                                          InputIterator begin, InputIterator end)
    {
        ws_request_parser::parse_result ret = parse_result_indeterminate;
        while (begin != end)
        {
            ret = consume(req, *begin++);
            if (ret != parse_result_indeterminate)
                return ret;
        }
        return ret;
    }
    */
private:

    //ws_request_parser::parse_result consume(ws_request& req, char input);

    //static bool is_char(int c);

    //static bool is_ctl(int c);

    //static bool is_tspecial(int c);

    //static bool is_digit(int c);

};
}
}

#endif // WS_REQUEST_PARSER_H
