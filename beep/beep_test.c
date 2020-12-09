#include <fcntl.h>
#include <stdio.h>

const char *beep_dev = "/dev/beep";

int main(int argc, char *argv[])
{
    char data;
    int beep_fd = open(beep_dev, O_RDWR);

    if (beep_fd < 0)
    {
        printf("[Beep] dev open error!\n");
        return 1;
    }

    data = 1;
    write(beep_fd, &data, 1);
    sleep(1);
    data = 0;
    write(beep_fd, &data, 1);

    close(beep_fd);

    return 0;
}
