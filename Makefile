KERN_DIR=/home/fairytail655/Desktop/linux/kernel/linux-2.6.22.6
BIN_DIR=./bin/
DRIVER_DIR=./driver

bin_obj=main http
driver_obj=leds_drv card_uart_drv

obj-m += $(patsubst %, %.o, $(driver_obj))

all: drv main

.PHONY: drv
drv: 
	make -C $(KERN_DIR) M=`pwd` modules
	mv $(patsubst %, %.ko, $(driver_obj)) $(DRIVER_DIR)

main: $(addsuffix .o, $(bin_obj))
	arm-linux-gcc -o $(BIN_DIR)/$@ $^ -lpthread

%.o: %.c
	arm-linux-gcc -c $< 

.PHONY: clean
clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	$(RM) -rf modules.order
	$(RM) -rf $(addprefix $(BIN_DIR), $(bin_obj))
