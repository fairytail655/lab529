#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include "http.h"

// 刷卡机返回数据存储空间大小
#define READ_BUFFER_SIZE 32
// http 返回数据存储空间大小
#define HTTP_BUFFER_SIZE 1024

// 日志文件路径
const char *log_file = "/var/log/lab529.log";
// 日志文件句柄
int log_fd;

// 刷卡机设备路径
const char *card_uart_dev = "/dev/card_uart";
// 刷卡机串口设备文件句柄
int card_uart_fd;
// 刷卡机读取前需要发送的请求数据
const unsigned char card_write_data[7] = {0x03, 0xff, 0x07, 0x04, 0x01, 0x00, 0xfe};
// 刷卡机串口数据缓冲区
unsigned char card_read_buffer[READ_BUFFER_SIZE];
unsigned int card_read_count;
// 发送刷卡机卡号的 url
char card_request_data[512];
const char *card_request_start = "http://47.98.115.156:1010/Prog/Attendance/AddCardRecord?card=";
const char *card_request_end = "&pas=123456&info=devID";
const char *card_response = "Info:OK!";

// 请求打印机时间的 url
const char *printer_request = "http://47.98.115.156:1010/Prog/File/GetPrintTime";
const char *printer_response = "Time:";

// leds 设备路径
const char *leds_dev = "/dev/leds";
// leds 设备文件句柄
int leds_fd;

// 刷卡机数据读取线程
void card_read_thread(void)
{
    int i;
    // 控制 leds 状态的数据
    // [0]: 对哪个 led 进行操作, 0,1,2
    // [1]: 执行的操作, 1-开启, 0-关闭
    unsigned char leds_data[2];
    // 字符串型的卡号
    char card_short[3];
    // http 返回数据存储区指针(动态分配内存)
    char *http_buffer;

    memset(card_read_buffer, 0, sizeof(card_read_buffer));

    while (1)
    {
        // 读取到正确格式的数据后从休眠中唤醒
        card_read_count = read(card_uart_fd, card_read_buffer, READ_BUFFER_SIZE);
        // 点亮 led1
        leds_data[0] = 1;
        leds_data[1] = 1;
        write(leds_fd, leds_data, 2);
        // 提取出上传卡号
        memset(card_request_data, 0, sizeof(card_request_data));
        strcat(card_request_data, card_request_start);
        for (i = 0; i < 4; i++)
        {
            sprintf(card_short, "%02x", card_read_buffer[7+i]);
            strcat(card_request_data, (const char *)card_short);
        }
        strcat(card_request_data, card_request_end); 
        // printf("%s\n", card_request_data);
        // 发送卡号给服务器
        http_buffer = (char *)malloc(HTTP_BUFFER_SIZE);
        http_buffer = http_get(card_request_data);
        // if (http_buffer)
        // {
        if (strstr((const char *)http_buffer, card_response))
        {
            http_buffer = (char *)malloc(HTTP_BUFFER_SIZE);
            http_buffer = http_get(printer_request);
            if (http_buffer)
                printf(http_buffer);
        }
        else
        {
            printf("Card http error!\n");
            sleep(1);
        }
        
        // }
        // 熄灭 led1
        leds_data[0] = 1;
        leds_data[1] = 0;
        write(leds_fd, leds_data, 2);
    }
}

// 刷卡机数据发送线程
void card_write_thread(void)
{
    while (1)
    {
        // 向刷卡机发送读取请求指令
        write(card_uart_fd, card_write_data, 7);
        // sleep(0.5);
        // 延时 500ms
        usleep(500000);
    }
}

int main(int argc, char *argv[])
{
    int i;
    pthread_t card_read_id, card_write_id;

    log_fd = open(log_file, O_RDWR);
    if (log_fd < 0)
    {
        printf("log file open error!\n");
        return 1;
    }
    card_uart_fd = open(card_uart_dev, O_RDWR);
    if (card_uart_fd < 0)
    {
        printf("Card uart open error!\n");
        return 1;
    }
    leds_fd = open(leds_dev, O_RDWR);
    if (leds_fd < 0)
    {
        printf("Card uart open error!\n");
        return 1;
    }

    pthread_create(&card_read_id, NULL, (void *)card_read_thread, NULL);
    pthread_create(&card_write_id, NULL, (void *)card_write_thread, NULL);

    while (1)
    {
        sleep(1);
    }

    close(log_fd);
    close(card_uart_fd);
    close(leds_fd);

    return 0;
}
