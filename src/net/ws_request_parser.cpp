#include "ws_request_parser.h"

namespace dooqu_service
{
namespace net
{

ws_request_parser::ws_request_parser()
    : state_(method_start)
{
}

void ws_request_parser::reset()
{
    state_ = method_start;
}

ws_request_parser::parse_result ws_request_parser::consume(ws_request& req, char input)
{

    switch (state_)
    {
    case method_start:
        if (!is_char(input) || is_ctl(input) || is_tspecial(input))
        {
            return parse_result_error;
        }
        else
        {
            state_ = method;
            req.method.push_back(input);
            return parse_result_indeterminate;
        }
    case method:
        if (input == ' ')
        {
            state_ = uri;
            return parse_result_indeterminate;
        }
        else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
        {
            return parse_result_error;
        }
        else
        {
            req.method.push_back(input);
            return parse_result_indeterminate;
        }
    case uri:
        if (input == ' ')
        {
            state_ = http_version_h;
            return parse_result_indeterminate;
        }
        else if (is_ctl(input))
        {
            return parse_result_error;
        }
        else
        {
            req.uri.push_back(input);
            return parse_result_indeterminate;
        }
    case http_version_h:
        if (input == 'H')
        {
            state_ = http_version_t_1;
            return parse_result_indeterminate;
        }
        else
        {
            return parse_result_error;
        }
    case http_version_t_1:
        if (input == 'T')
        {
            state_ = http_version_t_2;
            return parse_result_indeterminate;
        }
        else
        {
            return parse_result_error;
        }
    case http_version_t_2:
        if (input == 'T')
        {
            state_ = http_version_p;
            return parse_result_indeterminate;
        }
        else
        {
            return parse_result_error;
        }
    case http_version_p:
        if (input == 'P')
        {
            state_ = http_version_slash;
            return parse_result_indeterminate;
        }
        else
        {
            return parse_result_error;
        }
    case http_version_slash:
        if (input == '/')
        {
            req.http_version_major = 0;
            req.http_version_minor = 0;
            state_ = http_version_major_start;
            return parse_result_indeterminate;
        }
        else
        {
            return parse_result_error;
        }
    case http_version_major_start:
        if (is_digit(input))
        {
            req.http_version_major = req.http_version_major * 10 + input - '0';
            state_ = http_version_major;
            return parse_result_indeterminate;
        }
        else
        {
            return parse_result_error;
        }
    case http_version_major:
        if (input == '.')
        {
            state_ = http_version_minor_start;
            return parse_result_indeterminate;
        }
        else if (is_digit(input))
        {
            req.http_version_major = req.http_version_major * 10 + input - '0';
            return parse_result_indeterminate;
        }
        else
        {
            return parse_result_error;
        }
    case http_version_minor_start:
        if (is_digit(input))
        {
            req.http_version_minor = req.http_version_minor * 10 + input - '0';
            state_ = http_version_minor;
            return parse_result_indeterminate;
        }
        else
        {
            return parse_result_error;
        }
    case http_version_minor:
        if (input == '\r')
        {
            state_ = expecting_newline_1;
            return parse_result_indeterminate;
        }
        else if (is_digit(input))
        {
            req.http_version_minor = req.http_version_minor * 10 + input - '0';
            return parse_result_indeterminate;
        }
        else
        {
            return parse_result_error;
        }
    case expecting_newline_1:
        if (input == '\n')
        {
            state_ = header_line_start;
            return parse_result_indeterminate;
        }
        else
        {
            return parse_result_error;
        }
    case header_line_start:
        if (input == '\r')
        {
            state_ = expecting_newline_3;
            return parse_result_indeterminate;
        }
        else if (!req.headers.empty() && (input == ' ' || input == '\t'))
        {
            state_ = header_lws;
            return parse_result_indeterminate;
        }
        else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
        {
            return parse_result_error;
        }
        else
        {
            req.headers.push_back(header());
            req.headers.back().name.push_back(input);
            state_ = header_name;
            return parse_result_indeterminate;
        }
    case header_lws:
        if (input == '\r')
        {
            state_ = expecting_newline_2;
            return parse_result_indeterminate;
        }
        else if (input == ' ' || input == '\t')
        {
            return parse_result_indeterminate;
        }
        else if (is_ctl(input))
        {
            return parse_result_error;
        }
        else
        {
            state_ = header_value;
            req.headers.back().value.push_back(input);
            return parse_result_indeterminate;
        }
    case header_name:
        if (input == ':')
        {
            state_ = space_before_header_value;
            return parse_result_indeterminate;
        }
        else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
        {
            return parse_result_error;
        }
        else
        {
            req.headers.back().name.push_back(input);
            return parse_result_indeterminate;
        }
    case space_before_header_value:
        if (input == ' ')
        {
            state_ = header_value;
            return parse_result_indeterminate;
        }
        else
        {
            return parse_result_error;
        }
    case header_value:
        if (input == '\r')
        {
            state_ = expecting_newline_2;
            return parse_result_indeterminate;
        }
        else if (is_ctl(input))
        {
            return parse_result_error;
        }
        else
        {
            req.headers.back().value.push_back(input);
            return parse_result_indeterminate;
        }
    case expecting_newline_2:
        if (input == '\n')
        {
            state_ = header_line_start;
            return parse_result_indeterminate;
        }
        else
        {
            return parse_result_error;
        }
    case expecting_newline_3:
        if (input == '\n')
        {
            return parse_result_ok;
        }
        return parse_result_error;
    default:
        return parse_result_error;
    }


    return parse_result_ok;
}

bool ws_request_parser::is_char(int c)
{
    return c >= 0 && c <= 127;
}

bool ws_request_parser::is_ctl(int c)
{
    return (c >= 0 && c <= 31) || (c == 127);
}

bool ws_request_parser::is_tspecial(int c)
{

    switch (c)
    {
    case '(':
    case ')':
    case '<':
    case '>':
    case '@':
    case ',':
    case ';':
    case ':':
    case '\\':
    case '"':
    case '/':
    case '[':
    case ']':
    case '?':
    case '=':
    case '{':
    case '}':
    case ' ':
    case '\t':
        return true;
    default:
        return false;
    }


    return false;
}

bool ws_request_parser::is_digit(int c)
{
    return c >= '0' && c <= '9';
}

}
}
