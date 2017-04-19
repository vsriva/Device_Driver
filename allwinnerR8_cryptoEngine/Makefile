ifneq ($(KERNELRELEASE),)
	obj-m := crypto_engine.o crypto_encrypt.o crypto_decrypt.o
else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

MY_CFLAGS += -g -DDEBUG

default:
	make -C $(KERNELDIR) M=$(PWD) modules
	$(CC) testcrypto.c -o testcrypto.o
debug:
	make -C $(KERNELDIR) M=$(PWD) modules
	EXTRA_CFLAGS="$(MY_CFLAGS)"
	$(CC) testcrypto.c -o testcrypto.o
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
endif 

