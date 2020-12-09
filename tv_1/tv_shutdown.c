#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <poll.h>

// 保存标志位的文件地址
#define FLAG_FILE_PATH  "/root/tv/flag.txt"
// 保存日志的文件地址
#define LOG_FILE_PATH   "/root/tv/log.txt"
// 发送指令实际长度
#define SEND_BUFF_SIZE  11
// 重复发送次数
#define SEND_REPEAT     3
// 接收延时
#define REC_DELAY       1000

unsigned char send_buff[SEND_BUFF_SIZE] = {0x7e, 0x07, 0x00, 0x59, 0x39, 0x59, 0x39, 0x14, 0x00, 0};

struct pollfd fds;
int fd;

static char getFileFlag(const char *file_path);
static char sendShutdownCommand(void);
static void setFileLog(const char *file_path, const char *ch);
static void setFileFlag(const char *file_path, char ch);

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
        crc = 0;
    }
}

int main(void)
{
    fd = open("/dev/tv_uart", O_RDWR);
    if(fd < 0) 
    {
        setFileLog(LOG_FILE_PATH, "[Tv shutdown] error: can't open /dev/tv_uart\n");
        return -1;
    }

    fds.fd = fd;
    fds.events = POLLIN;

    calculate_crc(send_buff, 1, SEND_BUFF_SIZE);

    if(getFileFlag(FLAG_FILE_PATH) != 'C')
    {
        if(sendShutdownCommand() == 0)
        {
            setFileLog(LOG_FILE_PATH, "[Tv shutdown] shutdown finished!\n");
            //设置文件标志位为'C'
            setFileFlag(FLAG_FILE_PATH, 'C');    
        }
        else
        {
            setFileLog(LOG_FILE_PATH, "[Tv shutdown] error: disconnect with device\n");
        }
        setFileLog(LOG_FILE_PATH, "----------------------------------\n");
    }

    close(fd);

    return 0;
}

// 发送液晶屏开启指令
static char sendShutdownCommand(void)
{
    int repeat_count = 0;

    write(fd, send_buff, SEND_BUFF_SIZE);
    while (poll(&fds, 1, REC_DELAY) == 0 && repeat_count < SEND_REPEAT)
    {
        setFileLog(LOG_FILE_PATH, "[Tv shutdown] warning: repeat send...\n");
        write(fd, send_buff, SEND_BUFF_SIZE);
        repeat_count++;
    }
    if (repeat_count == SEND_REPEAT)
    {
        return -1;
    }
    read(fd, NULL, 0);

    return 0;
}

//获取指定文件的第一个字符
char getFileFlag(const char *file_path)
{
    char ch;
    FILE *pFile = fopen(file_path, "r");

    if (pFile == NULL)
    {
        printf("[Tv shutdown] error: can't open %s\n", file_path);
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
        printf("[Tv shutdown] error: can't open %s\n", file_path);
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
        printf("[Tv shutdown] error: can't open %s\n", file_path);
        return;
    }
    fputs(ch, pFile);
    fclose(pFile);
}
