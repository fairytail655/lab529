/*
 * @Description: -
 * @Version: -
 * @Author: Fox_benjiaming
 * @Date: 2020-10-21 01:06:55
 * @LastEditors: Fox_benjiaming
 * @LastEditTime: 2020-12-09 00:05:16
 */
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int fd;

    fd = open("/dev/tv_control", O_RDWR);

    while (1)
    {
        read(fd, NULL, 1);
        printf("bin interrupt\n");
        sleep(2);
    }

    close(fd);

    return 0;
}
