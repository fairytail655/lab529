#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>

#define READ_BUFF_SIZE 32

// 串口1文件句柄
int fd;

void read_thread(void)
{
    while (1)
    {
        printf("haha\n");
        read(fd, NULL, 0);
    }
}

int main(int argc, char *argv[])
{
    int i;
    pthread_t read_thread_id;
    unsigned int val = 25000;

    fd = open("/dev/timer1", O_RDWR);

    pthread_create(&read_thread_id, NULL, (void *)read_thread, NULL);

    // pthread_join(read_thread_id, NULL);
    while (1)
    {
        write(fd, &val, 2);
        sleep(5);
    }

    close(fd);

    return 0;
}
