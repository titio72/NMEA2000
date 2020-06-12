/*
 * Ports.h
 *
 *  Created on: Sep 29, 2018
 *      Author: aboni
 */

#ifndef PORTS_H_
#define PORTS_H_

class Port {

public:
	Port(const char* port);
	virtual ~Port();

	void listen();
	void close();

	void set_handler(int (*fun)(const char*));

	bool send(const char* msg);

	void debug(bool dbg=true) { trace = dbg; }

private:

	int open(bool output);

	void process_char(unsigned char c);

	int tty_fd = 0;

	char read_buffer[128];
	unsigned int pos = 0;

	const char* port = NULL;

	bool stop = false;

	int (*fun)(const char*);

	bool trace = false;
};

#endif /* PORTS_H_ */
