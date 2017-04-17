ifneq ($(KERNELRELEASE),) 
	obj-m := cryptoEngine.o
else 

KERNELDIR ?= /lib/modules/$(shell uname -r)/build 

PWD := $(shell pwd)

MY_CFLAGS += -g -DDEBUG

default: 
	make -C $(KERNELDIR) M=$(PWD) modules  
debug:
	make -C $(KERNELDIR) M=$(PWD) modules  
	EXTRA_CFLAGS="$(MY_CFLAGS)"
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif 

