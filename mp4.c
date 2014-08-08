/*
   Used for get Mp4 files' duration
   Author: xiecc
   Email: xiechc@gmail.com
   Date: 2014-08-08
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef unsigned int uint32;
#define ntoh(A) ((((uint32)(A) & 0xff000000) >> 24) | (((uint32)(A) & 0x00ff0000) >> 8) | \
                (((uint32)(A) & 0x0000ff00) << 8) | (((uint32)(A) & 0x000000ff) << 24))
#define moov "moov"
#define mvhd "mvhd"

float mp4_duration(char *file)
{
    int fd;
    uint32 size;
    char type[4];
    ssize_t val;
    uint32 time_scale;
    uint32 duration;
    
    fd = open(file, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    while (1) {
        val = read(fd, &size, sizeof(size));
        if (val < sizeof(size)) {
            return -1;
        }
        size = ntoh(size);
        val = read(fd, type, sizeof(type));
        if (val < sizeof(type)) {
            return -1;
        }
        if (memcmp(type, moov, strlen(moov)) != 0) {
            lseek(fd, (size - 8) ,SEEK_CUR);
        }else break;
    }

    while (1) {
        val = read(fd, &size, sizeof(size));
        if (val < sizeof(size)) {
            return -1;
        }
        size = ntoh(size);
        val = read(fd, type, sizeof(type));
        if (val < sizeof(type)) {
            return -1;
        }
        if (memcmp(type, mvhd, strlen(mvhd)) == 0) {
            lseek(fd, 12, SEEK_CUR);
            val = read(fd, &time_scale, sizeof(time_scale));
            if (val < sizeof(time_scale)) {
                return -1;
            }
            time_scale = ntoh(time_scale);
            val = read(fd, &duration, sizeof(duration));
            if (val < sizeof(duration)) {
                return -1;
            }
            duration = ntoh(duration);
            break;
        }
    }
    close(fd);
    return ((float )duration / time_scale);;
}

int main(int argc, char **argv)
{
    printf("%f\n", mp4_duration(argv[1]));
}
