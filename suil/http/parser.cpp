//
// Created by dc on 28/06/17.
//

#include <suil/http/parser.hpp>

//
// Created by dc on 02/04/17.
//

/* Based on src/http/ngxHTTP_parse.c from NGINX copyright Igor Sysoev
 *
 * Additional changes are licensed under the same terms as NGINX and
 * copyright Joyent, Inc. and other Node contributors. All rights reserved.
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

#ifndef _ULLONG_MAX
# define _ULLONG_MAX ((uint64_t) -1) /* 2^64-1 */
#endif

#ifndef _MIN
# define _MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef _ARRAY_SIZE
# define _ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef _BIT_AT
# define _BIT_AT(a, i)                                                \
  (!!((unsigned int) (a)[(unsigned int) (i) >> 3] &                  \
   (1 << ((unsigned int) (i) & 7))))
#endif

#ifndef _ELEM_AT
# define _ELEM_AT(a, i, v) ((unsigned int) (i) < _ARRAY_SIZE(a) ? (a)[(i)] : (v))
#endif

#define _SET_ERRNO(e)                                                 \
do {                                                                 \
  parser->http_errno = (e);                                          \
} while(0)


/* Run the notify callback FOR, returning ER if it fails */
#define _CALLBACK_NOTIFY_(FOR, ER)                                    \
do {                                                                 \
  assert(HTTP_PARSER_ERRNO(parser) == HPE_OK);                       \
                                                                     \
  if (settings->on_##FOR) {                                          \
    if (0 != settings->on_##FOR(parser)) {                           \
      _SET_ERRNO(HPE_CB_##FOR);                                       \
    }                                                                \
                                                                     \
    /* We either errored above or got paused; get out */             \
    if (HTTP_PARSER_ERRNO(parser) != HPE_OK) {                       \
      return (ER);                                                   \
    }                                                                \
  }                                                                  \
} while (0)

/* Run the notify callback FOR and consume the current byte */
#define _CALLBACK_NOTIFY(FOR)            _CALLBACK_NOTIFY_(FOR, p - data + 1)

/* Run the notify callback FOR and don't consume the current byte */
#define _CALLBACK_NOTIFY_NOADVANCE(FOR)  _CALLBACK_NOTIFY_(FOR, p - data)

/* Run data callback FOR with LEN bytes, returning ER if it fails */
#define _CALLBACK_DATA_(FOR, LEN, ER)                                 \
do {                                                                 \
  assert(HTTP_PARSER_ERRNO(parser) == HPE_OK);                       \
                                                                     \
  if (FOR##_mark) {                                                  \
    if (settings->on_##FOR) {                                        \
      if (0 != settings->on_##FOR(parser, FOR##_mark, (LEN))) {      \
        _SET_ERRNO(HPE_CB_##FOR);                                     \
      }                                                              \
                                                                     \
      /* We either errored above or got paused; get out */           \
      if (HTTP_PARSER_ERRNO(parser) != HPE_OK) {                     \
        return (ER);                                                 \
      }                                                              \
    }                                                                \
    FOR##_mark = NULL;                                               \
  }                                                                  \
} while (0)

/* Run the data callback FOR and consume the current byte */
#define _CALLBACK_DATA(FOR)                                           \
    _CALLBACK_DATA_(FOR, p - FOR##_mark, p - data + 1)

/* Run the data callback FOR and don't consume the current byte */
#define _CALLBACK_DATA_NOADVANCE(FOR)                                 \
    _CALLBACK_DATA_(FOR, p - FOR##_mark, p - data)

/* Set the mark FOR; non-destructive if mark is already set */
#define _MARK(FOR)                                                    \
do {                                                                 \
  if (!FOR##_mark) {                                                 \
    FOR##_mark = p;                                                  \
  }                                                                  \
} while (0)


#define _PROXY_CONNECTION "proxy-Connection"
#define _CONNECTION "Connection"
#define _CONTENT_LENGTH "content-length"
#define _TRANSFER_ENCODING "transfer-encoding"
#define _UPGRADE "upgrade"
#define _CHUNKED "chunked"
#define _KEEP_ALIVE "keep-alive"
#define _CLOSE "close"


enum state {
    s_dead = 1 /* important that this is > 0 */

    ,
    s_start_req_or_res,
    s_res_or_resp_H,
    s_start_res,
    s_res_H,
    s_res_HT,
    s_res_HTT,
    s_resHTTP,
    s_res_firstHTTP_major,
    s_resHTTP_major,
    s_res_firstHTTP_minor,
    s_resHTTP_minor,
    s_res_first_status_code,
    s_res_status_code,
    s_res_status_start,
    s_res_status,
    s_res_line_almost_done,
    s_start_req,
    s_req_method,
    s_req_spaces_before_url,
    s_req_schema,
    s_req_schema_slash,
    s_req_schema_slash_slash,
    s_req_server_start,
    s_req_server,
    s_req_server_with_at,
    s_req_path,
    s_req_query_string_start,
    s_req_query_string,
    s_req_fragment_start,
    s_req_fragment,
    s_reqHTTP_start,
    s_reqHTTP_H,
    s_reqHTTP_HT,
    s_reqHTTP_HTT,
    s_reqHTTPHTTP,
    s_req_firstHTTP_major,
    s_reqHTTP_major,
    s_req_firstHTTP_minor,
    s_reqHTTP_minor,
    s_req_line_almost_done,
    s_header_field_start,
    s_header_field,
    s_header_value_discard_ws,
    s_header_value_discard_ws_almost_done,
    s_header_value_discard_lws,
    s_header_value_start,
    s_header_value,
    s_header_value_lws,
    s_header_almost_done,
    s_chunk_size_start,
    s_chunk_size,
    s_chunk_parameters,
    s_chunk_size_almost_done,
    s_headers_almost_done,
    s_headers_done

    /* Important: 's_headers_done' must be the last 'header' state. All
     * states beyond this must be 'body' states. It is used for overflow
     * checking. See the PARSING_HEADER() macro.
     */

    ,
    s_chunk_data,
    s_chunk_data_almost_done,
    s_chunk_data_done,
    s_body_identity,
    s_body_identity_eof,
    s_message_done
};


#define PARSING_HEADER(state) (state <= s_headers_done)


enum header_states {
    h_general = 0,
    h_C,
    h_CO,
    h_CON,
    h_matching_connection,
    h_matching_proxy_connection,
    h_matching_content_length,
    h_matching_transfer_encoding,
    h_matching_upgrade,
    h_connection,
    h_content_length,
    h_transfer_encoding,
    h_upgrade,
    h_matching_transfer_encoding_chunked,
    h_matching_connection_keep_alive,
    h_matching_connection_close,
    h_transfer_encoding_chunked,
    h_connection_keep_alive,
    h_connection_close
};

enum http_host_state {
    sHTTP_host_dead = 1,
    sHTTP_userinfo_start,
    sHTTP_userinfo,
    sHTTP_host_start,
    sHTTP_host_v6_start,
    sHTTP_host,
    sHTTP_host_v6,
    sHTTP_host_v6_end,
    sHTTP_host_port_start,
    sHTTP_host_port
};

/* Macros for character classes; depends on strict-mode  */
#define _CR                  '\r'
#define _LF                  '\n'
#define _LOWER(c)            (unsigned char)(c | 0x20)
#define _IS_ALPHA(c)         (_LOWER(c) >= 'a' && _LOWER(c) <= 'z')
#define _IS_NUM(c)           ((c) >= '0' && (c) <= '9')
#define _IS_ALPHANUM(c)      (_IS_ALPHA(c) || _IS_NUM(c))
#define _IS_HEX(c)           (_IS_NUM(c) || (_LOWER(c) >= 'a' && _LOWER(c) <= 'f'))
#define _IS_MARK(c)          ((c) == '-' || (c) == '_' || (c) == '.' || \
  (c) == '!' || (c) == '~' || (c) == '*' || (c) == '\'' || (c) == '(' || \
  (c) == ')')
#define _IS_USERINFO_CHAR(c) (_IS_ALPHANUM(c) || _IS_MARK(c) || (c) == '%' || \
  (c) == ';' || (c) == ':' || (c) == '&' || (c) == '=' || (c) == '+' || \
  (c) == '$' || (c) == ',')

#if HTTP_PARSER_STRICT
#define _TOKEN(c)            (tokens[(unsigned char)c])
#define _IS_URL_CHAR(c)      (_BIT_AT(normal_url_char, (unsigned char)c))
#define _IS_HOST_CHAR(c)     (_IS_ALPHANUM(c) || (c) == '.' || (c) == '-')
#else
#define _TOKEN(c)            ((c == ' ') ? ' ' : tokens[(unsigned char)c])
#define _IS_URL_CHAR(c)                                                         \
  (_BIT_AT(normal_url_char, (unsigned char)c) || ((c) & 0x80))
#define _IS_HOST_CHAR(c)                                                        \
  (_IS_ALPHANUM(c) || (c) == '.' || (c) == '-' || (c) == '_')
#endif


#define _start_state (parser->type == HTTP_REQUEST ? s_start_req : s_start_res)


#if HTTP_PARSER_STRICT
# define _STRICT_CHECK(cond)                                          \
do {                                                                 \
  if (cond) {                                                        \
    _SET_ERRNO(HPE_STRICT);                                           \
    goto error;                                                      \
  }                                                                  \
} while (0)
# define _NEW_MESSAGE() (http_should_keep_alive(parser) ? _start_state : s_dead)
#else
# define _STRICT_CHECK(cond)
# define _NEW_MESSAGE() _start_state
#endif


int http_message_needs_eof(const http_parser *parser);

/* Our URL parser.
 *
 * This is designed to be shared by http_parser_execute() for URL validation,
 * hence it has a state transition + byte-for-byte interface. In addition, it
 * is meant to be embedded in http_parser_parse_url(), which does the dirty
 * work of turning state transitions URL components for its API.
 *
 * This function should only be invoked with non-space characters. It is
 * assumed that the caller cares about (and can detect) the transition between
 * URL and non-URL states by looking for these.
 */
inline enum state
parse_url_char(enum state s, const char ch) {
#if HTTP_PARSER_STRICT
# define _T(v) 0
#else
# define _T(v) v
#endif


    static const uint8_t normal_url_char[32] = {
/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
            0 | 0 | 0 | 0 | 0 | 0 | 0 | 0,
/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
            0 | _T(2) | 0 | 0 | _T(16) | 0 | 0 | 0,
/*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
            0 | 0 | 0 | 0 | 0 | 0 | 0 | 0,
/*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
            0 | 0 | 0 | 0 | 0 | 0 | 0 | 0,
/*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
            0 | 2 | 4 | 0 | 16 | 32 | 64 | 128,
/*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
            1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
/*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
            1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
/*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
            1 | 2 | 4 | 8 | 16 | 32 | 64 | 0,
/*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
            1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
/*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
            1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
/*  80  P    81  Q    82  R    83  S    84  _T    85  U    86  V    87  W  */
            1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
/*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
            1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
/*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
            1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
/* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
            1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
/* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
            1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
/* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
            1 | 2 | 4 | 8 | 16 | 32 | 64 | 0,};

#undef _T

    if (ch == ' ' || ch == '\r' || ch == '\n') {
        return s_dead;
    }

#if HTTP_PARSER_STRICT
    if (ch == '\t' || ch == '\f') {
        return s_dead;
    }
#endif

    switch (s) {
        case s_req_spaces_before_url:
            /* Proxied requests are followed by scheme of an absolute URI (alpha).
             * All methods except CONNECT are followed by '/' or '*'.
             */

            if (ch == '/' || ch == '*') {
                return s_req_path;
            }

            if (_IS_ALPHA(ch)) {
                return s_req_schema;
            }

            break;

        case s_req_schema:
            if (_IS_ALPHA(ch)) {
                return s;
            }

            if (ch == ':') {
                return s_req_schema_slash;
            }

            break;

        case s_req_schema_slash:
            if (ch == '/') {
                return s_req_schema_slash_slash;
            }

            break;

        case s_req_schema_slash_slash:
            if (ch == '/') {
                return s_req_server_start;
            }

            break;

        case s_req_server_with_at:
            if (ch == '@') {
                return s_dead;
            }

            /* FALLTHROUGH */
        case s_req_server_start:
        case s_req_server:
            if (ch == '/') {
                return s_req_path;
            }

            if (ch == '?') {
                return s_req_query_string_start;
            }

            if (ch == '@') {
                return s_req_server_with_at;
            }

            if (_IS_USERINFO_CHAR(ch) || ch == '[' || ch == ']') {
                return s_req_server;
            }

            break;

        case s_req_path:
            if (_IS_URL_CHAR(ch)) {
                return s;
            }

            switch (ch) {
                case '?':
                    return s_req_query_string_start;

                case '#':
                    return s_req_fragment_start;
                default:
                    break;
            }

            break;

        case s_req_query_string_start:
        case s_req_query_string:
            if (_IS_URL_CHAR(ch)) {
                return s_req_query_string;
            }

            switch (ch) {
                case '?':
                    /* allow extra '?' in query string */
                    return s_req_query_string;

                case '#':
                    return s_req_fragment_start;
            }

            break;

        case s_req_fragment_start:
            if (_IS_URL_CHAR(ch)) {
                return s_req_fragment;
            }

            switch (ch) {
                case '?':
                    return s_req_fragment;

                case '#':
                    return s;
                default:
                    break;
            }

            break;

        case s_req_fragment:
            if (_IS_URL_CHAR(ch)) {
                return s;
            }

            switch (ch) {
                case '?':
                case '#':
                    return s;
                default: break;
            }

            break;

        default:
            break;
    }

    /* We should never fall out of the switch above unless there's an error */
    return s_dead;
}

size_t http_parser_execute(http_parser *parser,
                           const http_parser_settings *settings,
                           const char *data,
                           size_t len) {
    static const char *method_strings[] =
            {
#define _XX(num, name, string) #string,
                    HTTP_METHOD_MAP(_XX)
#undef _XX
            };

/* Tokens as defined by rfc 2616. Also lowercases them.
 *        token       = 1*<any CHAR except CTLs or separators>
 *     separators     = "(" | ")" | "<" | ">" | "@"
 *                    | "," | ";" | ":" | "\" | <">
 *                    | "/" | "[" | "]" | "?" | "="
 *                    | "{" | "}" | SP | HT
 */
    static const char tokens[256] = {
/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
            0, 0, 0, 0, 0, 0, 0, 0,
/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
            0, 0, 0, 0, 0, 0, 0, 0,
/*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
            0, 0, 0, 0, 0, 0, 0, 0,
/*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
            0, 0, 0, 0, 0, 0, 0, 0,
/*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
            0, '!', 0, '#', '$', '%', '&', '\'',
/*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
            0, 0, '*', '+', 0, '-', '.', 0,
/*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
            '0', '1', '2', '3', '4', '5', '6', '7',
/*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
            '8', '9', 0, 0, 0, 0, 0, 0,
/*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
            0, 'a', 'b', 'c', 'd', 'e', 'f', 'g',
/*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
            'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
/*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
            'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
/*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
            'x', 'y', 'z', 0, 0, 0, '^', '_',
/*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
            '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
/* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
            'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
/* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
            'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
/* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
            'x', 'y', 'z', 0, '|', 0, '~', 0};


    static const int8_t unhex[256] =
            {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
             -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7,
             8, 9, -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
             -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1,
             -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
            };


    char c, ch;
    int8_t unhex_val;
    const char *p = data;
    const char *header_field_mark = 0;
    const char *header_value_mark = 0;
    const char *url_mark = 0;
    const char *body_mark = 0;
    const char *status_mark = 0;

    /* We're in an error state. Don't bother doing anything. */
    if (HTTP_PARSER_ERRNO(parser) != HPE_OK) {
        return 0;
    }

    if (len == 0) {
        switch (parser->state) {
            case s_body_identity_eof:
                /* Use of _CALLBACK_NOTIFY() here would erroneously return 1 byte read if
                 * we got paused.
                 */
                _CALLBACK_NOTIFY_NOADVANCE(message_complete);
                return 0;

            case s_dead:
            case s_start_req_or_res:
            case s_start_res:
            case s_start_req:
                return 0;

            default:
                _SET_ERRNO(HPE_INVALID_EOF_STATE);
                return 1;
        }
    }


    if (parser->state == s_header_field)
        header_field_mark = data;
    if (parser->state == s_header_value)
        header_value_mark = data;
    switch (parser->state) {
        case s_req_path:
        case s_req_schema:
        case s_req_schema_slash:
        case s_req_schema_slash_slash:
        case s_req_server_start:
        case s_req_server:
        case s_req_server_with_at:
        case s_req_query_string_start:
        case s_req_query_string:
        case s_req_fragment_start:
        case s_req_fragment:
            url_mark = data;
            break;
        case s_res_status:
            status_mark = data;
            break;
        default: break;
    }

    for (p = data; p != data + len; p++) {
        ch = *p;

        if (PARSING_HEADER(parser->state)) {
            ++parser->nread;
            /* Don't allow the total size of the HTTP headers (including the status
             * line) to exceed HTTP_MAX_HEADER_SIZE.  This check is here to protect
             * embedders against denial-of-application attacks where the attacker feeds
             * us a never-ending header that the embedder keeps buffering.
             *
             * This check is arguably the responsibility of embedders but we're doing
             * it on the embedder's behalf because most won't bother and this way we
             * make the web a little safer.  HTTP_MAX_HEADER_SIZE is still far bigger
             * than any reasonable Request or Response so this should never affect
             * day-to-day operation.
             */
            if (parser->nread > (HTTP_MAX_HEADER_SIZE)) {
                _SET_ERRNO(HPE_HEADER_OVERFLOW);
                goto error;
            }
        }

        reexecute_byte:
        switch (parser->state) {

            case s_dead:
                /* this state is used after a 'Connection: close' message
                 * the parser will error out if it reads another message
                 */
                if (ch == _CR || ch == _LF)
                    break;

                _SET_ERRNO(HPE_CLOSED_CONNECTION);
                goto error;

            case s_start_req_or_res: {
                if (ch == _CR || ch == _LF)
                    break;
                parser->flags = 0;
                parser->content_length = _ULLONG_MAX;

                if (ch == 'H') {
                    parser->state = s_res_or_resp_H;

                    _CALLBACK_NOTIFY(message_begin);
                } else {
                    parser->type = HTTP_REQUEST;
                    parser->state = s_start_req;
                    goto reexecute_byte;
                }

                break;
            }

            case s_res_or_resp_H:
                if (ch == 'T') {
                    parser->type = HTTP_RESPONSE;
                    parser->state = s_res_HT;
                } else {
                    if (ch != 'E') {
                        _SET_ERRNO(HPE_INVALID_CONSTANT);
                        goto error;
                    }

                    parser->type = HTTP_REQUEST;
                    parser->method = HTTP_HEAD;
                    parser->index = 2;
                    parser->state = s_req_method;
                }
                break;

            case s_start_res: {
                parser->flags = 0;
                parser->content_length = _ULLONG_MAX;

                switch (ch) {
                    case 'H':
                        parser->state = s_res_H;
                        break;

                    case _CR:
                    case _LF:
                        break;

                    default:
                        _SET_ERRNO(HPE_INVALID_CONSTANT);
                        goto error;
                }

                _CALLBACK_NOTIFY(message_begin);
                break;
            }

            case s_res_H:
                _STRICT_CHECK(ch != 'T');
                parser->state = s_res_HT;
                break;

            case s_res_HT:
                _STRICT_CHECK(ch != 'T');
                parser->state = s_res_HTT;
                break;

            case s_res_HTT:
                _STRICT_CHECK(ch != 'P');
                parser->state = s_resHTTP;
                break;

            case s_resHTTP:
                _STRICT_CHECK(ch != '/');
                parser->state = s_res_firstHTTP_major;
                break;

            case s_res_firstHTTP_major:
                if (ch < '0' || ch > '9') {
                    _SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }

                parser->http_major = ch - '0';
                parser->state = s_resHTTP_major;
                break;

                /* major HTTP version or dot */
            case s_resHTTP_major: {
                if (ch == '.') {
                    parser->state = s_res_firstHTTP_minor;
                    break;
                }

                if (!_IS_NUM(ch)) {
                    _SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }

                parser->http_major *= 10;
                parser->http_major += ch - '0';

                if (parser->http_major > 999) {
                    _SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }

                break;
            }

                /* first digit of minor HTTP version */
            case s_res_firstHTTP_minor:
                if (!_IS_NUM(ch)) {
                    _SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }

                parser->http_minor = ch - '0';
                parser->state = s_resHTTP_minor;
                break;

                /* minor HTTP version or end of Request line */
            case s_resHTTP_minor: {
                if (ch == ' ') {
                    parser->state = s_res_first_status_code;
                    break;
                }

                if (!_IS_NUM(ch)) {
                    _SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }

                parser->http_minor *= 10;
                parser->http_minor += ch - '0';

                if (parser->http_minor > 999) {
                    _SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }

                break;
            }

            case s_res_first_status_code: {
                if (!_IS_NUM(ch)) {
                    if (ch == ' ') {
                        break;
                    }

                    _SET_ERRNO(HPE_INVALID_STATUS);
                    goto error;
                }
                parser->status_code = ch - '0';
                parser->state = s_res_status_code;
                break;
            }

            case s_res_status_code: {
                if (!_IS_NUM(ch)) {
                    switch (ch) {
                        case ' ':
                            parser->state = s_res_status_start;
                            break;
                        case _CR:
                            parser->state = s_res_line_almost_done;
                            break;
                        case _LF:
                            parser->state = s_header_field_start;
                            break;
                        default:
                            _SET_ERRNO(HPE_INVALID_STATUS);
                            goto error;
                    }
                    break;
                }

                parser->status_code *= 10;
                parser->status_code += ch - '0';

                if (parser->status_code > 999) {
                    _SET_ERRNO(HPE_INVALID_STATUS);
                    goto error;
                }

                break;
            }

            case s_res_status_start: {
                if (ch == _CR) {
                    parser->state = s_res_line_almost_done;
                    break;
                }

                if (ch == _LF) {
                    parser->state = s_header_field_start;
                    break;
                }

                _MARK(status);
                parser->state = s_res_status;
                parser->index = 0;
                break;
            }

            case s_res_status:
                if (ch == _CR) {
                    parser->state = s_res_line_almost_done;
                    _CALLBACK_DATA(status);
                    break;
                }

                if (ch == _LF) {
                    parser->state = s_header_field_start;
                    _CALLBACK_DATA(status);
                    break;
                }

                break;

            case s_res_line_almost_done:
                _STRICT_CHECK(ch != _LF);
                parser->state = s_header_field_start;
                break;

            case s_start_req: {
                if (ch == _CR || ch == _LF)
                    break;
                parser->flags = 0;
                parser->content_length = _ULLONG_MAX;

                if (!_IS_ALPHA(ch)) {
                    _SET_ERRNO(HPE_INVALID_METHOD);
                    goto error;
                }

                parser->method = (enum http_method) 0;
                parser->index = 1;
                switch (ch) {
                    case 'C':
                        parser->method = HTTP_CONNECT; /* or COPY, CHECKOUT */ break;
                    case 'D':
                        parser->method = HTTP_DELETE;
                        break;
                    case 'G':
                        parser->method = HTTP_GET;
                        break;
                    case 'H':
                        parser->method = HTTP_HEAD;
                        break;
                    case 'L':
                        parser->method = HTTP_LOCK;
                        break;
                    case 'M':
                        parser->method = HTTP_MKCOL; /* or MOVE, MKACTIVITY, MERGE, M-SEARCH, MKCALENDAR */ break;
                    case 'N':
                        parser->method = HTTP_NOTIFY;
                        break;
                    case 'O':
                        parser->method = HTTP_OPTIONS;
                        break;
                    case 'P':
                        parser->method = HTTP_POST;
                        /* or PROPFIND|PROPPATCH|PUT|PATCH|PURGE */
                        break;
                    case 'R':
                        parser->method = HTTP_REPORT;
                        break;
                    case 'S':
                        parser->method = HTTP_SUBSCRIBE; /* or SEARCH */ break;
                    case 'T':
                        parser->method = HTTP_TRACE;
                        break;
                    case 'U':
                        parser->method = HTTP_UNLOCK; /* or UNSUBSCRIBE */ break;
                    default:
                        _SET_ERRNO(HPE_INVALID_METHOD);
                        goto error;
                }
                parser->state = s_req_method;

                _CALLBACK_NOTIFY(message_begin);

                break;
            }

            case s_req_method: {
                const char *matcher;
                if (ch == '\0') {
                    _SET_ERRNO(HPE_INVALID_METHOD);
                    goto error;
                }

                matcher = method_strings[parser->method];
                if (ch == ' ' && matcher[parser->index] == '\0') {
                    parser->state = s_req_spaces_before_url;
                } else if (ch == matcher[parser->index]) { ; /* nada */
                } else if (parser->method == HTTP_CONNECT) {
                    if (parser->index == 1 && ch == 'H') {
                        parser->method = HTTP_CHECKOUT;
                    } else if (parser->index == 2 && ch == 'P') {
                        parser->method = HTTP_COPY;
                    } else {
                        _SET_ERRNO(HPE_INVALID_METHOD);
                        goto error;
                    }
                } else if (parser->method == HTTP_MKCOL) {
                    if (parser->index == 1 && ch == 'O') {
                        parser->method = HTTP_MOVE;
                    } else if (parser->index == 1 && ch == 'E') {
                        parser->method = HTTP_MERGE;
                    } else if (parser->index == 1 && ch == '-') {
                        parser->method = HTTP_MSEARCH;
                    } else if (parser->index == 2 && ch == 'A') {
                        parser->method = HTTP_MKACTIVITY;
                    } else if (parser->index == 3 && ch == 'A') {
                        parser->method = HTTP_MKCALENDAR;
                    } else {
                        _SET_ERRNO(HPE_INVALID_METHOD);
                        goto error;
                    }
                } else if (parser->method == HTTP_SUBSCRIBE) {
                    if (parser->index == 1 && ch == 'E') {
                        parser->method = HTTP_SEARCH;
                    } else {
                        _SET_ERRNO(HPE_INVALID_METHOD);
                        goto error;
                    }
                } else if (parser->index == 1 && parser->method == HTTP_POST) {
                    if (ch == 'R') {
                        parser->method = HTTP_PROPFIND; /* or HTTP_PROPPATCH */
                    } else if (ch == 'U') {
                        parser->method = HTTP_PUT; /* or HTTP_PURGE */
                    } else if (ch == 'A') {
                        parser->method = HTTP_PATCH;
                    } else {
                        _SET_ERRNO(HPE_INVALID_METHOD);
                        goto error;
                    }
                } else if (parser->index == 2) {
                    if (parser->method == HTTP_PUT) {
                        if (ch == 'R') {
                            parser->method = HTTP_PURGE;
                        } else {
                            _SET_ERRNO(HPE_INVALID_METHOD);
                            goto error;
                        }
                    } else if (parser->method == HTTP_UNLOCK) {
                        if (ch == 'S') {
                            parser->method = HTTP_UNSUBSCRIBE;
                        } else {
                            _SET_ERRNO(HPE_INVALID_METHOD);
                            goto error;
                        }
                    } else {
                        _SET_ERRNO(HPE_INVALID_METHOD);
                        goto error;
                    }
                } else if (parser->index == 4 && parser->method == HTTP_PROPFIND && ch == 'P') {
                    parser->method = HTTP_PROPPATCH;
                } else {
                    _SET_ERRNO(HPE_INVALID_METHOD);
                    goto error;
                }

                ++parser->index;
                break;
            }

            case s_req_spaces_before_url: {
                if (ch == ' ') break;

                _MARK(url);
                if (parser->method == HTTP_CONNECT) {
                    parser->state = s_req_server_start;
                }

                parser->state = parse_url_char((enum state) parser->state, ch);
                if (parser->state == s_dead) {
                    _SET_ERRNO(HPE_INVALID_URL);
                    goto error;
                }

                break;
            }

            case s_req_schema:
            case s_req_schema_slash:
            case s_req_schema_slash_slash:
            case s_req_server_start: {
                switch (ch) {
                    /* No whitespace allowed here */
                    case ' ':
                    case _CR:
                    case _LF:
                        _SET_ERRNO(HPE_INVALID_URL);
                        goto error;
                    default:
                        parser->state = parse_url_char((enum state) parser->state, ch);
                        if (parser->state == s_dead) {
                            _SET_ERRNO(HPE_INVALID_URL);
                            goto error;
                        }
                }

                break;
            }

            case s_req_server:
            case s_req_server_with_at:
            case s_req_path:
            case s_req_query_string_start:
            case s_req_query_string:
            case s_req_fragment_start:
            case s_req_fragment: {
                switch (ch) {
                    case ' ':
                        parser->state = s_reqHTTP_start;
                        _CALLBACK_DATA(url);
                        break;
                    case _CR:
                    case _LF:
                        parser->http_major = 0;
                        parser->http_minor = 9;
                        parser->state = (ch == _CR) ?
                                        s_req_line_almost_done :
                                        s_header_field_start;
                        _CALLBACK_DATA(url);
                        break;
                    default:
                        parser->state = parse_url_char((enum state) parser->state, ch);
                        if (parser->state == s_dead) {
                            _SET_ERRNO(HPE_INVALID_URL);
                            goto error;
                        }
                }
                break;
            }

            case s_reqHTTP_start:
                switch (ch) {
                    case 'H':
                        parser->state = s_reqHTTP_H;
                        break;
                    case ' ':
                        break;
                    default:
                        _SET_ERRNO(HPE_INVALID_CONSTANT);
                        goto error;
                }
                break;

            case s_reqHTTP_H:
                _STRICT_CHECK(ch != 'T');
                parser->state = s_reqHTTP_HT;
                break;

            case s_reqHTTP_HT:
                _STRICT_CHECK(ch != 'T');
                parser->state = s_reqHTTP_HTT;
                break;

            case s_reqHTTP_HTT:
                _STRICT_CHECK(ch != 'P');
                parser->state = s_reqHTTPHTTP;
                break;

            case s_reqHTTPHTTP:
                _STRICT_CHECK(ch != '/');
                parser->state = s_req_firstHTTP_major;
                break;

                /* first digit of major HTTP version */
            case s_req_firstHTTP_major:
                if (ch < '1' || ch > '9') {
                    _SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }

                parser->http_major = ch - '0';
                parser->state = s_reqHTTP_major;
                break;

                /* major HTTP version or dot */
            case s_reqHTTP_major: {
                if (ch == '.') {
                    parser->state = s_req_firstHTTP_minor;
                    break;
                }

                if (!_IS_NUM(ch)) {
                    _SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }

                parser->http_major *= 10;
                parser->http_major += ch - '0';

                if (parser->http_major > 999) {
                    _SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }

                break;
            }

                /* first digit of minor HTTP version */
            case s_req_firstHTTP_minor:
                if (!_IS_NUM(ch)) {
                    _SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }

                parser->http_minor = ch - '0';
                parser->state = s_reqHTTP_minor;
                break;

                /* minor HTTP version or end of Request line */
            case s_reqHTTP_minor: {
                if (ch == _CR) {
                    parser->state = s_req_line_almost_done;
                    break;
                }

                if (ch == _LF) {
                    parser->state = s_header_field_start;
                    break;
                }

                /* XXX allow spaces after digit? */

                if (!_IS_NUM(ch)) {
                    _SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }

                parser->http_minor *= 10;
                parser->http_minor += ch - '0';

                if (parser->http_minor > 999) {
                    _SET_ERRNO(HPE_INVALID_VERSION);
                    goto error;
                }

                break;
            }

                /* end of Request line */
            case s_req_line_almost_done: {
                if (ch != _LF) {
                    _SET_ERRNO(HPE_LF_EXPECTED);
                    goto error;
                }

                parser->state = s_header_field_start;
                break;
            }

            case s_header_field_start: {
                if (ch == _CR) {
                    parser->state = s_headers_almost_done;
                    break;
                }

                if (ch == _LF) {
                    /* they might be just sending \n instead of \r\n so this would be
                     * the second \n to denote the end of headers*/
                    parser->state = s_headers_almost_done;
                    goto reexecute_byte;
                }

                c = _TOKEN(ch);

                if (!c) {
                    _SET_ERRNO(HPE_INVALID_HEADER_TOKEN);
                    goto error;
                }

                _MARK(header_field);

                parser->index = 0;
                parser->state = s_header_field;

                switch (c) {
                    case 'c':
                        parser->header_state = h_C;
                        break;

                    case 'p':
                        parser->header_state = h_matching_proxy_connection;
                        break;

                    case 't':
                        parser->header_state = h_matching_transfer_encoding;
                        break;

                    case 'u':
                        parser->header_state = h_matching_upgrade;
                        break;

                    default:
                        parser->header_state = h_general;
                        break;
                }
                break;
            }

            case s_header_field: {
                c = _TOKEN(ch);

                if (c) {
                    switch (parser->header_state) {
                        case h_general:
                            break;

                        case h_C:
                            parser->index++;
                            parser->header_state = (c == 'o' ? h_CO : h_general);
                            break;

                        case h_CO:
                            parser->index++;
                            parser->header_state = (c == 'n' ? h_CON : h_general);
                            break;

                        case h_CON:
                            parser->index++;
                            switch (c) {
                                case 'n':
                                    parser->header_state = h_matching_connection;
                                    break;
                                case 't':
                                    parser->header_state = h_matching_content_length;
                                    break;
                                default:
                                    parser->header_state = h_general;
                                    break;
                            }
                            break;

                            /* Connection */

                        case h_matching_connection:
                            parser->index++;
                            if (parser->index > sizeof(_CONNECTION) - 1
                                || c != _CONNECTION[parser->index]) {
                                parser->header_state = h_general;
                            } else if (parser->index == sizeof(_CONNECTION) - 2) {
                                parser->header_state = h_connection;
                            }
                            break;

                            /* proxy-Connection */

                        case h_matching_proxy_connection:
                            parser->index++;
                            if (parser->index > sizeof(_PROXY_CONNECTION) - 1
                                || c != _PROXY_CONNECTION[parser->index]) {
                                parser->header_state = h_general;
                            } else if (parser->index == sizeof(_PROXY_CONNECTION) - 2) {
                                parser->header_state = h_connection;
                            }
                            break;

                            /* content-length */

                        case h_matching_content_length:
                            parser->index++;
                            if (parser->index > sizeof(_CONTENT_LENGTH) - 1
                                || c != _CONTENT_LENGTH[parser->index]) {
                                parser->header_state = h_general;
                            } else if (parser->index == sizeof(_CONTENT_LENGTH) - 2) {
                                parser->header_state = h_content_length;
                            }
                            break;

                            /* transfer-encoding */

                        case h_matching_transfer_encoding:
                            parser->index++;
                            if (parser->index > sizeof(_TRANSFER_ENCODING) - 1
                                || c != _TRANSFER_ENCODING[parser->index]) {
                                parser->header_state = h_general;
                            } else if (parser->index == sizeof(_TRANSFER_ENCODING) - 2) {
                                parser->header_state = h_transfer_encoding;
                            }
                            break;

                            /* upgrade */

                        case h_matching_upgrade:
                            parser->index++;
                            if (parser->index > sizeof(_UPGRADE) - 1
                                || c != _UPGRADE[parser->index]) {
                                parser->header_state = h_general;
                            } else if (parser->index == sizeof(_UPGRADE) - 2) {
                                parser->header_state = h_upgrade;
                            }
                            break;

                        case h_connection:
                        case h_content_length:
                        case h_transfer_encoding:
                        case h_upgrade:
                            if (ch != ' ') parser->header_state = h_general;
                            break;

                        default:
                            assert(0 && "Unknown header_state");
                            break;
                    }
                    break;
                }

                if (ch == ':') {
                    parser->state = s_header_value_discard_ws;
                    _CALLBACK_DATA(header_field);
                    break;
                }

                if (ch == _CR) {
                    parser->state = s_header_almost_done;
                    _CALLBACK_DATA(header_field);
                    break;
                }

                if (ch == _LF) {
                    parser->state = s_header_field_start;
                    _CALLBACK_DATA(header_field);
                    break;
                }

                _SET_ERRNO(HPE_INVALID_HEADER_TOKEN);
                goto error;
            }

            case s_header_value_discard_ws:
                if (ch == ' ' || ch == '\t') break;

                if (ch == _CR) {
                    parser->state = s_header_value_discard_ws_almost_done;
                    break;
                }

                if (ch == _LF) {
                    parser->state = s_header_value_discard_lws;
                    break;
                }

                /* FALLTHROUGH */

            case s_header_value_start: {
                _MARK(header_value);

                parser->state = s_header_value;
                parser->index = 0;

                c = _LOWER(ch);

                switch (parser->header_state) {
                    case h_upgrade:
                        parser->flags |= F_UPGRADE;
                        parser->header_state = h_general;
                        break;

                    case h_transfer_encoding:
                        /* looking for 'Transfer-Encoding: chunked' */
                        if ('c' == c) {
                            parser->header_state = h_matching_transfer_encoding_chunked;
                        } else {
                            parser->header_state = h_general;
                        }
                        break;

                    case h_content_length:
                        if (!_IS_NUM(ch)) {
                            _SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
                            goto error;
                        }

                        parser->content_length = ch - '0';
                        break;

                    case h_connection:
                        /* looking for 'Connection: keep-alive' */
                        if (c == 'k') {
                            parser->header_state = h_matching_connection_keep_alive;
                            /* looking for 'Connection: close' */
                        } else if (c == 'c') {
                            parser->header_state = h_matching_connection_close;
                        } else {
                            parser->header_state = h_general;
                        }
                        break;

                    default:
                        parser->header_state = h_general;
                        break;
                }
                break;
            }

            case s_header_value: {

                if (ch == _CR) {
                    parser->state = s_header_almost_done;
                    _CALLBACK_DATA(header_value);
                    break;
                }

                if (ch == _LF) {
                    parser->state = s_header_almost_done;
                    _CALLBACK_DATA_NOADVANCE(header_value);
                    goto reexecute_byte;
                }

                c = _LOWER(ch);

                switch (parser->header_state) {
                    case h_general:
                        break;

                    case h_connection:
                    case h_transfer_encoding:
                        assert(0 && "Shouldn't get here.");
                        break;

                    case h_content_length: {
                        uint64_t t;

                        if (ch == ' ') break;

                        if (!_IS_NUM(ch)) {
                            _SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
                            goto error;
                        }

                        t = parser->content_length;
                        t *= 10;
                        t += ch - '0';

                        /* Overflow? Test against a conservative limit for simplicity. */
                        if ((_ULLONG_MAX - 10) / 10 < parser->content_length) {
                            _SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
                            goto error;
                        }

                        parser->content_length = t;
                        break;
                    }

                        /* Transfer-Encoding: chunked */
                    case h_matching_transfer_encoding_chunked:
                        parser->index++;
                        if (parser->index > sizeof(_CHUNKED) - 1
                            || c != _CHUNKED[parser->index]) {
                            parser->header_state = h_general;
                        } else if (parser->index == sizeof(_CHUNKED) - 2) {
                            parser->header_state = h_transfer_encoding_chunked;
                        }
                        break;

                        /* looking for 'Connection: keep-alive' */
                    case h_matching_connection_keep_alive:
                        parser->index++;
                        if (parser->index > sizeof(_KEEP_ALIVE) - 1
                            || c != _KEEP_ALIVE[parser->index]) {
                            parser->header_state = h_general;
                        } else if (parser->index == sizeof(_KEEP_ALIVE) - 2) {
                            parser->header_state = h_connection_keep_alive;
                        }
                        break;

                        /* looking for 'Connection: close' */
                    case h_matching_connection_close:
                        parser->index++;
                        if (parser->index > sizeof(_CLOSE) - 1 || c != _CLOSE[parser->index]) {
                            parser->header_state = h_general;
                        } else if (parser->index == sizeof(_CLOSE) - 2) {
                            parser->header_state = h_connection_close;
                        }
                        break;

                    case h_transfer_encoding_chunked:
                    case h_connection_keep_alive:
                    case h_connection_close:
                        if (ch != ' ') parser->header_state = h_general;
                        break;

                    default:
                        parser->state = s_header_value;
                        parser->header_state = h_general;
                        break;
                }
                break;
            }

            case s_header_almost_done: {
                _STRICT_CHECK(ch != _LF);

                parser->state = s_header_value_lws;
                break;
            }

            case s_header_value_lws: {
                if (ch == ' ' || ch == '\t') {
                    parser->state = s_header_value_start;
                    goto reexecute_byte;
                }

                /* finished the header */
                switch (parser->header_state) {
                    case h_connection_keep_alive:
                        parser->flags |= F_CONNECTION_KEEP_ALIVE;
                        break;
                    case h_connection_close:
                        parser->flags |= F_CONNECTION_CLOSE;
                        break;
                    case h_transfer_encoding_chunked:
                        parser->flags |= F_CHUNKED;
                        break;
                    default:
                        break;
                }

                parser->state = s_header_field_start;
                goto reexecute_byte;
            }

            case s_header_value_discard_ws_almost_done: {
                _STRICT_CHECK(ch != _LF);
                parser->state = s_header_value_discard_lws;
                break;
            }

            case s_header_value_discard_lws: {
                if (ch == ' ' || ch == '\t') {
                    parser->state = s_header_value_discard_ws;
                    break;
                } else {
                    /* header value was empty */
                    _MARK(header_value);
                    parser->state = s_header_field_start;
                    _CALLBACK_DATA_NOADVANCE(header_value);
                    goto reexecute_byte;
                }
            }

            case s_headers_almost_done: {
                _STRICT_CHECK(ch != _LF);

                if (parser->flags & F_TRAILING) {
                    /* End of a chunked Request */
                    parser->state = _NEW_MESSAGE();
                    _CALLBACK_NOTIFY(message_complete);
                    break;
                }

                parser->state = s_headers_done;

                /* Set this here so that on_headers_complete() callbacks can see it */
                parser->upgrade =
                        (parser->flags & F_UPGRADE || parser->method == HTTP_CONNECT);

                /* Here we call the headers_complete callback. This is somewhat
                 * different than other callbacks because if the user returns 1, we
                 * will interpret that as saying that this message has no body. This
                 * is needed for the annoying case of recieving a Response to a HEAD
                 * Request.
                 *
                 * We'd like to use _CALLBACK_NOTIFY_NOADVANCE() here but we cannot, so
                 * we have to simulate it by handling a change in errno below.
                 */
                if (settings->on_headers_complete) {
                    switch (settings->on_headers_complete(parser)) {
                        case 0:
                            break;

                        case 1:
                            parser->flags |= F_SKIPBODY;
                            break;

                        default:
                            _SET_ERRNO(HPE_CB_headers_complete);
                            return p - data; /* Error */
                    }
                }

                if (HTTP_PARSER_ERRNO(parser) != HPE_OK) {
                    return p - data;
                }

                goto reexecute_byte;
            }

            case s_headers_done: {
                _STRICT_CHECK(ch != _LF);

                parser->nread = 0;

                /* Exit, the rest of the connect is in a different protocol. */
                if (parser->upgrade) {
                    parser->state = _NEW_MESSAGE();
                    _CALLBACK_NOTIFY(message_complete);
                    return (p - data) + 1;
                }

                if (parser->flags & F_SKIPBODY) {
                    parser->state = _NEW_MESSAGE();
                    _CALLBACK_NOTIFY(message_complete);
                } else if (parser->flags & F_CHUNKED) {
                    /* chunked encoding - ignore Content-Length header */
                    parser->state = s_chunk_size_start;
                } else {
                    if (parser->content_length == 0) {
                        /* Content-Length header given but zero: Content-Length: 0\r\n */
                        parser->state = _NEW_MESSAGE();
                        _CALLBACK_NOTIFY(message_complete);
                    } else if (parser->content_length != _ULLONG_MAX) {
                        /* Content-Length header given and non-zero */
                        parser->state = s_body_identity;
                    } else {
                        if (parser->type == HTTP_REQUEST ||
                            !http_message_needs_eof(parser)) {
                            /* Assume content-length 0 - read the next */
                            parser->state = _NEW_MESSAGE();
                            _CALLBACK_NOTIFY(message_complete);
                        } else {
                            /* Read body until EOF */
                            parser->state = s_body_identity_eof;
                        }
                    }
                }

                break;
            }

            case s_body_identity: {
                uint64_t to_read = _MIN(parser->content_length,
                                        (uint64_t)((data + len) - p));

                assert(parser->content_length != 0
                       && parser->content_length != _ULLONG_MAX);

                /* The difference between advancing content_length and p is because
                 * the latter will automaticaly advance on the next loop iteration.
                 * Further, if content_length ends up at 0, we want to see the last
                 * byte again for our message complete callback.
                 */
                _MARK(body);
                parser->content_length -= to_read;
                p += to_read - 1;

                if (parser->content_length == 0) {
                    parser->state = s_message_done;

                    /* Mimic _CALLBACK_DATA_NOADVANCE() but with one extra byte.
                     *
                     * The alternative to doing this is to wait for the next byte to
                     * trigger the data callback, just as in every other case. The
                     * problem with this is that this makes it difficult for the test
                     * harness to distinguish between complete-on-EOF and
                     * complete-on-length. It's not clear that this distinction is
                     * important for applications, but let's keep it for now.
                     */
                    _CALLBACK_DATA_(body, p - body_mark + 1, p - data);
                    goto reexecute_byte;
                }

                break;
            }

                /* read until EOF */
            case s_body_identity_eof:
                _MARK(body);
                p = data + len - 1;

                break;

            case s_message_done:
                parser->state = _NEW_MESSAGE();
                _CALLBACK_NOTIFY(message_complete);
                break;

            case s_chunk_size_start: {
                assert(parser->nread == 1);
                assert(parser->flags & F_CHUNKED);

                unhex_val = unhex[(unsigned char) ch];
                if (unhex_val == -1) {
                    _SET_ERRNO(HPE_INVALID_CHUNK_SIZE);
                    goto error;
                }

                parser->content_length = unhex_val;
                parser->state = s_chunk_size;
                break;
            }

            case s_chunk_size: {
                uint64_t t;

                assert(parser->flags & F_CHUNKED);

                if (ch == _CR) {
                    parser->state = s_chunk_size_almost_done;
                    break;
                }

                unhex_val = unhex[(unsigned char) ch];

                if (unhex_val == -1) {
                    if (ch == ';' || ch == ' ') {
                        parser->state = s_chunk_parameters;
                        break;
                    }

                    _SET_ERRNO(HPE_INVALID_CHUNK_SIZE);
                    goto error;
                }

                t = parser->content_length;
                t *= 16;
                t += unhex_val;

                /* Overflow? Test against a conservative limit for simplicity. */
                if ((_ULLONG_MAX - 16) / 16 < parser->content_length) {
                    _SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
                    goto error;
                }

                parser->content_length = t;
                break;
            }

            case s_chunk_parameters: {
                assert(parser->flags & F_CHUNKED);
                /* just ignore this shit. TODO check for overflow */
                if (ch == _CR) {
                    parser->state = s_chunk_size_almost_done;
                    break;
                }
                break;
            }

            case s_chunk_size_almost_done: {
                assert(parser->flags & F_CHUNKED);
                _STRICT_CHECK(ch != _LF);

                parser->nread = 0;

                if (parser->content_length == 0) {
                    parser->flags |= F_TRAILING;
                    parser->state = s_header_field_start;
                } else {
                    parser->state = s_chunk_data;
                }
                break;
            }

            case s_chunk_data: {
                uint64_t to_read = _MIN(parser->content_length,
                                        (uint64_t)((data + len) - p));

                assert(parser->flags & F_CHUNKED);
                assert(parser->content_length != 0
                       && parser->content_length != _ULLONG_MAX);

                /* See the explanation in s_body_identity for why the content
                 * length and data pointers are managed this way.
                 */
                _MARK(body);
                parser->content_length -= to_read;
                p += to_read - 1;

                if (parser->content_length == 0) {
                    parser->state = s_chunk_data_almost_done;
                }

                break;
            }

            case s_chunk_data_almost_done:
                assert(parser->flags & F_CHUNKED);
                assert(parser->content_length == 0);
                _STRICT_CHECK(ch != _CR);
                parser->state = s_chunk_data_done;
                _CALLBACK_DATA(body);
                break;

            case s_chunk_data_done:
                assert(parser->flags & F_CHUNKED);
                _STRICT_CHECK(ch != _LF);
                parser->nread = 0;
                parser->state = s_chunk_size_start;
                break;

            default:
                assert(0 && "unhandled state");
                _SET_ERRNO(HPE_INVALID_INTERNAL_STATE);
                goto error;
        }
    }

    /* Run callbacks for any marks that we have leftover after we ran our of
     * bytes. There should be at most one of these set, so it's OK to invoke
     * them in series (unset marks will not result in callbacks).
     *
     * We use the NOADVANCE() variety of callbacks here because 'p' has already
     * overflowed 'data' and this allows us to correct for the off-by-one that
     * we'd otherwise have (since _CALLBACK_DATA() is meant to be start with a 'p'
     * value that's in-bounds).
     */

    assert(((header_field_mark ? 1 : 0) +
            (header_value_mark ? 1 : 0) +
            (url_mark ? 1 : 0) +
            (body_mark ? 1 : 0) +
            (status_mark ? 1 : 0)) <= 1);

    _CALLBACK_DATA_NOADVANCE(header_field);
    _CALLBACK_DATA_NOADVANCE(header_value);
    _CALLBACK_DATA_NOADVANCE(url);
    _CALLBACK_DATA_NOADVANCE(body);
    _CALLBACK_DATA_NOADVANCE(status);

    return len;

    error:
    if (HTTP_PARSER_ERRNO(parser) == HPE_OK) {
        _SET_ERRNO(HPE_UNKNOWN);
    }

    return (p - data);
}


/* Does the parser need to see an EOF to find the end of the message? */
inline int
http_message_needs_eof(const http_parser *parser) {
    if (parser->type == HTTP_REQUEST) {
        return 0;
    }

    /* See RFC 2616 section 4.4 */
    if (parser->status_code / 100 == 1 || /* 1xx e.g. Continue */
        parser->status_code == 204 ||     /* No Content */
        parser->status_code == 304 ||     /* Not Modified */
        parser->flags & F_SKIPBODY) {     /* Response to a HEAD Request */
        return 0;
    }

    if ((parser->flags & F_CHUNKED) || parser->content_length != _ULLONG_MAX) {
        return 0;
    }

    return 1;
}


inline int
http_should_keep_alive(const http_parser *parser) {
    if (parser->http_major > 0 && parser->http_minor > 0) {
        /* HTTP/1.1 */
        if (parser->flags & F_CONNECTION_CLOSE) {
            return 0;
        }
    } else {
        /* HTTP/1.0 or earlier */
        if (!(parser->flags & F_CONNECTION_KEEP_ALIVE)) {
            return 0;
        }
    }

    return !http_message_needs_eof(parser);
}


const char *
http_method_str(enum http_method m) {
    static const char *method_strings[] =
            {
#define _XX(num, name, string) #string,
                    HTTP_METHOD_MAP(_XX)
#undef _XX
            };
    return _ELEM_AT(method_strings, m, "<unknown>");
}


void
http_parser_init(http_parser *parser, enum http_parser_type t) {
    void *data = parser->data; /* preserve application data */
    memset(parser, 0, sizeof(*parser));
    parser->data = data;
    parser->type = t;
    parser->method = HTTP_GET;
    parser->state = (t == HTTP_REQUEST ? s_start_req : (t == HTTP_RESPONSE ? s_start_res : s_start_req_or_res));
    parser->http_errno = HPE_OK;
}

const char *
http_errno_name(enum http_errno err) {
/* Map errno values to strings for human-readable output */
#define HTTP_STRERROR_GEN(n, s) { "HPE_" #n, s },
    static struct {
        const char *name;
        const char *description;
    } http_strerror_tab[] = {
            HTTP_ERRNO_MAP(HTTP_STRERROR_GEN)
    };
#undef HTTP_STRERROR_GEN
    assert(err < (sizeof(http_strerror_tab) / sizeof(http_strerror_tab[0])));
    return http_strerror_tab[err].name;
}

const char *
http_errno_description(enum http_errno err) {
/* Map errno values to strings for human-readable output */
#define HTTP_STRERROR_GEN(n, s) { "HPE_" #n, s },
    static struct {
        const char *name;
        const char *description;
    } http_strerror_tab[] = {
            HTTP_ERRNO_MAP(HTTP_STRERROR_GEN)
    };
#undef HTTP_STRERROR_GEN
    assert(err < (sizeof(http_strerror_tab) / sizeof(http_strerror_tab[0])));
    return http_strerror_tab[err].description;
}

inline static enum http_host_state
http_parse_host_char(enum http_host_state s, const char ch) {
    switch (s) {
        case sHTTP_userinfo:
        case sHTTP_userinfo_start:
            if (ch == '@') {
                return sHTTP_host_start;
            }

            if (_IS_USERINFO_CHAR(ch)) {
                return sHTTP_userinfo;
            }
            break;

        case sHTTP_host_start:
            if (ch == '[') {
                return sHTTP_host_v6_start;
            }

            if (_IS_HOST_CHAR(ch)) {
                return sHTTP_host;
            }

            break;

        case sHTTP_host:
            if (_IS_HOST_CHAR(ch)) {
                return sHTTP_host;
            }

            /* FALLTHROUGH */
        case sHTTP_host_v6_end:
            if (ch == ':') {
                return sHTTP_host_port_start;
            }

            break;

        case sHTTP_host_v6:
            if (ch == ']') {
                return sHTTP_host_v6_end;
            }

            /* FALLTHROUGH */
        case sHTTP_host_v6_start:
            if (_IS_HEX(ch) || ch == ':' || ch == '.') {
                return sHTTP_host_v6;
            }

            break;

        case sHTTP_host_port:
        case sHTTP_host_port_start:
            if (_IS_NUM(ch)) {
                return sHTTP_host_port;
            }

            break;

        default:
            break;
    }
    return sHTTP_host_dead;
}

inline int
http_parse_host(const char *buf, struct http_parser_url *u, int found_at) {
    enum http_host_state s;

    const char *p;
    size_t buflen = u->field_data[UF_HOST].off + u->field_data[UF_HOST].len;

    u->field_data[UF_HOST].len = 0;

    s = found_at ? sHTTP_userinfo_start : sHTTP_host_start;

    for (p = buf + u->field_data[UF_HOST].off; p < buf + buflen; p++) {
        enum http_host_state new_s = http_parse_host_char(s, *p);

        if (new_s == sHTTP_host_dead) {
            return 1;
        }

        switch (new_s) {
            case sHTTP_host:
                if (s != sHTTP_host) {
                    u->field_data[UF_HOST].off = p - buf;
                }
                u->field_data[UF_HOST].len++;
                break;

            case sHTTP_host_v6:
                if (s != sHTTP_host_v6) {
                    u->field_data[UF_HOST].off = p - buf;
                }
                u->field_data[UF_HOST].len++;
                break;

            case sHTTP_host_port:
                if (s != sHTTP_host_port) {
                    u->field_data[UF_PORT].off = p - buf;
                    u->field_data[UF_PORT].len = 0;
                    u->field_set |= (1 << UF_PORT);
                }
                u->field_data[UF_PORT].len++;
                break;

            case sHTTP_userinfo:
                if (s != sHTTP_userinfo) {
                    u->field_data[UF_USERINFO].off = p - buf;
                    u->field_data[UF_USERINFO].len = 0;
                    u->field_set |= (1 << UF_USERINFO);
                }
                u->field_data[UF_USERINFO].len++;
                break;

            default:
                break;
        }
        s = new_s;
    }

    /* Make sure we don't end somewhere unexpected */
    switch (s) {
        case sHTTP_host_start:
        case sHTTP_host_v6_start:
        case sHTTP_host_v6:
        case sHTTP_host_port_start:
        case sHTTP_userinfo:
        case sHTTP_userinfo_start:
            return 1;
        default:
            break;
    }

    return 0;
}

inline int
http_parser_parse_url(const char *buf, size_t buflen, int is_connect,
                      struct http_parser_url *u) {
    enum state s;
    const char *p;
    enum http_parser_url_fields uf, old_uf;
    int found_at = 0;

    u->port = u->field_set = 0;
    s = is_connect ? s_req_server_start : s_req_spaces_before_url;
    old_uf = UF_MAX;

    for (p = buf; p < buf + buflen; p++) {
        s = parse_url_char(s, *p);

        /* Figure out the next field that we're operating on */
        switch (s) {
            case s_dead:
                return 1;

                /* Skip delimeters */
            case s_req_schema_slash:
            case s_req_schema_slash_slash:
            case s_req_server_start:
            case s_req_query_string_start:
            case s_req_fragment_start:
                continue;

            case s_req_schema:
                uf = UF_SCHEMA;
                break;

            case s_req_server_with_at:
                found_at = 1;

                /* FALLTROUGH */
            case s_req_server:
                uf = UF_HOST;
                break;

            case s_req_path:
                uf = UF_PATH;
                break;

            case s_req_query_string:
                uf = UF_QUERY;
                break;

            case s_req_fragment:
                uf = UF_FRAGMENT;
                break;

            default:
                assert(!"Unexpected state");
                return 1;
        }

        /* Nothing's changed; soldier on */
        if (uf == old_uf) {
            u->field_data[uf].len++;
            continue;
        }

        u->field_data[uf].off = p - buf;
        u->field_data[uf].len = 1;

        u->field_set |= (1 << uf);
        old_uf = uf;
    }

    /* host must be present if there is a schema */
    /* parsing http:///toto will fail */
    if ((u->field_set & ((1 << UF_SCHEMA) | (1 << UF_HOST))) != 0) {
        if (http_parse_host(buf, u, found_at) != 0) {
            return 1;
        }
    }

    /* CONNECT requests can only contain "hostname:port" */
    if (is_connect && u->field_set != ((1 << UF_HOST) | (1 << UF_PORT))) {
        return 1;
    }

    if (u->field_set & (1 << UF_PORT)) {
        /* Don't bother with endp; we've already validated the string */
        unsigned long v = strtoul(buf + u->field_data[UF_PORT].off, NULL, 10);

        /* Ports have a max value of 2^16 */
        if (v > 0xffff) {
            return 1;
        }

        u->port = (uint16_t) v;
    }

    return 0;
}

inline void
http_parser_pause(http_parser *parser, int paused) {
    /* Users should only be pausing/unpausing a parser that is not in an error
     * state. In non-debug builds, there's not much that we can do about this
     * other than ignore it.
     */
    if (HTTP_PARSER_ERRNO(parser) == HPE_OK ||
        HTTP_PARSER_ERRNO(parser) == HPE_PAUSED) {
        _SET_ERRNO((paused) ? HPE_PAUSED : HPE_OK);
    } else {
        assert(0 && "Attempting to pause parser in error state");
    }
}

inline int
http_body_is_final(const struct http_parser *parser) {
    return parser->state == s_message_done;
}

inline unsigned long
http_parser_version(void) {
    return HTTP_PARSER_VERSION_MAJOR * 0x10000 |
           HTTP_PARSER_VERSION_MINOR * 0x00100 |
           HTTP_PARSER_VERSION_PATCH * 0x00001;
}

#ifdef __cplusplus
}
#endif

namespace suil {
    namespace http {

        int parser::on_message_begin(http_parser *s) {
            parser *p = static_cast<parser*>(s);
            p->clear(true);
            return 0;
        }

        int parser::on_url(http_parser *s, const char *at, size_t len) {
            parser *p = static_cast<parser*>(s);
            p->raw_url.append(at, (uint32_t)len);
            return 0;
        }

        int parser::on_header_field(http_parser *s, const char *at, size_t len) {
            parser *p = static_cast<parser*>(s);
            switch (p->state)
            {
                case STATE_FIELD:
                    if (p->hv)
                    {
                        zcstring hf(p->hf);
                        zcstring hv(p->hv);
                        p->headers.emplace(
                                std::move(hf),
                                std::move(hv));
                    }
                    p->hf.append(at, (uint32_t) len);
                    p->state = STATE_VALUE;
                    break;
                case STATE_VALUE:
                    p->hf.append(at, (uint32_t) len);
                    break;
                default:
                    break;
            }
            return 0;
        }

        int parser::on_header_value(http_parser *s, const char *at, size_t len) {
            parser *p = static_cast<parser*>(s);
            switch (p->state)
            {
                case STATE_FIELD:
                    p->hv.append(at, (uint32_t) len);
                    break;
                case STATE_VALUE:
                    p->state = 0;
                    p->hv.append(at, (uint32_t) len);
                    break;

                default:
                    break;
            }
            return 0;
        }

        int parser::on_headers_complete(http_parser *s) {
            parser *p = static_cast<parser*>(s);
            if (p->hf)
            {
                zcstring hf(p->hf);
                zcstring hv(p->hv);
                p->headers.emplace(
                        std::move(hf),
                        std::move(hv));
            }
            p->headers_complete = 1;
            return p->handle_headers_complete();
        }

        int parser::on_body(http_parser *s, const char *at, size_t len) {
            parser *p = static_cast<parser*>(s);
            return p->handle_body_part(at, len);
        }

        int parser::on_msg_complete(http_parser *s) {
            parser *p = static_cast<parser*>(s);
            // construct query string
            strview sv = p->raw_url;
            size_t pos = sv.find("?");
            auto url = sv.substr(0, pos);
            p->url = utils::strndup(url.data(), url.size());
            if (pos != sv.size()) {
                strview tmp((sv.data()+pos), sv.length()-pos);
                p->qps = query_string(tmp);
            }

            p->body_complete = 1;
            p->msg_complete();
            return 0;
        }

        parser::parser(http_parser_type type)
            : url(nullptr),
              body(0),
              hf(0),
              hv(0),
              raw_url(32)
        {
            http_parser_init(this, type);
        }

        int parser::handle_body_part(const char *at, size_t len) {
            body.append(at, len);

            return 0;
        }

        int parser::handle_headers_complete() {
            return 0;
        }

        bool parser::feed(const char *buf, size_t len) {
            const static http_parser_settings PARSER_SETTINGS {
                    parser::on_message_begin,
                    parser::on_url,
                    nullptr,
                    parser::on_header_field,
                    parser::on_header_value,
                    parser::on_headers_complete,
                    parser::on_body,
                    parser::on_msg_complete,
            };

            size_t nparsed = http_parser_execute(this, &PARSER_SETTINGS, buf, len);
            return nparsed == len;
        }

        void parser::clear(bool internal) {
            if (url) {
                memory::free(url);
                url = nullptr;
            }

            raw_url.clear();
            state = 0;
            headers_complete = 0;
            body_complete = 0;
            body.clear();
            hv.clear();
            hf.clear();
            headers.clear();
            qps.clear();
        }
    }
}