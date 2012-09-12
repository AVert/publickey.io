#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>


#include "bytes.c"
#include "p2p.c"


int indata(struct sockaddr_in6 addr, bytes data, void * additional) {
	debug("Incoming email.")
	struct p2p_st * p2p = additional;

	bprint(data);	

	p2psend(p2p, data);
	return 0; }

struct termdata { 
	int pingfd; 
	struct peertreenode ** node; };



int interm(int epoll, struct objdata * obj) { 
	size_t len = 4096;
	char buf[len];
	debug("Received data from terminal!");
	ssize_t r = read(0, buf, len);

	struct termdata * td = (void*) (obj + 1);
	struct peertreenode ** n = td->node;

	bytes re = { buf, r };

	bfound f = bfind(re, Bs("list"));
	if(f.found.length > 0) {
		debug("List command recevied");
		peertreepreorder(n, &p2pprint, 0); }

	f = bfind(re, Bs("ping "));
	if(f.found.length > 0) {
		debug("Ping command received");

		f = bfind(f.after, B(' '));
		struct sockaddr_in6 addr;
		memset(&addr, 0, sizeof(addr));
		f.found.as_char[0] = 0;
		inet_pton(AF_INET6, f.before.as_void, &addr.sin6_addr);
		f = bfind(f.after, B('\n'));
		f.found.as_char[0] = 0;
		addr.sin6_port = htons(atoi(f.before.as_char));
		addr.sin6_family = AF_INET6;
		p2ppingpeer(&addr, td->pingfd);
		
	}
	f = bfind(re, Bs("logging"));
	if(f.found.length > 0) {
		debug("Flipping logging bit!");
		logging = !logging; }
	
	return 0; }


int main(int argc, char ** argv) {

	int epoll = epoll_create(1);
	uint16_t port = 8080;


	struct p2p_st p2p;
	p2pprepare(epoll, port, &indata, &p2p, &p2p);

	// Interactive mode support.
	if(argc == 2 && strcmp(argv[0], "-i")) {
		printf("Interactive mode. Commands: list, add, remove, ping.\n");


		struct objdata * data = malloc(sizeof(struct objdata)
			+ sizeof(struct termdata));

		*data = (struct objdata) { 0, &interm, &err, &hup, &kill };
		struct termdata * td = (void*) (data + 1);
		*td = (struct termdata) { p2p.socket, &p2p.peer };


		epoll_add(epoll, 0, data);



	}

	eventloop(epoll);
	return 0; }

