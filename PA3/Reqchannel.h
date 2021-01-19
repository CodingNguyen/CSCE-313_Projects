#ifndef _reqchannel_H_
#define _reqchannel_H_

#include "common.h"
// abstract class for all channel implementations
class RequestChannel
{
public:
	enum Side {SERVER_SIDE, CLIENT_SIDE};
	enum Mode {READ_MODE, WRITE_MODE};
	
protected:
	string my_name;
	Side my_side;
	
	int wfd;
	int rfd;
	
	string s1, s2; //abstract name
	
	int open_ipc(string _pipe_name, int mode){;}
	
public:
	RequestChannel(const string _name, const Side _side) : my_name(_name), my_side(_side){}
	// must be included in derived classes
	virtual ~RequestChannel(){} // class specific destructors
	virtual int cread(void* msgbuf, int bufcapacity) = 0; // blocking read, returns bytes read, returns -1 if read fails
	virtual int cwrite(void *msgbuf , int msglen) = 0; // write data to the channel, returns number of char written, -1 if fail
	
	string name() { return my_name; }// return name of channel
};

#endif
