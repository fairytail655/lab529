PHONY := all
cc = arm-linux-gcc
obj = RPiTVPowerOn RPiTVShutdown ButtonControl

all: $(obj)

ButtonControl: ButtonControl.o
	$(cc) $^ -o $@ -lpthread

RPiTVPowerOn: RPiTVPowerOn.o
	$(cc) $^ -o $@

RPiTVShutdown: RPiTVShutdown.o
	$(cc) $^ -o $@

%.o: %.c
	$(cc) -c $^ -o $@

PHONY += clean
clean:
	$(RM) -rf *.o

.PHONY: PHONY