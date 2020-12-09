#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int fd;
    unsigned char val;
    unsigned char vals[3];

    if(argc > 3 || argc < 2)
    {
        printf("Arg's count must be in range [2, 3]!\n");
        return 1;
    }

    fd = open("/dev/leds", O_RDWR);
    if(fd < 0)
    {
        printf("Can't open /dev/leds\n");
        return 1;
    }

    if (argc == 2)
    {
        val = atoi(argv[1]);
        write(fd, &val, 1);
    }
    else
    {
        vals[0] = atoi(argv[1]);
        vals[1] = atoi(argv[2]);
        // printf("%d, %d\n", vals[0], vals[1]);
        write(fd, vals, 2);
    }
    

    return 0;
}
