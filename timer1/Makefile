PHONY := all
kernel_dir = /home/fairytail655/Desktop/linux/kernel/linux-2.6.22.6
cc = arm-linux-gcc

my_obj := timer1
my_drv := $(my_obj:%=%_drv)
my_bin := $(my_obj:%=%_test)

obj-m += $(my_drv:%=%.o)

all: drv bin

PHONY += drv
# drv: $(addsuffix .c, $(my_drv))
drv: 
	make -C $(kernel_dir) M=`pwd` modules

PHONY += bin
bin: $(my_bin)
$(my_bin): $(my_bin:%=%.c)
	$(cc) -o $@ $^ -lpthread

PHONY += clean
clean:
	make -C $(kernel_dir) M=`pwd` modules clean
	$(RM) -rf modules.order
	$(RM) -rf *.o

.PHONY: PHONY