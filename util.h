/*
 * twemproxy - A fast and lightweight proxy for memcached protocol.
 * Copyright (C) 2011 Twitter, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#define true 1
#define false 0
#define LF                  (uint8_t) 10
#define CR                  (uint8_t) 13
#define CRLF                "\x0d\x0a"
#define CRLF_LEN            (sizeof("\x0d\x0a") - 1)
#define NC_OK        0
#define NC_ERROR    -1
#define NC_EAGAIN   -2
#define NC_ENOMEM   -3
#define NC_CLOSE    -4

#define NELEMS(a)           ((sizeof(a)) / sizeof((a)[0]))

#define MIN(a, b)           ((a) < (b) ? (a) : (b))
#define MAX(a, b)           ((a) > (b) ? (a) : (b))

#define SQUARE(d)           ((d) * (d))
#define VAR(s, s2, n)       (((n) < 2) ? 0.0 : ((s2) - SQUARE(s)/(n)) / ((n) - 1))
#define STDDEV(s, s2, n)    (((n) < 2) ? 0.0 : sqrt(VAR((s), (s2), (n))))

#define SDK_INET4_ADDRSTRLEN (sizeof("255.255.255.255") - 1)
#define SDK_MAXHOSTNAMELEN   256

/*
 * Wrapper to workaround well known, safe, implicit type conversion when
 * invoking system calls.
 */
#define sdk_gethostname(_name, _len) \
    gethostname((char *)_name, (size_t)_len)

#define sdk_atoi(_line, _n)          \
    _sdk_atoi((uint8_t *)_line, (size_t)_n)

typedef unsigned int bool;

int sdk_set_blocking(int sd);
int sdk_set_nonblocking(int sd);
int sdk_set_reuseaddr(int sd);
int sdk_set_tcpnodelay(int sd);
int sdk_set_linger(int sd, int timeout);
int sdk_set_sndbuf(int sd, int size);
int sdk_set_rcvbuf(int sd, int size);
int sdk_get_soerror(int sd);
int sdk_get_sndbuf(int sd);
int sdk_get_rcvbuf(int sd);
int sdk_set_keepalive(int sd);

int _sdk_atoi(uint8_t *line, size_t n);
bool sdk_valid_port(int n);

/*
 * Memory allocation and free wrappers.
 *
 * These wrappers enables us to loosely detect double free, dangling
 * pointer access and zero-byte alloc.
 */
#define sdk_alloc(_s)                    \
    _sdk_alloc((size_t)(_s), __FILE__, __LINE__)

#define sdk_zalloc(_s)                   \
    _sdk_zalloc((size_t)(_s), __FILE__, __LINE__)

#define sdk_calloc(_n, _s)               \
    _sdk_calloc((size_t)(_n), (size_t)(_s), __FILE__, __LINE__)

#define sdk_realloc(_p, _s)              \
    _sdk_realloc(_p, (size_t)(_s), __FILE__, __LINE__)

#define sdk_free(_p) do {                \
    _sdk_free(_p, __FILE__, __LINE__);   \
    (_p) = NULL;                        \
} while (0)

void *_sdk_alloc(size_t size, const char *name, int line);
void *_sdk_zalloc(size_t size, const char *name, int line);
void *_sdk_calloc(size_t nmemb, size_t size, const char *name, int line);
void *_sdk_realloc(void *ptr, size_t size, const char *name, int line);
void _sdk_free(void *ptr, const char *name, int line);

/*
 * Wrappers to send or receive n byte message on a blocking
 * socket descriptor.
 */
#define sdk_sendn(_s, _b, _n)    \
    _sdk_sendn(_s, _b, (size_t)(_n))

#define sdk_recvn(_s, _b, _n)    \
    _sdk_recvn(_s, _b, (size_t)(_n))

/*
 * Wrappers to read or write data to/from (multiple) buffers
 * to a file or socket descriptor.
 */
#define sdk_read(_d, _b, _n)     \
    read(_d, _b, (size_t)(_n))

#define sdk_readv(_d, _b, _n)    \
    readv(_d, _b, (int)(_n))

#define sdk_write(_d, _b, _n)    \
    write(_d, _b, (size_t)(_n))

#define sdk_writev(_d, _b, _n)   \
    writev(_d, _b, (int)(_n))

ssize_t _sdk_sendn(int sd, const void *vptr, size_t n);
ssize_t _sdk_recvn(int sd, void *vptr, size_t n);


#define ASSERT(_x) do {                         \
    if (!(_x)) {                                \
        sdk_assert(#_x, __FILE__, __LINE__, 1);  \
    }                                           \
} while (0)

#define NOT_REACHED() ASSERT(0)


void sdk_assert(const char *cond, const char *file, int line, int panic);

int64_t sdk_usec_now(void);
int64_t sdk_msec_now(void);

/*
 * Address resolution for internet (ipv4 and ipv6) and unix domain
 * socket address.
 */

struct sockinfo {
    int       family;              /* socket address family */
    socklen_t addrlen;             /* socket address length */
    union {
        struct sockaddr_in  in;    /* ipv4 socket address */
        struct sockaddr_in6 in6;   /* ipv6 socket address */
        struct sockaddr_un  un;    /* unix domain address */
    } addr;
};

struct msg {
    void *data;
    uint32_t len; 
};
int sdk_resolve(char *name, int port, struct sockinfo *si);
char *sdk_unresolve_addr(struct sockaddr *addr, socklen_t addrlen);
char *sdk_unresolve_peer_desc(int sd);
char *sdk_unresolve_desc(int sd);

#endif
