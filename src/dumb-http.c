#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "bytes.c"

struct objdata;
typedef int (*objfun)(int epoll, struct objdata * blob);
struct objdata {
	int fd;
	objfun in, err, hup, kill; };


	
int eventloop(int epollfd) {
	int max_events = 1024;
	struct epoll_event * buffer = malloc(max_events * sizeof(*buffer));
	while(true) {
		int ready = epoll_wait(epollfd, buffer, max_events, -1);

		debug("Events ready: %d", ready);
		for(int i = 0; i < ready; i++) {
			int events = buffer[i].events;
			struct objdata * d = buffer[i].data.ptr;
			objfun fun;
			int r = 0;
			
			if(events & EPOLLERR) r += d->err(epollfd, d);
			else {
				if(events & EPOLLIN) r += d->in(epollfd, d);
				if(events & EPOLLHUP) r += d->hup(epollfd, d); }

			if(r) { 
				debug("Socket callback failed")
				d->kill(epollfd, d); }
			}} }


int main(int argc, char ** argv) {

	int epoll = epoll_create(1);

	int listener = socket(AF_INET6, SOCK_STREAM, 0);
	
	int r = true;
	setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &r, sizeof(r));

	struct sockaddr_in6 def = { AF_INET6, htons(80), 0, 0, 0 };
	if(bind(listener, (struct sockaddr *) &def, sizeof(def))) {
		debug("Bind failed!");
		return -1; }

	

	int kill(int epoll, struct objdata * data) {
		// Don't worry, epoll concatenates events on the same destriptor.
		debug("Terminating!");
		close(data->fd);
		free(data);
		return 0; }

	int err(int epoll, struct objdata * data ) {
		debug("Receiving error: %s", strerror(errno));
		data->kill(epoll, data);
		return 0; }

	int hup(int epoll, struct objdata * data ) {
		debug("Hangup received.");
		data->kill(epoll, data);
		return 0; }


	int in(int epoll, struct objdata * data) {
		debug("Accepting a new connection.");
		struct sockaddr_in6 def;
		socklen_t len = sizeof(def);

		int socket = accept(data->fd, (struct sockaddr *) &def, &len);

		// Clients have their own "in" callback.
		int in(int epoll, struct objdata * data) {
			uint8_t buffer[4096];
			debug("Received data.  ------------");
			ssize_t length = recv(data->fd, buffer, 4096, 0);
			debug("%d bytes of data.", length);
			if(length <= 0 || length > 4096) {
				debug("Ending connection.")
				kill(epoll, data);
				return 0; }

			write(0, buffer, length);
			if(buffer[0] == 'G') {
				char * page = "HTTP/1.1 200 OK\r\n"
"Content-Length: 5\r\n"
"Content-Type: text/html\r\n"
"\r\nhello";
				send(data->fd, page, strlen(page), 0); }
			debug("----------------------------");
			return 0; }

		struct objdata * d = malloc(sizeof(struct objdata));
		*d = (struct objdata) { socket, &in, &err, &hup, &kill };
		
		struct epoll_event e;
		e.events = EPOLLIN | EPOLLET;
		e.data.ptr = d;

		if(epoll_ctl(epoll, EPOLL_CTL_ADD, socket, &e) < 0) {
			debug("Accept epoll_ctl failed"); return -1; }

		return 0;
	}
		

	struct objdata listenerdata = { listener, &in, &err, &hup, &kill };

	
	struct epoll_event e;
	e.events = EPOLLIN | EPOLLET;
	e.data.ptr = &listenerdata;

	if(epoll_ctl(epoll, EPOLL_CTL_ADD, listener, &e) < 0) {
		debug("Listener epoll_ctl failed"); return -1; }

	listen(listener, 1024);


	eventloop(epoll);
	return 0; }


	

