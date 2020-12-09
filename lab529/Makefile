PHONY := all
cc = arm-linux-gcc

all: lab529_test
lab529_test: main.o http.o
	$(cc) -o $@ $^ -lpthread

%.o: %.c
	$(cc) -c $^ -o $@

PHONY += clean
clean:
	$(RM) -rf *.o

.PHONY: PHONY