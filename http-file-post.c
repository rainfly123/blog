#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <pthread.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "util.h"
#include "md5.h"

static int set_nonblock(int sd)
{
    int flags;

    flags = fcntl(sd, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }
    return fcntl(sd, F_SETFL, flags | O_NONBLOCK);
}

static int ipc_connect(char *ip, int port)
{
    struct sockaddr_in addr;
    int skt;

    skt = socket(AF_INET, SOCK_STREAM, 0);

    memset(&addr, 0, sizeof(addr) );
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);

    connect(skt, (struct sockaddr *)&addr, sizeof(addr));
    set_nonblock(skt);
    return skt;
}

static int ipc_send(int skt, char *buf, int len)
{
    fd_set writefds;
    struct timeval tv;
    int ret ;
    int val;
    int total = 0;

    tv.tv_sec = 5;
    tv.tv_usec = 0;

    while (1) {

        FD_ZERO(&writefds);
        FD_SET(skt, &writefds);

        val = select(skt + 1, NULL, &writefds, NULL, &tv); 
        if (val <= 0) {
            return -1;
        }
        if (FD_ISSET(skt, &writefds)) {
            ret = send(skt, buf + total, len - total, 0);
            if (ret == 0) {
                return -1;
            }
	    else if (ret < 0){
                if (errno == EINTR)
                    continue;
		if ((errno != EAGAIN) || (errno != EWOULDBLOCK)) {
                    return -1;
                }
	    }
           else {
               total += ret;
               if (total == len)
                   return 0;
           }
	} 
    }
    return 0;
}

static int get_server (char *obdid, char *token, char *ip, int *port, char *address)
{
    char *ntp = "i.auto.soooner.com";
    struct sockinfo sock;
    int skt;
    int mess_len;
    char mess[256];
    char body[128];
    int body_len;
    char response[512];
    int val;
    char *pbody ;


    sdk_resolve(ntp, 80, &sock);
    skt = socket(AF_INET, SOCK_STREAM, 0);
    val = connect(skt, (struct sockaddr *)&sock.addr, sock.addrlen);
    if (val < 0) {
        return -1;
    }
    body_len = sprintf(body, "{\"obdid\":\"%s\", \"token\":\"%s\"}", obdid, token);
    mess_len = sprintf(mess, "POST /getserver HTTP/1.0\r\n");
    mess_len += sprintf(mess + mess_len, "HOST:%s\r\n", ntp);
    mess_len += sprintf(mess + mess_len, "Content-Type:application/x-www-form-urlencoded\r\n");
    mess_len += sprintf(mess + mess_len, "Content-Length:%d\r\n\r\n", body_len);
    mess_len += sprintf(mess + mess_len, "%s", body);
ReSend:
    val = send(skt, mess, mess_len, 0);
    if (val < 0){
        if (errno == EINTR)
            goto ReSend;
    }
ReRecv:
    val = recv(skt, response, sizeof(response), 0);
    if (val < 0){
        if (errno == EINTR)
            goto ReRecv;
    }
    close (skt);
    pbody = strstr(response, "uploadurl");
    if (pbody == NULL) {
        return -1;
    }
    pbody += strlen("uploadurl");
    do {
        if (isdigit(*pbody))
           break;
    }while (*pbody++ != '\0');

    while (isdigit(*pbody) || *pbody == '.')
        *ip++ = *pbody++;

    char *pport;
    char  *pslash ;
    pport = strchr(pbody, ':');
    if (pport == NULL) {
       *pport = 80;
       pslash = strchr(pbody, '/');
       pslash++;
    }
    else {
       pslash = strchr(pport, '/');
       *pslash++ = '\0';
       *port = atoi(++pport);
    }
    while (isalpha(*pslash))
        *address++ = *pslash++;
    return 0;
}

 


#define METHOD "POST /%s?obdid=%s&eventid=%s&token=%s&md5=%s&cameraid=%s HTTP/1.0\r\n"
#define USER_AGENT  "User-Agent: SOOONER-SDK-xiecc\r\n"
#define CON_LEN "Content-Length: %d\r\n"
#define CON_TYPE  "Content-Type: multipart/form-data; boundary=------xiecc\r\n\r\n"

#define BOUNDARY_S "--------xiecc\r\n"
#define FILE_NAME "Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
#define FILE_TYPE_JPG "Content-Type: image/jpeg\r\n\r\n"
#define FILE_TYPE_MP4 "Content-Type: video/mp4\r\n\r\n"
#define BOUNDARY_E "\r\n--------xiecc--\r\n"
static int http_post_jpg(char *file, char *obdid, char *token, char *eventid, char *cameraid, 
                          char *ip, int port, char *address)
{
    int bound_len = strlen(BOUNDARY_S) + strlen(FILE_NAME) + strlen(file) + \
                   strlen(FILE_TYPE_JPG) + strlen(BOUNDARY_E) - 2;

    struct stat stat;
    char temp[8];
    int len;
    char *data;
    ssize_t val;
    char md5[48];

    file_md5(file, md5, sizeof(md5));
    memset(temp, 0, sizeof(temp));
    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    fstat(fd, &stat);
    data = (char *) malloc(512 + stat.st_size);
    memset(data, 0, (512 + stat.st_size));
    len = sprintf(data, METHOD, address, obdid, eventid, token, md5, cameraid);
    len += sprintf(data + len, USER_AGENT);
    len += sprintf(data + len, CON_LEN, bound_len + (int)stat.st_size);
    len += sprintf(data + len, CON_TYPE);
    len += sprintf(data + len, BOUNDARY_S);
    len += sprintf(data + len, FILE_NAME, file);
    len += sprintf(data + len, FILE_TYPE_JPG);

    val = read(fd, (void *)(data + len), stat.st_size);
    close(fd);
    if (val < 0){
        free(data);
        return -1;
    }
    len += val; 
    len += sprintf(data + len , "%s", BOUNDARY_E); 
    int skt;
    int res;
    skt = ipc_connect(ip, port);
    if (skt < 0)
        return -1;
    res = ipc_send(skt, (char *)data, len);
    close(skt);
    free(data);
    return res;
}
static int http_post_mp4(char *file, char *obdid, char *token, char *eventid, char *cameraid, 
                          char *ip, int port, char *address)
{
    int bound_len = strlen(BOUNDARY_S) + strlen(FILE_NAME) + strlen(file) + \
                   strlen(FILE_TYPE_MP4) + strlen(BOUNDARY_E) - 2;

    struct stat stat;
    char temp[8];
    int len;
    char *data;
    ssize_t val;
    char md5[48];

    file_md5(file, md5, sizeof(md5));
    memset(temp, 0, sizeof(temp));
    int fd = open(file, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    fstat(fd, &stat);
    data = (char *) malloc(512 + stat.st_size);
    memset(data, 0, (512 + stat.st_size));
    len = sprintf(data, METHOD, address, obdid, eventid, token, md5, cameraid);
    len += sprintf(data + len, USER_AGENT);
    len += sprintf(data + len, CON_LEN, bound_len + (int)stat.st_size);
    len += sprintf(data + len, CON_TYPE);
    len += sprintf(data + len, BOUNDARY_S);
    len += sprintf(data + len, FILE_NAME, file);
    len += sprintf(data + len, FILE_TYPE_MP4);

    val = read(fd, (void *)(data + len), stat.st_size);
    close(fd);
    if (val < 0){
        free(data);
        return -1;
    }
    len += val; 
    len += sprintf(data + len , "%s", BOUNDARY_E); 
    int skt;
    int res;
    skt = ipc_connect(ip, port);
    if (skt < 0)
        return -1;
    res = ipc_send(skt, (char *)data, len);
    close(skt);
    free(data);
    return res;
}
//int http_post_jpg()
//int http_post_jpg()
/*
int main(void){
char ip[48];
int port;
char address[48];
get_server ("testobd01", "bWrJoFRfpoRxOO7HoWYM0NLDcLqyJccPmyjlyNKGNRw",ip, &port, address);
printf("%s %s %d\n", ip, address, port);
http_post_mp4("1407171636_0001_thm.mp4" , "testobd01", "bWrJoFRfpoRxOO7HoWYM0N7uxaSawckGC8a2Z9yDkxM",\
  "201408", "800", ip, port, address);
http_post_jpg("1407171636_0001.jpg" , "testobd01", "bWrJoFRfpoRxOO7HoWYM0N7uxaSawckGC8a2Z9yDkxM",\
  "201408", "800", ip, port, address);
}
*/
struct data {
    char *file;
    char *obdid;
    char *token; 
    char *eventid;
    char *cameraid;
};


static void * http_post_thread(void *data)
{
    char ip[48];
    char address[48];
    int port;
    int val;
    char *type ;
    struct data *p = (struct data *)data;
    int tryy = 1;

    pthread_detach(pthread_self());

    do {
        val = get_server (p->obdid, p->token, ip, &port, address);
        if (val == 0) 
             break;
        sleep((tryy * 10));
        tryy++;
    } while (tryy <= 3);

    if (tryy >= 4) {
        free(p->cameraid);
        free(p->eventid);
        free(p->token);
        free(p->obdid);
        free(p->file);
        free(p);
        return (void *)0;
    }

    tryy = 1;
    type = strstr(p->file, "mp4"); 
    do {
        if (type != NULL)
           val = http_post_mp4(p->file, p->obdid, p->token, p->eventid, p->cameraid, ip, port, address);
       else
           val = http_post_jpg(p->file, p->obdid, p->token, p->eventid, p->cameraid, ip, port, address);
       if (val == 0)
           break;
        sleep((tryy * 10));
        tryy++;
    } while (tryy <= 5);

    free(p->cameraid);
    free(p->eventid);
    free(p->token);
    free(p->obdid);
    free(p->file);
    free(p);
    return (void *)0;
}

int http_post_start(char *file, char *obdid, char *token, char *eventid, char *cameraid)
{
    struct data *p;
    pthread_t tid;

    if (file == NULL)
       return -1;
    if (obdid == NULL)
       return -1;
    if (token == NULL)
       return -1;
    if (eventid == NULL)
       return -1;
    if (cameraid == NULL)
       return -1;
    if (access(file, R_OK) != 0)
       return -1;

    p = calloc(1, sizeof(struct data)); 
    p->file =  strdup(file);
    p->obdid =  strdup(obdid);
    p->token =  strdup(token);
    p->eventid =  strdup(eventid);
    p->cameraid =  strdup(cameraid);
    pthread_create(&tid, NULL, http_post_thread, p);
    return 0;
}
