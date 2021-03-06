#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>

// 保存标志位的文件地址
#define FLAG_FILE_PATH "/root/tv/flag.txt"
// 保存日志的文件地址
#define LOG_FILE_PATH "/root/tv/log.txt"
// 保存动作顺序的文件地址
#define ORDER_FILE "/root/tv/order.txt"
// 发送指令实际长度
#define SEND_BUFF_SIZE 11
// 最大动作数
#define ACT_COUNT 50 
// 重复发送次数
#define SEND_REPEAT 3

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
int fd;

char getFileFlag(const char *file_path);
char sendShutdownCommand(void);
void setFileLog(const char *file_path, const char *ch);
void setFileFlag(const char *file_path, char ch);
void getFileOrder(char *str, int len,  const char *file_path);

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

int main(void)
{

    fd = open("/dev/tv_control", O_RDWR);
    if(fd < 0) 
    {
        setFileLog(LOG_FILE_PATH, "[RPiTVShutdown] error: can't open /dev/tv_control\n");
        return 1;
    }

    calculate_crc((char *)send_buff, 7, SEND_BUFF_SIZE);

    if(getFileFlag(FLAG_FILE_PATH) != 'C')
    {
        if(sendShutdownCommand() == 0)
        {
            //设置文件标志位为'C'
            setFileFlag(FLAG_FILE_PATH, 'C');    
        }
    }

    close(fd);

    return 0;
}

// 发送液晶屏开启指令
char sendShutdownCommand(void)
{
    int j;

    // for(j = 0;j < SEND_REPEAT;j++)
    write(fd, send_buff[0], SEND_BUFF_SIZE);
    setFileLog(LOG_FILE_PATH, "[RPiTVShutdown] shutdown finished!\n");

    return 0;
}

//获取指定文件的第一个字符
char getFileFlag(const char *file_path)
{
    char ch;
    FILE *pFile = fopen(file_path, "r");

    if (pFile == NULL)
    {
        printf("[RPiTVShutdown] error: can't open %s\n", file_path);
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
        printf("[RPiTVShutdown] error: can't open %s\n", file_path);
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
        printf("[RPiTVShutdown] error: can't open %s\n", file_path);
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
        printf("[RPiTVShutdown] error: can't open %s\n", file_path);
        return;
    }
    fgets(str, len, pFile);
    fclose(pFile);
}
