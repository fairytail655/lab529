/*
 * @Description: -
 * @Version: -
 * @Author: Fox_benjiaming
 * @Date: 2020-10-21 01:06:55
 * @LastEditors: Fox_benjiaming
 * @LastEditTime: 2020-10-22 01:25:00
 */
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>

#define SEND_BUFF_SIZE 11
#define READ_BUFF_SIZE 32

void calculate_crc(unsigned char *data, int x_len, int y_len)
{
    int i, j;
    unsigned char crc = 0;
    unsigned char *ptr;

    for (i = 0; i < x_len; i++)
    {
        ptr = data + i*y_len;
        for (j = 0; j < y_len-1; j++)
            crc += *(ptr + j);
        *(ptr + j) = crc;
    }
}

unsigned char send_buff[][SEND_BUFF_SIZE] = {
//发送学习模块存储器1里数据的指令
{0x7e, 0x07, 0x00, 0x59, 0x39, 0x59, 0x39, 0x14, 0x00, 0},
//发送学习模块存储器2里数据的指令
{0x7e, 0x07, 0x00, 0x59, 0x39, 0x59, 0x39, 0x14, 0x01, 0},
//发送学习模块存储器3里数据的指令
{0x7e, 0x07, 0x00, 0x59, 0x39, 0x59, 0x39, 0x14, 0x02, 0},
//发送学习模块存储器4里数据的指令
{0x7e, 0x07, 0x00, 0x59, 0x39, 0x59, 0x39, 0x14, 0x03, 0},
//发送学习模块存储器5里数据的指令
{0x7e, 0x07, 0x00, 0x59, 0x39, 0x59, 0x39, 0x14, 0x04, 0},
//发送学习模块存储器6里数据的指令
{0x7e, 0x07, 0x00, 0x59, 0x39, 0x59, 0x39, 0x14, 0x05, 0},
//发送学习模块存储器7里数据的指令
{0x7e, 0x07, 0x00, 0x59, 0x39, 0x59, 0x39, 0x14, 0x06, 0}
};

// 串口2文件句柄
int fd;
// 单次串口数据缓冲区
unsigned char uart_read_buffer[READ_BUFF_SIZE];
unsigned int uart_read_count;
// 单次串口数据读取完成标志位,需要手动复位
bool read_finish_flag = false;

void read_thread(void)
{
    read(fd, NULL, 0);
    printf("haha\n");
}

int main(int argc, char *argv[])
{
    int i;
    pthread_t read_thread_id;
    char *str = "aaa\n";

    fd = open("/dev/tv_control", O_RDWR);

    // printf("I am main:%d\n", pthread_self());
    // pthread_create(&read_thread_id, NULL, (void *)read_thread, NULL);
    // write(fd, str, 7);

    // pthread_join(read_thread_id, NULL);
    // while (1)
    // {
    calculate_crc((char *)send_buff, 7, SEND_BUFF_SIZE);

    // for (i = 0; i < 1; i++)
    // {
    write(fd, send_buff[0], SEND_BUFF_SIZE);
        // write(fd, str, 4);
    sleep(2);
    // }
    // write(fd, str, 3);
    // }

    close(fd);

    return 0;
}
