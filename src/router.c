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

	debug("End of email.")
	p2psend(p2p, data, p2pendpoint);
	return 0; }


int interm(int epoll, struct objdata * obj) { 
	size_t len = 4096;
	char buf[len];
	debug("Received data from terminal!");
	ssize_t r = read(0, buf, len);

	
	struct p2p_st * p2p = *((struct p2p_st**)(obj+1));

	bytes re = { buf, r };

	bfound f = bfind(re, Bs("list"));
	if(f.found.length > 0) {
		debug("List command recevied");
		peertreepreorder(&p2p->peer, &p2pprint, 0); }

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
		p2ppingpeer(p2p, &addr);
		
	}
	f = bfind(re, Bs("logging"));
	if(f.found.length > 0) {
		debug("Flipping logging bit!");
		logging = !logging; }
	
	return 0; }

struct frame
{
	ssize_t recvd;
	union 
	{
		uint32_t len;
		uint8_t frame[4092];
	};
};



int inmail(int epoll, struct objdata * obj) { 
	int * slots;
	struct frame * frame_freelist;

	uint32_t framelen;
	ssize_t r = recv(obj->fd, &framelen, sizeof(framelen), MSG_PEEK);

	if(r < 0)
	{
		debug("An error has occured.");
	}
	else if(r == 0)
	{
		debug("Connection closed by peer.");
		close(obj->fd);
	}
	else
	{
		if(r < sizeof(framelen))
		{
			return 0;
		}
		
		struct frame * buffer;
		buffer->recvd = recv(obj->fd, buffer->frame, 4092, 0);

		if(buffer->recvd == buffer->len) 
		{
			// Interpret the frame.
		}
	}
}	


int main(int argc, char ** argv) {

	int epoll = epoll_create(1);
	uint16_t cacheport = 8080;
	int slot_exp = 10;
	int slot_len = 1024;

	int slots = malloc(sizeof(int) * (1 << slot_exp) * slot_len);

	int cachesock = socket(AF_INET6, SOCK_STREAM, 0);
	prepare_socket(cachesock, cacheport);
	
	struct objdata_ex * data = malloc(sizeof(*data));
	prepare_objdata_ex(data, 0, &interm, &err, &hup, &kill, &p2p);
	epoll_add(epoll, cachesock, data);

	// Interactive mode support.
	if(argc == 2 && strcmp(argv[0], "-i")) {
		printf("Interactive mode. Commands: list, add, remove, ping.\n");


		struct objdata_ex * data = malloc(sizeof(*data));

		prepare_objdata_ex(data, 0, &interm, &err, &hup, &kill, &p2p);

		epoll_add(epoll, 0, data);
	}

	eventloop(epoll);
	return 0; }


