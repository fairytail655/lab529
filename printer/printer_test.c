#include <fcntl.h>
#include <stdio.h>

const char *printer_dev = "/dev/printer";

int main(int argc, char *argv[])
{
    char data;
    int printer_fd = open(printer_dev, O_RDWR);

    if (printer_fd < 0)
    {
        printf("[Beep] dev open error!\n");
        return 1;
    }

    data = 1;
    write(printer_fd, &data, 1);
    sleep(60);
    data = 0;
    write(printer_fd, &data, 1);

    close(printer_fd);

    return 0;
}
