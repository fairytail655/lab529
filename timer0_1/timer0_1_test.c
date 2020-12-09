#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>

// 定时器写数据结构体
typedef struct 
{
    // 0-timer0, 1-timer1
    unsigned int timer_id;
    // TCNTBn
    unsigned int timer_cnt;
} timer0_1_wr_data;

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

void read_thread2(void)
{
    while (1)
    {
        printf("xixi\n");
        read(fd, NULL, 1);
    }
}

int main(int argc, char *argv[])
{
    int i;
    pthread_t read_thread_id, read_thread_id2;
    timer0_1_wr_data data0;
    data0.timer_id = 0;
    data0.timer_cnt = 25000;
    timer0_1_wr_data data1;
    data1.timer_id = 1;
    data1.timer_cnt = 50000;

    fd = open("/dev/timer0_1", O_RDWR);
    if (fd < 0)
    {
        printf("[timer0_1] open error!\n");
        return 1;
    }

    pthread_create(&read_thread_id, NULL, (void *)read_thread, NULL);
    pthread_create(&read_thread_id, NULL, (void *)read_thread2, NULL);
    write(fd, &data0, sizeof(data0));
    write(fd, &data1, sizeof(data1));

    // pthread_join(read_thread_id, NULL);
    while (1)
    {
        sleep(5);
    }

    close(fd);

    return 0;
}
