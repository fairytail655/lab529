#include <sys/time.h>
#include <string.h>
#include <signal.h>
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

// 日志文件
const char *log_file = "/var/log/lab529.log";
FILE *log_fd;

// 刷卡机设备
const char *card_uart_dev = "/dev/card_uart";
int card_uart_fd;
// 刷卡机读取前需要发送的请求数据
const unsigned char card_write_data[7] = {0x03, 0xff, 0x07, 0x04, 0x01, 0x00, 0xfe};
// 刷卡机串口数据缓冲区
unsigned char card_read_buffer[READ_BUFFER_SIZE];
// 发送刷卡机卡号的 url
char card_request_data[512];
const char *card_request_start = "http://47.98.115.156:1010/Prog/Attendance/AddCardRecord?card=";
const char *card_request_end = "&pas=123456&info=devID";
const char *card_response = "Info:OK!";

// 刷卡机设备
const char *printer_dev = "/dev/printer";
int printer_fd;
// 请求打印机时间的 url
const char *printer_request = "http://47.98.115.156:1010/Prog/File/GetPrintTime";
const char *printer_response = "Time:";
// 获取到的打印机时间
int printer_time;

// leds 设备
const char *leds_dev = "/dev/leds";
int leds_fd;

// beep 设备
const char *beep_dev = "/dev/beep";
int beep_fd;

// timer0 设备
const char *timer0_dev = "/dev/timer0";
int timer0_fd;

// timer1 设备
const char *timer1_dev = "/dev/timer1";
int timer1_fd;
int timer1_sec;

// 控制 leds 亮灭
// sel: 对哪个 led 进行操作, 0,1,2
// status: 状态, 1-开启, 0-关闭
static inline void leds_control(unsigned char sel, unsigned char status)
{
    unsigned char leds_data[2];

    leds_data[0] = sel;
    leds_data[1] = status;
    write(leds_fd, leds_data, 2);
}

// 控制 beep 的开关
// status: 状态, 1-开启, 0-关闭
static inline void beep_control(unsigned char status)
{
    write(beep_fd, &status, 1);
}

// 控制 printer 的开关
// status: 状态, 1-开启, 0-关闭
static inline void printer_control(unsigned char status)
{
    write(printer_fd, &status, 1);
}

// 打印机开启并计时准备关闭进程
static void printer_thread(void)
{
    int timer1_cnt;

    // 开启打印机和 led0
    leds_control(0, 1);
    printer_control(1);

    printf("[Printer] thread start!\n");

    while (1)
    {
        // 开启 timer1 ，并设置中断周期为 1s(25000 * 40us)
        timer1_cnt = 25000;
        write(timer1_fd, &timer1_cnt, 2);
        // 等待 timer1 中断
        read(timer1_fd, NULL, 0);
        // printf("timer_sec: %d\n", timer1_sec);
        if (timer1_sec < printer_time*60)
            timer1_sec++;
        else
        {
            timer1_sec = 0;
            printer_time = 0;
            // 关闭打印机和 led0
            leds_control(0, 0);
            printer_control(0);
            printf("[Printer] thread exit!\n");
            return;
        }
    }
}

// 网络请求线程
static void url_get_thread(void)
{
    // http 返回数据存储区指针(动态分配内存)
    char *http_buffer;
    char *time_str;
    int ret;
    int time_num;
    pthread_t printer_thread_id;

    printf("[Url get] thread start!\n");

    // 发送包含卡号的 url 给服务器
    http_buffer = http_get(card_request_data);
    // 如果发送成功且返回数据中包含指定字符串
    if ((http_buffer != NULL) && strstr((const char *)http_buffer, card_response))
    {
        // 释放 malloc 申请的返回数据内存, 详情见 http.c
        free(http_buffer);
        // 发送获取打印机开启时间的 url
        http_buffer = http_get(printer_request);
        // 如果发送成功
        if (http_buffer != NULL)
        {
            // 获取返回数据中指定字符串首次出现位置的指针
            time_str = strstr((const char *)http_buffer, printer_response);
            // 如果找到指定字符串
            if (time_str != NULL)
            {
                // printf("%s\n", time_str);
                // 将指针指向返回数据中指定字符串的下一位，获取打印机开启时间（字符串型）
                time_str +=  strlen(printer_response);
                // 获取打印机开启时间（数字型）
                time_num = atoi((const char *)time_str);
                if (time_num > 0)
                {
                    // 更新全局变量: 打印机开启时间, 当前 timer1 累计的秒数
                    printer_time = time_num;
                    timer1_sec = 0;
                    // 查看线程是否已存在
                    ret = pthread_kill(printer_thread_id, 0);
                    // 如果线程不存在
                    if (ret != 0)
                    {
                        ret = pthread_create(&printer_thread_id, NULL, (void *)printer_thread, NULL);
                        if (ret != 0)
                        {
                            printf("[Printer thread] create error!\n");
                            // return;
                        }
                    }
                }
                printf("%d\n", time_num);
            }
            // 释放 malloc 申请的返回数据内存, 详情见 http.c
            free(http_buffer);
        }
        else
        {
            printf("[Http] printer time get error!\n");
            // return;
        }
    }
    else
    {
        printf("[Http] card send error!\n");
    }
    // 熄灭 led1
    leds_control(1, 0);
    printf("[Url get] thread exit!\n");
}

// 关闭 beep 的线程
static void beep_off_thread(void)
{
    while (1)
    {
        // 等待 timer0 定时中断
        read(timer0_fd, NULL, 0);
        // 关闭 beep
        beep_control(0);
    }
}

// 刷卡机数据读取线程
static void card_read_thread(void)
{
    int i;
    int ret;
    unsigned int timer0_cnt;
    char card_short[3];
    pthread_t url_get_id, beep_off_id;

    ret = pthread_create(&beep_off_id, NULL, (void *)beep_off_thread, NULL);
    if (ret != 0)
    {
        printf("[Url get thread] create error!\n");
        return;
    }

    while (1)
    {
        // 读取到正确格式的数据后从休眠中唤醒
        read(card_uart_fd, card_read_buffer, READ_BUFFER_SIZE);
        // 开启 led1 和 beep
        leds_control(1, 1);
        beep_control(1);
        // 开启 timer0, 设置定时 200ms 后 beep 关闭
        timer0_cnt = 5000;
        write(timer0_fd, &timer0_cnt, 2);
        // 提取出卡号, 将其进行拼接, 形成上传卡号的 url
        memset(card_request_data, 0, sizeof(card_request_data));
        strcat(card_request_data, card_request_start);
        for (i = 0; i < 4; i++)
        {
            sprintf(card_short, "%02x", card_read_buffer[7+i]);
            strcat(card_request_data, (const char *)card_short);
        }
        strcat(card_request_data, card_request_end); 

        // 查看线程是否存在
        ret = pthread_kill(url_get_id, 0);
        // 线程不存在
        if (ret != 0)
        {
            ret = pthread_create(&url_get_id, NULL, (void *)url_get_thread, NULL);
            if (ret != 0)
            {
                printf("[Url get thread] create error!\n");
                return;
            }
        }
    }
}

// 刷卡机数据发送线程
static void card_write_thread(void)
{
    while (1)
    {
        // 向刷卡机发送读取请求指令
        write(card_uart_fd, card_write_data, 7);
        // 延时 500ms
        usleep(500000);
    }
}

// 初始化相关的设备和文件
// return: 0-成功, 1-失败
static unsigned char device_init(void)
{
    log_fd = fopen(log_file, "w");
    if (log_fd == NULL)
    {
        printf("[%s] open error!\n", log_file);
        return 1;
    }
    card_uart_fd = open(card_uart_dev, O_RDWR);
    if (card_uart_fd < 0)
    {
        printf("[Card uart] open error!\n");
        return 1;
    }
    leds_fd = open(leds_dev, O_RDWR);
    if (leds_fd < 0)
    {
        printf("[Leds] open error!\n");
        return 1;
    }
    beep_fd = open(beep_dev, O_RDWR);
    if (beep_fd < 0)
    {
        printf("[Beep] open error!\n");
        return 1;
    }
    timer0_fd = open(timer0_dev, O_RDWR);
    if (timer0_fd < 0)
    {
        printf("[Timer0] open error!\n");
        return 1;
    }
    timer1_fd = open(timer1_dev, O_RDWR);
    if (timer1_fd < 0)
    {
        printf("[Timer1] open error!\n");
        return 1;
    }
    printer_fd = open(printer_dev, O_RDWR);
    if (printer_fd < 0)
    {
        printf("[Printer] open error!\n");
        return 1;
    }

    leds_control(0, 0);
    leds_control(1, 0);
    leds_control(2, 0);
    beep_control(0);
    printer_control(0);

    return 0;
}

static void device_close(void)
{
    leds_control(0, 0);
    leds_control(1, 0);
    leds_control(2, 0);
    beep_control(0);
    printer_control(0);

    fclose(log_fd);
    close(card_uart_fd);
    close(leds_fd);
    close(beep_fd);
    close(printer_fd);
}

int main(int argc, char *argv[])
{
    int i;
    int ret;
    pthread_t card_read_id, card_write_id, timer;

    if (device_init() == 1)
        return 1;

    // 创建线程
    ret = pthread_create(&card_read_id, NULL, (void *)card_read_thread, NULL);
    if (ret != 0)
    {
        printf("[card read thread] create error!\n");
        return 1;
    }
    ret = pthread_create(&card_write_id, NULL, (void *)card_write_thread, NULL);
    if (ret != 0)
    {
        printf("[card write thread] create error!\n");
        return 1;
    }

    while (1)
    {
        // sleep(1);
        // 延时 500ms
        usleep(500000);
        leds_control(2, 1);
        // 延时 500ms
        usleep(500000);
        leds_control(2, 0);
        // // 向刷卡机发送读取请求指令
        // write(card_uart_fd, card_write_data, 7);
    }

    device_close();

    return 0;
}
