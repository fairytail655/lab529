#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>

#define READ_BUFF_SIZE 32

// 串口1文件句柄
int fd;
// 单次串口数据缓冲区
unsigned char uart_read_buffer[READ_BUFF_SIZE];
unsigned int uart_read_count;
// 单次串口数据读取完成标志位,需要手动复位
bool read_finish_flag = false;

void read_thread(void)
{
    int i;

    memset(uart_read_buffer, 0, sizeof(uart_read_buffer));
    printf("I am read_thread:%d\n", pthread_self());

    while (1)
    {
        // if (read_finish_flag == false)
        // {
        uart_read_count = read(fd, uart_read_buffer, READ_BUFF_SIZE);
        for (i = 0; i < uart_read_count; i++)
        {
            printf("%d ", uart_read_buffer[i]);
        }
        printf("\n");
        // }
    }
}

int main(int argc, char *argv[])
{
    int i;
    char str[7] = {0x03, 0xff, 0x07, 0x04, 0x01, 0x00, 0xfe};
    pthread_t read_thread_id;

    fd = open("/dev/card_uart", O_RDWR);

    printf("I am main:%d\n", pthread_self());
    pthread_create(&read_thread_id, NULL, (void *)read_thread, NULL);
    // write(fd, str, 7);

    // pthread_join(read_thread_id, NULL);
    while (1)
    {
        write(fd, str, 7);
        sleep(1);
        // if (read_finish_flag == true)
        // {
        //     // printf("main count: %d\n", uart_read_count);
        //     for (i = 0; i < uart_read_count; i++)
        //     {
        //         printf("%d ", uart_read_buffer[i]);
        //     }
        //     printf("\n");
        //     read_finish_flag = false;
        // }
    }

    close(fd);

    return 0;
}
