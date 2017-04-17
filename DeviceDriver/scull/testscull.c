#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#define BUFFER_LENGTH 257

static char receive [BUFFER_LENGTH];

int main(){
	int ret, fd;
	char stringToSend[BUFFER_LENGTH];
	printf("Starting device test code example ...\n");
	fd=open("/dev/scullchar", O_RDWR);
	if(fd<0){
		perror("Failed to open the device");
		return errno;
	}
	printf("Type in a short string to send:\n");
	scanf("%[^\n]s", stringToSend);
	printf("Writing message [%s]  to device\n", stringToSend);
	ret=write(fd,stringToSend, strlen(stringToSend));
	if(ret<0){
		perror("Failed to write the message to the device.\n");
		return errno;
	}
	printf("Press Enter to read from the device.");
	getchar();

	printf("Reading from device.\n");
	ret=read(fd, receive, strlen(stringToSend));
	if(ret<0){
		perror("Failed to read from the device");
		return errno;
	}
	printf("Message received:%s",receive);
	return 0;
}



