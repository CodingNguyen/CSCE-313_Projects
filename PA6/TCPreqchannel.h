
#ifndef _TCPreqchannel_H_
#define _TCPreqchannel_H_

#include "common.h"
#include <sys/socket.h>
#include <netdb.h>

class TCPRequestChannel
{
	// show server by keeping hostname empty
	// client is provided host and post number
private:
	// socket is bidirectional, 1 file descriptor
	int sockfd;
public:
	TCPRequestChannel(const string host, const string port);

	TCPRequestChannel(int fd);

	~TCPRequestChannel();
	/* Destructor of the local copy of the bus. By default, the Server Side deletes any IPC 
	 mechanisms associated with the channel. */


	int cread(void* msgbuf, int bufcapacity);
	
	int cwrite(void *msgbuf , int msglen);

	int getfd();
};

#endif
