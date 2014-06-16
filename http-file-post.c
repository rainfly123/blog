struct file{
    struct con connection;
    uint32_t camera_id;
    uint8_t sbuffer[256];
    uint32_t slen;
    uint32_t send_bytes;

    Queue *msg_queue; //filename "xiecc_camea-20133120343.ts" and so on
    struct msg *p_msg;
    struct sockinfo file_addr;
    bool authenticated;
};#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "file.h"
#include "log.h"
#include "util.h"
#include "gconf.h"
#include "event.h"
#include "client.h"
#include "queue.h"


//max 16 cameras
static struct file * g_file [16];

static void file_close(struct con * conn)
{
    ASSERT(conn != NULL);
    struct file * filer = (struct file *) conn->ctx;
    close(conn->skt);
    g_file[filer->camera_id] = NULL;
    sdk_free(filer);
}

static int file_recv(struct con *conn)
{
    ssize_t n;

    ASSERT(conn != NULL);
    struct file * filer = (struct file *) conn->ctx;
    char rbuffer[256];
    
    memset(rbuffer, 0, sizeof(rbuffer));
    for (;;) {
        n = sdk_read(conn->skt, rbuffer, sizeof(rbuffer));
        log_debug(LOG_VERB, "recv on sd %d %zd %s ", conn->skt, n, rbuffer);
        if (n > 0) {
            log_debug(LOG_VERB, "recv on sd return  ");
            return NC_OK;
        }

        if (n == 0) {
            log_debug(LOG_INFO, "recv on sd %d eof rb ", conn->skt);
            return NC_CLOSE;
        }

        if (errno == EINTR) {
            log_debug(LOG_VERB, "recv on sd %d not ready - eintr", conn->skt);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            log_debug(LOG_VERB, "recv on sd %d not ready - eagain", conn->skt);
            return NC_OK;
        } else {
            conn->err = errno;
            log_error("recv on sd %d failed: %s", conn->skt, strerror(errno));
            return NC_ERROR;
        }
    }

    NOT_REACHED();
    return NC_ERROR;
}

static int file_send(struct con *conn)
{
    ASSERT(conn != NULL);
    ssize_t n;
    struct file * filer = (struct file *) conn->ctx;

    for (;;) 
    {
        if (filer->authenticated == false) {
            n = sdk_write(conn->skt, (filer->sbuffer + filer->send_bytes),\
                     (filer->slen - filer->send_bytes));
            log_debug(LOG_VERB, "send on sd %d %zd",  conn->skt, n);
        } else {
            if (filer->p_msg == NULL) {
               if (sdk_get_size(filer->msg_queue))
                  sdk_pop_queue(filer->msg_queue, (void*)&(filer->p_msg));
               else
                   return NC_OK;
            }
            n = sdk_write(conn->skt, filer->p_msg->data + filer->send_bytes,\
                              filer->p_msg->len - filer->send_bytes);
        }

        if (n > 0) {
            filer->send_bytes += (size_t)n;
            if (filer->authenticated == false) {
                if (filer->slen == filer->send_bytes)
                {
                    filer->send_bytes = 0;
                    filer->authenticated = true;
                    log_debug(LOG_VERB, "send on sd %d complete slen %d\n", conn->skt, filer->slen);
                    event_del_out(filer->connection.evb, &filer->connection);
                    return NC_OK;
              //send completed;
                }
            }
            else {
                if (filer->send_bytes == filer->p_msg->len) {
                    sdk_free(filer->p_msg->data);
                    sdk_free(filer->p_msg);
                    filer->p_msg = NULL;
                    filer->send_bytes = 0;
                    event_del_out(filer->connection.evb, &filer->connection);
                }
                
            }
        } else {

        if (errno == EINTR) {
            log_debug(LOG_VERB, "send on sd %d not ready - eintr", conn->skt);
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            log_debug(LOG_VERB, "send on sd %d not ready - eagain", conn->skt);
            return NC_OK;
        } else {
            conn->err = errno;
            log_error("send on sd %d failed: %s", conn->skt, strerror(errno));
            return NC_ERROR;
        }
        }
    }

    NOT_REACHED();
    return NC_ERROR;
}

//real_server "193.13.3.3:8000"
int sdk_file_add_camera(struct event_base *evb, int camera_id, char *server)
{
    struct file *filer;
    int skt;
    char *port ;
    int sport = 80;
    char *real_server;

    if (g_file[camera_id] != NULL)
       return -1;

    real_server = strdup(server);
    ASSERT(evb != NULL);
    ASSERT(camera_id >= 0);
    ASSERT(camera_id <= 15);
    filer = sdk_calloc(1, sizeof(struct file));
    ASSERT(filer != NULL);
  
    port = strchr(real_server, ':');
    if (port != NULL) {
        *port = '\0';
        sport = sdk_atoi(++port, 5);
    }
    sdk_resolve(real_server, sport, &filer->file_addr);
    
    g_file[camera_id] = filer;
    filer->camera_id = camera_id;
    filer->msg_queue = sdk_init_queue();
    filer->slen = sdk_build_file_request(camera_id, filer->sbuffer, sizeof(filer->sbuffer));
    filer->connection.type = tFILE;
    filer->connection.evb = evb;
    filer->authenticated  = true;

    skt = sdk_tcp_socket();
    sdk_set_nonblocking(skt);
    sdk_set_sndbuf(skt, 64*1024);
    sdk_set_rcvbuf(skt, 64*1024);
    sdk_set_tcpnodelay(skt);

    filer->connection.skt = skt;
    filer->connection.send = &file_send;
    filer->connection.recv = &file_recv;
    filer->connection.close = &file_close;
    filer->connection.ctx = filer;
    int status = connect(skt, (struct sockaddr *)&filer->file_addr.addr, filer->file_addr.addrlen);
    if (status != 0) {
        if (errno != EINPROGRESS)
            filer->connection.err = errno;
    }
    event_add_conn(evb, &filer->connection);
    log_debug(LOG_DEBUG, "add file camera %d", camera_id);
    free(real_server);
    return 0;
}

int sdk_file_releae_camera(int camera_id)
{
    ASSERT(camera_id >= 0);
    ASSERT(camera_id <= 15);

    struct file * filer = g_file[camera_id];
    close(filer->connection.skt);
    sdk_free(filer);
    g_file[camera_id] = NULL;
    return 0;
}

#define METHOD "POST /upload HTTP/1.1\r\n"
#define USER_AGENT  "User-Agent: SOOONER-SDK-xiecc\r\n"
#define CON_TYPE  "Content-Type: multipart/form-data; boundary=---xiecc\r\n"
#define CON_LEN "Content-Length: %d\r\n\r\n"

#define BOUNDARY_S "---xiecc\r\n"
#define FILE_NAME "Content-Disposition: form-data; name=\"file\"; filename=%s\r\n"
#define FILE_TYPE "Content-Type: application/octet-stream\r\n\r\n"
#define BOUNDARY_E "\r\n---xiecc--\r\n"

static int build_http_body(struct file * filer, char *file)
{
    int bound_len = strlen(BOUNDARY_S) + strlen(FILE_NAME) + strlen(file) + strlen(FILE_TYPE)\
                    + strlen(BOUNDARY_E) - 2;
    int head_len = strlen(METHOD) + strlen(USER_AGENT) + strlen(CON_LEN) + strlen(CON_TYPE) - 2;
    int body_len;

    struct stat stat;
    char temp[8];
    int len;
    int buff_len;
    struct msg *message;
    ssize_t val;

    memset(temp, 0, sizeof(temp));
    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        log_error("open file error %s %s", file, strerror(errno));
        return -1;
    }
    fstat(fd, &stat);
    body_len = bound_len + stat.st_size;
    len = sprintf(temp, "%d", body_len);
    buff_len = head_len + len + body_len;
 
    message = sdk_calloc(1, sizeof(struct msg));
    message->data = sdk_calloc(1, buff_len);
    message->len = buff_len;
    len = sprintf(message->data, METHOD);
    len += sprintf(message->data + len, USER_AGENT);
    len += sprintf(message->data + len, CON_TYPE);
    len += sprintf(message->data + len, CON_LEN, body_len);
    len += sprintf(message->data + len, BOUNDARY_S);
    len += sprintf(message->data + len, FILE_NAME, file);
    len += sprintf(message->data + len, FILE_TYPE);

    val = read(fd, (void *)(message->data + len), stat.st_size);
    close(fd);
    if (val < 0){
        sdk_free(message->data);
        sdk_free(message);
        log_error("read file error %s", strerror(errno));
        return -1;
    }
    
    sprintf(message->data + len + val, "%s", BOUNDARY_E); 
    sdk_push_queue(filer->msg_queue, message);
    return 0;
}

int sdk_file_push(int camera_id, char *file_name)
{
    if (g_file[camera_id] == NULL)
        return -1;

    struct file *filer = g_file[camera_id];
    build_http_body(filer, file_name);
    event_add_out(filer->connection.evb, &filer->connection);
    return 0;
}
