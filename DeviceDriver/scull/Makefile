obj-m+=scull.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	$(CC) testscull.c -o test
clean:
	make -c /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean

