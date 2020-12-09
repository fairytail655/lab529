#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <pthread.h>
#include <poll.h>

// 保存标志位的文件地址
#define FLAG_FILE_PATH  "/root/tv/flag.txt"
// 保存日志的文件地址
#define LOG_FILE_PATH   "/root/tv/log.txt"
// 保存动作顺序的文件地址
#define ORDER_FILE      "/root/tv/order.txt"
// 发送指令实际长度
#define SEND_BUFF_SIZE  11
// 最大动作数
#define ACT_COUNT       50 
// 重复发送次数
#define SEND_REPEAT     3
// 接收延时
#define REC_DELAY       1000
// 设备路径
#define UART_DEVICE     "/dev/tv_uart"
#define BUTTON_DEVICE   "/dev/tv_control"

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

// 液晶屏设备句柄
int uart_fd, button_fd;
char button_flag = 0;
struct pollfd fds;

static char getFileFlag(const char *file_path);
static char sendShutdownCommand(void);
static void setFileLog(const char *file_path, const char *ch);
static void setFileFlag(const char *file_path, char ch);
static void getFileOrder(char *str, int len,  const char *file_path);
static char sendPowerONCommand(void);

void calculate_crc(char *data, int x_len, int y_len)
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
        crc = 0;
    }
}

void button_check_thread(void)
{
    while (1)
    {
        if (button_flag == 0)
        {
            read(button_fd, NULL, 1);
            button_flag = 1;
        }
        sleep(2);
    }
}

int main(void)
{
    pthread_t button_thread_id;

    button_fd = open(BUTTON_DEVICE, O_RDWR);
    if(button_fd < 0) 
    {
        setFileLog(LOG_FILE_PATH, "[Button control] error: can't open button device\n");
        return 1;
    }
    uart_fd = open(UART_DEVICE, O_RDWR);
    if(uart_fd < 0) 
    {
        setFileLog(LOG_FILE_PATH, "[Button control] error: can't open uart device\n");
        return 1;
    }

    fds.fd = uart_fd;
    fds.events = POLLIN;

    calculate_crc((char *)send_buff, 7, SEND_BUFF_SIZE);
    pthread_create(&button_thread_id, NULL, (void *)button_check_thread, NULL);

    while(1)
    {
        if (button_flag == 1)
        {
            //文件标志位为'O'(Open)
            if(getFileFlag(FLAG_FILE_PATH) == 'O')
            {
                if(sendShutdownCommand() == 0)
                {
                    setFileLog(LOG_FILE_PATH, "[Button control] shutdown finished!\n");
                    //设置文件标志位为'C'(Close)
                    setFileFlag(FLAG_FILE_PATH, 'C');    
                }
                else
                {
                    setFileLog(LOG_FILE_PATH, "[Button control] error: disconnect with device\n");
                }
            }
            //文件标志位为'C'(Close)
            else if(getFileFlag(FLAG_FILE_PATH) == 'C')
            {
                if(sendPowerONCommand() == 0)
                {
                    setFileLog(LOG_FILE_PATH, "[Button control] power on finished!\n");
                    //设置文件标志位为'O'
                    setFileFlag(FLAG_FILE_PATH, 'O');    
                }
                else
                {
                    setFileLog(LOG_FILE_PATH, "[Button control] error: disconnect with device\n");
                }
            }
            else
            {
                setFileLog(LOG_FILE_PATH, "[Button control] error: invalid flag file\n");
            }
            button_flag = 0;
            setFileLog(LOG_FILE_PATH, "----------------------------------\n");
        }
        sleep(2);
    }

    close(button_fd);
    close(uart_fd);

    return 0;
}

// 发送液晶屏开启指令
static char sendShutdownCommand(void)
{
    int repeat_count = 0;

    write(uart_fd, send_buff[0], SEND_BUFF_SIZE);
    while (poll(&fds, 1, REC_DELAY) == 0 && repeat_count < SEND_REPEAT)
    {
        setFileLog(LOG_FILE_PATH, "[Button control] warning: repeat send...\n");
        write(uart_fd, send_buff[0], SEND_BUFF_SIZE);
        repeat_count++;
    }
    if (repeat_count == SEND_REPEAT)
    {
        return -1;
    }
    read(uart_fd, NULL, 0);

    return 0;
}

// 发送液晶屏开启指令
static char sendPowerONCommand(void)
{
    char order_data[ACT_COUNT];
    int i = 0;
    int repeat_count = 0;

    getFileOrder(order_data, ACT_COUNT, ORDER_FILE);
    for(i = 0; i < ACT_COUNT; i++)
    {
        if(order_data[i] >= '9' || order_data[i] <= '0')
        {
            order_data[i] = 0;
            break;
        }
    }

    write(uart_fd, send_buff[0], SEND_BUFF_SIZE);
    while (poll(&fds, 1, REC_DELAY) == 0 && repeat_count < SEND_REPEAT)
    {
        setFileLog(LOG_FILE_PATH, "[Button control] warning: repeat send...\n");
        write(uart_fd, send_buff[0], SEND_BUFF_SIZE);
        repeat_count++;
    }
    if (repeat_count == SEND_REPEAT)
    {
        return -1;
    }
    read(uart_fd, NULL, 0);
    repeat_count = 0;
    sleep(20);

    i = 0; 
    while(order_data[i] != 0)
    {
        write(uart_fd, send_buff[order_data[i]-'0'], SEND_BUFF_SIZE);
        while (poll(&fds, 1, REC_DELAY) == 0 && repeat_count < SEND_REPEAT)
        {
            setFileLog(LOG_FILE_PATH, "[Button control] warning: repeat send...\n");
            write(uart_fd, send_buff[order_data[i]-'0'], SEND_BUFF_SIZE);
            repeat_count++;
        }
        if (repeat_count == SEND_REPEAT)
        {
            return -1;
        }
        read(uart_fd, NULL, 0);
        repeat_count = 0;
        sleep(3);
        setFileLog(LOG_FILE_PATH, "[Button control] command send finished!\n");
        i++;
    }

    return 0;
}

//获取指定文件的第一个字符
char getFileFlag(const char *file_path)
{
    char ch;
    FILE *pFile = fopen(file_path, "r");

    if (pFile == NULL)
    {
        printf("[Button control] error: can't open %s\n", file_path);
        return 0;
    }
    ch = fgetc(pFile);
    fclose(pFile);
    
    return ch;
}

//写入指定文件一个字符
void setFileFlag(const char *file_path, char ch)
{
    FILE *pFile = fopen(file_path, "w");
        
    if (pFile == NULL)
    {
        printf("[Button control] error: can't open %s\n", file_path);
        return;
    }
    fputc(ch, pFile);
    fclose(pFile);
}

//写入指定文件一串字符
void setFileLog(const char *file_path, const char *ch)
{
    FILE *pFile = fopen(file_path, "a");
        
    if (pFile == NULL)
    {
        printf("[Button control] error: can't open %s\n", file_path);
        return;
    }
    fputs(ch, pFile);
    fclose(pFile);
}

//读取指定文件一串字符
void getFileOrder(char *str, int len,  const char *file_path)
{
    FILE *pFile = fopen(file_path, "r");

    if (pFile == NULL)
    {
        printf("[Button control] error: can't open %s\n", file_path);
        return;
    }
    fgets(str, len, pFile);
    fclose(pFile);
}
