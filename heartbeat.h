/********************************************************************************
---Author: xiecc
---E-mail: xiechc@gmail.com
---Date: 2014-03-26
*********************************************************************************/



#include "util.h"

typedef int (*RTMPUserInvoke)(void *cb_ctx, const char* cmd, const char *src, int len, char* dst, int dst_len);

typedef struct 
{
    struct msg buffer;
    struct msg cmd_buffer;
    int skt;
    int cmd_skt;
    int done;
    struct sockinfo sock;
    pthread_t thread;
    RTMPUserInvoke callback;
    void *rtmp;
    int state;
    int send_cmd;
} heartbeat;

typedef heartbeat * Handle;

/**
* @brief allocate a hearbeat handle
* @url
* @log FILE
*/
Handle sdk_heartbeat_init(char *url, RTMPUserInvoke cb, void *data);

/**
* @brief close a tcp_push handle
* @handle  allocated using sdk_tcp_push_init
*/
void sdk_heartbeat_close(Handle handle);


