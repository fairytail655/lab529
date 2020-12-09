PHONY := all
kernel_dir = /home/fairytail655/Desktop/linux/kernel/linux-2.6.22.6
cc = arm-linux-gcc

all: main_test
main_test: main.o http.o
	$(cc) -o $@ $^ -lpthread

%.o: %.c
	$(cc) -c $^ -o $@

PHONY += clean
clean:
	$(RM) -rf *.o

.PHONY: PHONY