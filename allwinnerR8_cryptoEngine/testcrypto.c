/*
 *Copyright (c) 2017 Vishal SRIVASTAVA <vsriva10@asu.edu>
 *
 *Permission is hereby granted, free of charge, to any person obtaining a copy
 *of this software and associated documentation files (the "Software"), to deal
 *in the Software without restriction, including without limitation the rights 
 *to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *copies of the Software, and to permit persons to whom the Software is
 *furnished to do so, subject to the following conditions:
 *
 *The above copyright notice and this permission notice shall be included in all
 *copies or substantial portions of the Software.
 *
 *THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_LENGTH 257

static char receive[BUFFER_LENGTH];

int main(void)
{
	int ret, fd1, fd2;
	char string_to_send[BUFFER_LENGTH];

	printf("Starting device test code example ...\n");
	fd1 = open("/dev/crypto_encrypt", O_RDWR);
	fd2 = open("/dev/crypto_decrypt", O_RDWR);
	if ((fd1 < 0) || (fd2 < 0)) {
		perror("Failed to open the device");
		return errno;
	}
	printf("Type in a short string to send:\n");
	scanf("%[^\n]s", string_to_send);
	printf("Writing message [%s] to encrypt device\n", string_to_send);
	ret = write(fd1, string_to_send, strlen(string_to_send));
	if (ret < 0) {
		perror("Failed to write the message to the encrypt device.\n");
		return errno;
	}

	printf("Reading from encrypt device.\n");
	ret = read(fd1, receive, strlen(string_to_send));
	if (ret < 0) {
		perror("Failed to read from the encrypt device");
		return errno;
	}
	printf("Message received from encrypt:%s\n", receive);
	printf("Press a key to continue...\n");
	getchar();

	strcpy(string_to_send, receive);
	printf("Writing message [%s]  to decrypt device\n", string_to_send);
	ret = write(fd2, string_to_send, strlen(string_to_send));
	if (ret < 0) {
		perror("Failed to write the message to the decrypt device.\n");
		return errno;
	}

	printf("Reading from decrypt device.\n");
	ret = read(fd2, receive, strlen(string_to_send));
	if (ret < 0) {
		perror("Failed to read from the decrypt device");
		return errno;
	}
	printf("Message received from decrypt:%s\n", receive);
	return 0;
}
