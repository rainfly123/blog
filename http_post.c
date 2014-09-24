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
#include <sys/types.h>
#include <dirent.h>
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

    tv.tv_sec = 10;
    tv.tv_usec = 0;

    while (1) {

        FD_ZERO(&writefds);
        FD_SET(skt, &writefds);

        val = select(skt + 1, NULL, &writefds, NULL, &tv); 
        if (val < 0) {
            return -1;
        }
        if (val == 0) {
            continue;
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
    int len;
    char *data;
    ssize_t val;
    char md5[48];

    file_md5(file, md5, sizeof(md5));
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
    int len;
    char *data;
    ssize_t val;
    char md5[48];

    file_md5(file, md5, sizeof(md5));
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
    char *event_file_path;
    int remove;
};


static void * http_post_thread(void *data)
{
    char ip[48];
    char address[48];
    int port;
    int val1 = -1;
    int val2 = -1;
    char *type ;
    struct data *p = (struct data *)data;
    int tryy = 1;

	printf("http_post_thread file:%s event_file_path:%s start.........\n", p->file, p->event_file_path);

    pthread_detach(pthread_self());   //yanghongbo

    do {
        val1 = get_server (p->obdid, p->token, ip, &port, address);
        if (val1 == 0) 
             break;
        sleep((tryy * 5));
        tryy++;
    } while (tryy <= 3);

    type = strstr(p->file, "mp4"); 
	printf("val1:%d get the ip address.\n", val1);
    if (val1 == 0) {
        tryy = 1;
	do {
            if (type != NULL)
	        val2 = http_post_mp4(p->file, p->obdid, p->token, p->eventid, p->cameraid, ip, port, address);
            else
                val2 = http_post_jpg(p->file, p->obdid, p->token, p->eventid, p->cameraid, ip, port, address);
            if (val2 == 0)
                break;
            sleep((tryy * 10));
            tryy++;
	} while (tryy <= 5);
   }

   printf("val2:%d......\n", val2);
   if ((val2 == 0) && (p->remove == 1)) {
       unlink(p->file);
    }
   if (val2 != 0) 
   {
        char dst[255];
        if (type != NULL) {
            sprintf(dst, "%s/up_%s_%s.mp4", p->event_file_path, p->obdid, p->eventid);
        }
        else {
            sprintf(dst, "%s/up_%s_%s.jpg", p->event_file_path, p->obdid, p->eventid);
        }
        rename(p->file, dst);
    }

	printf("http_post_thread file:%s event_file_path:%s end.......\n", p->file, p->event_file_path);
    free(p->event_file_path);
    free(p->cameraid);
    free(p->eventid);
    free(p->token);
    free(p->obdid);
    free(p->file);
    free(p);

    return (void *)0;
}

int http_post_start(char *file, char *obdid, char *token, char *eventid, char *cameraid, int remove, char *event_file_path)
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
    if (event_file_path == NULL)
       return -1;
    if (access(file, R_OK) != 0)
       return -1;

    p = (struct data *) calloc(1, sizeof(struct data)); 
    p->file =  strdup(file);
    p->obdid =  strdup(obdid);
    p->token =  strdup(token);
    p->eventid =  strdup(eventid);
    p->cameraid =  strdup(cameraid);
    p->remove = remove;
    p->event_file_path = strdup(event_file_path);
    pthread_create(&tid, NULL, http_post_thread, p);
    //http_post_thread(p);
    return 0;
}

static int get_token(char *obdid, char *token) {
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
    body_len = sprintf(body, "{\"obdid\":\"%s\"}", obdid);
    mess_len = sprintf(mess, "POST /regobd HTTP/1.0\r\n");
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
	printf("response:%s\n", response);
    pbody = strstr(response, "token");
    if (pbody == NULL) {
        return -1;
    }
    pbody += strlen("token");
    do {
        if (isalnum(*pbody))
           break;
    }while (*pbody++ != '\0');

    while (*pbody != '"')
        *token++ = *pbody++;
	*token = '\0';
    return 0;

}

void upload_old_file(char *event_file_path, char *cameraid) {
    
    char token[96];
    char file[255];

    DIR *dir_p;
    struct  dirent *dp;
    char *p;
    char *d;
    char obdid[64];
    char eventid[64];
    int val;
    time_t now;
    struct stat info;
    memset(token, 0, sizeof(token));

    if  ((dir_p = opendir (event_file_path)) == NULL)
    {
        return ;
    }
    while  ((dp = readdir (dir_p)) != NULL)
    {
        p = strstr(dp->d_name, "_");
        if (p == NULL) {
            continue;
        }
        //delete the old file
        memset(file, 0, sizeof(file));
        sprintf(file, "%s/%s", event_file_path, dp->d_name);
        p = strstr(dp->d_name, "up");
        if (p == NULL) {
            now = time(NULL);
            stat(file, &info);
            if ((now - info.st_mtime) > (3600 * 24 * 7))
                unlink(file);
            continue;
        }
        //upload 
        memset(obdid, 0, sizeof(obdid));
        p = strstr(dp->d_name, "up_");
        p += strlen("up_");
        d = obdid;
        while (*p != '_')
           *d++ = *p++;

        memset(eventid, 0, sizeof(eventid));
        d = eventid;
        p++;
        while (*p != '.')
           *d++ = *p++;

        printf("file:%s\n", file);
        if (strlen(token) == 0)
        {
            val = get_token(obdid, token);
            if (val != 0)
                break;
        }
        printf("obid %s\n eventid %s\n token %s \n", obdid, eventid, token);
        http_post_start(file, obdid, token, eventid, cameraid, 1, event_file_path);
    }
    closedir (dir_p);

}

struct thread_data
{
    char * event_file_path;
    char * cameraid;
};

static void *upload_thread(void *p)
{
    struct thread_data *data = (struct thread_data *)p;
    while (1) {
        printf("upload thread starting\n");
        upload_old_file(data->event_file_path, data->cameraid);
        sleep(1200);
    } 
}

int start_reupload_thread(char *event_file_path, char *cameraid)
{
    struct thread_data *p;
    pthread_t tid;

    if (event_file_path == NULL)
       return -1;
    if (cameraid == NULL)
       return -1;
    if (access(event_file_path, R_OK) != 0)
       return -1;

    p = (struct thread_data *) calloc(1, sizeof(struct thread_data)); 
    p->event_file_path =  strdup(event_file_path);
    p->cameraid =  strdup(cameraid);
    pthread_create(&tid, NULL, upload_thread, p);
    return 0;
}

#if 0
int main()
{
 char * token = "bWrJoFRfpoRxOO7HoWYM0ET16iroUnFqRn1Lpc3-6co";
 char *obdid = "testobd01";
 char *cameraid = "camera1";
 int c;
 //http_post_start("08301519_0004.jpg", obdid, token, "200230", cameraid, 1);
 http_post_start("08051400_0233_thm.mp4", obdid, token, "200230", cameraid, 1, "./");
 c = getchar();
}
#endif
#if 0
int main()
{
    int c;
    start_reupload_thread("/home/xiecc/ipc/event", "test1234");
    c = getchar();
}
#endif
