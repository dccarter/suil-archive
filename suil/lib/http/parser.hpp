//
// Created by dc on 02/04/17.
//

#ifndef SUIL_HTTP_PARSER_HPP
#define SUIL_HTTP_PARSER_HPP


/* merged revision: 5b951d74bd66ec9d38448e0a85b1cf8b85d97db3 */
/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, dup, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#ifdef __cplusplus
extern "C" {
#endif

/* Also update SONAME in the Makefile whenever you change these. */
#define HTTP_PARSER_VERSION_MAJOR 2
#define HTTP_PARSER_VERSION_MINOR 3
#define HTTP_PARSER_VERSION_PATCH 0

#include <sys/types.h>
#if defined(_WIN32) && !defined(__MINGW32__) && (!defined(_MSC_VER) || _MSC_VER < 1600)
#include <BaseTsd.h>
#include <stddef.h>
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

/* Compile with -DHTTP_PARSER_STRICT=0 to make less checks, but start
 * faster
 */
#ifndef     HTTP_PARSER_STRICT
# define    HTTP_PARSER_STRICT 1
#endif

/* Maximium header size allowed. If the macro is not defined
 * before including this header then the default is used. To
 * change the maximum header size, define the macro in the build
 * environment (e.g. -DHTTP_MAX_HEADER_SIZE=<value>). To remove
 * the effective limit on the size of the header, define the macro
 * to a very large number (e.g. -DHTTP_MAX_HEADER_SIZE=0x7fffffff)
 */
#ifndef HTTP_MAX_HEADER_SIZE
# define HTTP_MAX_HEADER_SIZE (80*1024)
#endif

typedef struct http_parser http_parser;
typedef struct http_parser_settings http_parser_settings;


/* Callbacks should return non-zero to indicate an error. The parser will
 * then halt execution.
 *
 * The one exception is on_headers_complete. In a HTTP_RESPONSE parser
 * returning '1' from on_headers_complete will tell the parser that it
 * should not expect a body. This is used when receiving a response to a
 * HEAD request which may contain 'Content-Length' or 'Transfer-Encoding:
 * chunked' headers that indicate the presence of a body.
 *
 * http_data_cb does not return data chunks. It will be call arbitrarally
 * many times for each string. E.G. you might get 10 callbacks for "on_url"
 * each providing just a few characters more data.
 */
typedef int (*http_data_cb)(http_parser *, const char *at, size_t length);
typedef int (*http_cb)(http_parser *);


/* Request Methods */
#define HTTP_METHOD_MAP(_XX)         \
  _XX(0,  DELETE,      DELETE)       \
  _XX(1,  GET,         GET)          \
  _XX(2,  HEAD,        HEAD)         \
  _XX(3,  POST,        POST)         \
  _XX(4,  PUT,         PUT)          \
  /* pathological */                \
  _XX(5,  CONNECT,     CONNECT)      \
  _XX(6,  OPTIONS,     OPTIONS)      \
  _XX(7,  TRACE,       TRACE)        \
  /* webdav */                      \
  _XX(8,  COPY,        COPY)         \
  _XX(9,  LOCK,        LOCK)         \
  _XX(10, MKCOL,       MKCOL)        \
  _XX(11, MOVE,        MOVE)         \
  _XX(12, PROPFIND,    PROPFIND)     \
  _XX(13, PROPPATCH,   PROPPATCH)    \
  _XX(14, SEARCH,      SEARCH)       \
  _XX(15, UNLOCK,      UNLOCK)       \
  /* subversion */                  \
  _XX(16, REPORT,      REPORT)       \
  _XX(17, MKACTIVITY,  MKACTIVITY)   \
  _XX(18, CHECKOUT,    CHECKOUT)     \
  _XX(19, MERGE,       MERGE)        \
  /* upnp */                        \
  _XX(20, MSEARCH,     M-SEARCH)     \
  _XX(21, NOTIFY,      NOTIFY)       \
  _XX(22, SUBSCRIBE,   SUBSCRIBE)    \
  _XX(23, UNSUBSCRIBE, UNSUBSCRIBE)  \
  /* RFC-5789 */                    \
  _XX(24, PATCH,       PATCH)        \
  _XX(25, PURGE,       PURGE)        \
  /* CalDAV */                      \
  _XX(26, MKCALENDAR,  MKCALENDAR)   \

enum http_method {
#define _XX(num, name, string) HTTP_##name = num,
    HTTP_METHOD_MAP(_XX)
#undef _XX
};


enum http_parser_type {
    HTTP_REQUEST, HTTP_RESPONSE, HTTP_BOTH
};


/* Flag values for http_parser.flags field */
enum flags {
    F_CHUNKED = 1 << 0,
    F_CONNECTION_KEEP_ALIVE = 1 << 1,
    F_CONNECTION_CLOSE = 1 << 2,
    F_TRAILING = 1 << 3,
    F_UPGRADE = 1 << 4,
    F_SKIPBODY = 1 << 5
};


/* Map for errno-related constants
 * 
 * The provided argument should be a macro that takes 2 arguments.
 */
#define HTTP_ERRNO_MAP(_XX)                                           \
  /* No error */                                                     \
  _XX(OK, "success")                                                  \
                                                                     \
  /* Callback-related errors */                                      \
  _XX(CB_message_begin, "the on_message_begin callback failed")       \
  _XX(CB_url, "the on_url callback failed")                           \
  _XX(CB_header_field, "the on_header_field callback failed")         \
  _XX(CB_header_value, "the on_header_value callback failed")         \
  _XX(CB_headers_complete, "the on_headers_complete callback failed") \
  _XX(CB_body, "the on_body callback failed")                         \
  _XX(CB_message_complete, "the on_message_complete callback failed") \
  _XX(CB_status, "the on_status callback failed")                     \
                                                                     \
  /* Parsing-related errors */                                       \
  _XX(INVALID_EOF_STATE, "stream ended at an unexpected time")        \
  _XX(HEADER_OVERFLOW,                                                \
     "too many header bytes seen; overflow detected")                \
  _XX(CLOSED_CONNECTION,                                              \
     "data received after completed connection: close message")      \
  _XX(INVALID_VERSION, "invalid HTTP version")                        \
  _XX(INVALID_STATUS, "invalid HTTP status code")                     \
  _XX(INVALID_METHOD, "invalid HTTP method")                          \
  _XX(INVALID_URL, "invalid URL")                                     \
  _XX(INVALID_HOST, "invalid host")                                   \
  _XX(INVALID_PORT, "invalid port")                                   \
  _XX(INVALID_PATH, "invalid path")                                   \
  _XX(INVALID_QUERY_STRING, "invalid query string")                   \
  _XX(INVALID_FRAGMENT, "invalid fragment")                           \
  _XX(LF_EXPECTED, "_LF character expected")                           \
  _XX(INVALID_HEADER_TOKEN, "invalid character in header")            \
  _XX(INVALID_CONTENT_LENGTH,                                         \
     "invalid character in content-length header")                   \
  _XX(INVALID_CHUNK_SIZE,                                             \
     "invalid character in chunk size header")                       \
  _XX(INVALID_CONSTANT, "invalid constant string")                    \
  _XX(INVALID_INTERNAL_STATE, "encountered unexpected internal state")\
  _XX(STRICT, "strict mode assertion failed")                         \
  _XX(PAUSED, "parser is paused")                                     \
  _XX(UNKNOWN, "an unknown error occurred")


/* Define HPE_* values for each errno value above */
#define HTTP_ERRNO_GEN(n, s) HPE_##n,
enum http_errno {
    HTTP_ERRNO_MAP(HTTP_ERRNO_GEN)
};
#undef HTTP_ERRNO_GEN


/* Get an http_errno value from an http_parser */
#define HTTP_PARSER_ERRNO(p)            ((enum http_errno) (p)->http_errno)


struct http_parser {
    /** PRIVATE **/
    unsigned int type : 2;         /* enum http_parser_type */
    unsigned int flags : 6;        /* F_* values from 'flags' enum; semi-public */
    unsigned int state : 8;        /* enum state from http_parser.c */
    unsigned int header_state : 8; /* enum header_state from http_parser.c */
    unsigned int index : 8;        /* index into current matcher */

    uint32_t nread;          /* # bytes read in various scenarios */
    uint64_t content_length; /* # bytes in body (0 if no Content-Length header) */

    /** READ-ONLY **/
    unsigned short http_major;
    unsigned short http_minor;
    unsigned int status_code : 16; /* responses only */
    unsigned int method : 8;       /* requests only */
    unsigned int http_errno : 7;

    /* 1 = Upgrade header was present and the parser has exited because of that.
     * 0 = No upgrade header present.
     * Should be checked when http_parser_execute() returns in addition to
     * error checking.
     */
    unsigned int upgrade : 1;

    /** PUBLIC **/
    void *data; /* A pointer to get hook to the "connection" or "socket" object */
};


struct http_parser_settings {
    http_cb on_message_begin;
    http_data_cb on_url;
    http_data_cb on_status;
    http_data_cb on_header_field;
    http_data_cb on_header_value;
    http_cb on_headers_complete;
    http_data_cb on_body;
    http_cb on_message_complete;
};


enum http_parser_url_fields {
    UF_SCHEMA = 0, UF_HOST = 1, UF_PORT = 2, UF_PATH = 3, UF_QUERY = 4, UF_FRAGMENT = 5, UF_USERINFO = 6, UF_MAX = 7
};


/* Result structure for http_parser_parse_url().
 *
 * Callers should index into field_data[] with UF_* values iff field_set
 * has the relevant (1 << UF_*) bit set. As a courtesy to clients (and
 * because we probably have padding left over), we convert any port to
 * a uint16_t.
 */
struct http_parser_url {
    uint16_t field_set;           /* Bitmask of (1 << UF_*) values */
    uint16_t port;                /* Converted UF_PORT string */

    struct {
        uint16_t off;               /* Offset into buffer in which field starts */
        uint16_t len;               /* Length of start in buffer */
    } field_data[UF_MAX];
};


/* Returns the library version. Bits 16-23 contain the major version number,
 * bits 8-15 the minor version number and bits 0-7 the patch level.
 * Usage example:
 *
 *   unsigned long version = http_parser_version();
 *   unsigned major = (version >> 16) & 255;
 *   unsigned minor = (version >> 8) & 255;
 *   unsigned patch = version & 255;
 *   printf("http_parser v%u.%u.%u\n", major, minor, version);
 */
unsigned long http_parser_version(void);

void http_parser_init(http_parser *parser, enum http_parser_type type);


size_t http_parser_execute(http_parser *parser,
                           const http_parser_settings *settings,
                           const char *data,
                           size_t len);


/* If http_should_keep_alive() in the on_headers_complete or
 * on_message_complete callback returns 0, then this should be
 * the last message on the connection.
 * If you are the server, respond with the "Connection: close" header.
 * If you are the client, close the connection.
 */
int http_should_keep_alive(const http_parser *parser);

/* Returns a string version of the HTTP method. */
const char *http_method_str(enum http_method m);

/* Return a string name of the given error */
const char *http_errno_name(enum http_errno err);

/* Return a string description of the given error */
const char *http_errno_description(enum http_errno err);

/* Parse a URL; return nonzero on failure */
int http_parser_parse_url(const char *buf, size_t buflen,
                          int is_connect,
                          struct http_parser_url *u);

/* Pause or un-pause the parser; a nonzero value pauses */
void http_parser_pause(http_parser *parser, int paused);

/* Checks if this is the final chunk of the body. */
int http_body_is_final(const http_parser *parser);

#ifdef __cplusplus
}
#endif

#include "mem.hpp"
#include "sys.hpp"
#include "http.hpp"

namespace suil {

    namespace http {

        struct parser : public http_parser {
            char *url;
            zcstr_map_t <zcstring> headers;
            query_string qps;
            buffer_t body;

        protected:

            parser(http_parser_type type = HTTP_REQUEST);

            // return false on error
            bool feed(const char *buffer, size_t length);

            virtual void clear();

            inline bool isupgrade() const {
                return upgrade;
            }

            inline bool isversion(int major, int minor) const {
                return http_major == major && http_minor == minor;
            }

            inline bool done() {
                return feed(nullptr, 0);
            }

            virtual int handle_body_part(const char *at, size_t length);

            virtual int msg_complete() {
                content_length = body.size();
            }

            struct {
                uint8_t headers_complete : 1;
                uint8_t body_complete    : 1;
                uint8_t state : 6;
            };

            enum {
                STATE_FIELD = 0,
                STATE_VALUE
            };

            buffer_t hf;
            buffer_t hv;
            buffer_t raw_url;

        private:

            static int on_msg_complete(http_parser *);

            static int on_headers_complete(http_parser *);

            static int on_body(http_parser *, const char *, size_t);

            static int on_header_field(http_parser *, const char *, size_t);

            static int on_header_value(http_parser *, const char *, size_t);

            static int on_url(http_parser *, const char *, size_t);

            static int on_message_begin(http_parser *);

            template<typename __H, typename ...__Mws>
            friend
            struct connection;
        };
    }
}

#endif //SUIL_HTTP_PARSER_HPP
