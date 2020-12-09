/*
 * @Description: -
 * @Version: -
 * @Author: Fox_benjiaming
 * @Date: 2020-10-21 01:06:55
 * @LastEditors: Fox_benjiaming
 * @LastEditTime: 2020-12-09 03:18:37
 */
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

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

int main(int argc, char *argv[])
{
    struct pollfd fds;
    int i = 0;
    int result = 0;

    fd = open("/dev/tv_uart", O_RDWR);
    if (fd < 0) 
    {
        printf("can't open!\n");
        return 0;
    }

    fds.fd = fd;
    fds.events = POLLIN;

    // printf("I am main:%d\n", pthread_self());
    // pthread_create(&read_thread_id, NULL, (void *)read_thread, NULL);
    // write(fd, str, 7);

    // pthread_join(read_thread_id, NULL);
    while (1)
    {
        calculate_crc((char *)send_buff, 7, SEND_BUFF_SIZE);
        // for (i = 0; i < 1; i++)
        // {
        write(fd, send_buff[0], SEND_BUFF_SIZE);
        result = poll(&fds, 1, 2000);
        if (result == 0)
        {
            printf("timeout\n");
        }
        else
        {
            printf("ok\n");
            read(fd, NULL, 0);
        }
        sleep(2);
    // }
    // write(fd, str, 3);
    }

    close(fd);

    return 0;
}
