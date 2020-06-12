/*
 * Ports.cpp
 *
 *  Created on: Sep 29, 2018
 *      Author: aboni
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include "Ports.h"

#define XTRACE

Port::Port(const char* port_name) {
	port = strdup(port_name);
	fun = NULL;
}

Port::~Port() {
	delete port;
}

void Port::set_handler(int (*fun)(const char*)) {
	Port::fun = fun;
}

void Port::process_char(unsigned char c) {
	if (c!=10 && c!=13) {
		read_buffer[pos] = c;
		pos++;
		pos %= 1024; // avoid buffer overrun
	} else if (pos!=0) {
		static char sentence_copy[128];
		strcpy(sentence_copy, (const char*)read_buffer);
		if (fun) {
			(*fun)(sentence_copy);
		}
		#ifdef TRACE
		printf("Read {%s}\n", sentence_copy);
		#endif
		pos = 0;
	}
	read_buffer[pos] = 0;
}

int fd_set_blocking(int fd, int blocking) {
    /* Save the current flags */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return 0;

    if (blocking)
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags) != -1;
}

void Port::close() {
	stop = true;
}

int Port::open(bool output) {
    struct termios tio;

    memset(&tio,0,sizeof(tio));
    tio.c_iflag=0;
    tio.c_oflag=0;
    tio.c_cflag=CS8|CREAD|CLOCAL;           // 8n1, see termios.h for more information
    tio.c_lflag=0;
    tio.c_cc[VMIN]=1;
    tio.c_cc[VTIME]=5;

    #ifdef TRACE
	printf("Opening port {%s} Output {%d}\n", port, output);
	#endif

	tty_fd = ::open(port, (output?O_WRONLY:O_RDONLY) | O_NONBLOCK);        // O_NONBLOCK might override VMIN and VTIME, so read() may return immediately.
	if (tty_fd>0) {
		fd_set_blocking(tty_fd, 1);
		cfsetospeed(&tio,B9600);
		cfsetispeed(&tio,B9600);
		tcsetattr(tty_fd,TCSANOW,&tio);
	}

	#ifdef TRACE
	printf("Err opening port {%s} {%d} {%s}\n", port, errno, strerror(errno));
	#endif

	return tty_fd!=0;
}

void Port::listen() {

    unsigned char c;

	while (!stop) {
		if (open(false)) {
			while (!stop) {
				ssize_t bread = read(tty_fd, &c, 1);
				if (bread>0) {
					process_char(c);
				} else {
					#ifdef TRACE
					printf("Err reading port {%s} {%d} {%s}\n", port, errno, strerror(errno));
					#endif
					sleep(2);
					break;
				}
			}

			::close(tty_fd);
		} else {
			if (errno==2){
				sleep(2);
			} else {
				stop = 1;
			}
		}
	}
}

bool Port::send(const char* sentence) {
	static char tempbuf[128];
	sprintf(tempbuf, "%s\r\n", sentence);

	if (tty_fd<=0) {
		#ifdef TRACE
		printf("Opening port {%s}\n", port);
		#endif
		open(true);
		#ifdef TRACE
		printf("Port opened {%s}\n", (tty_fd>0)?"OK":"KO");
		#endif
	}

	if (tty_fd>0) {
		#ifdef TRACE
		printf("Sending {%s} to {%s}\n", sentence, port);
		#endif
		if (write(tty_fd, tempbuf, strlen(tempbuf)) <= 0) {
			#ifdef TRACE
			printf("Error sending message {%d} {%s}\n", errno, strerror(errno));
			#endif
			::close(tty_fd);
			tty_fd = 0;
			return false;
		} else {
			return true;
		}
	} else {
		return false;
	}
}
