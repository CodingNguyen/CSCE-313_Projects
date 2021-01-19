
#ifndef _SHMreqchannel_H_
#define _SHMreqchannel_H_

#include <semaphore.h>
#include <sys/mman.h>
#include <string>

#include "common.h"
#include "Reqchannel.h"

using namespace std;

// kernel-semaphore protected shared memory segments rather than normal file descriptors
class SHMQ
{
private:
	char *segment;
	sem_t *sd;
	sem_t *rd;
	string name;
	int len;

public:
	SHMQ(string _name, int _len) : name(_name), len(_len)
	{
		int fd = shm_open(name.c_str(), O_RDWR | O_CREAT, 0600);
		if (fd < 0)
		{
			EXITONERROR("could not create/open shared memory segment");
		}
		ftruncate(fd, len); //set the length to 1024, the default is 0, so this is a necessary step

		segment = (char *)mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (!segment)
		{
			EXITONERROR("Cannot map");
		}

		rd = sem_open((name + "_rd").c_str(), O_CREAT, 0600, 1); 
		sd = sem_open((name + "_sd").c_str(), O_CREAT, 0600, 0); 
	}
	
	// shared memory "send"
	int shm_send(void* msg, int len)
	{
		sem_wait(rd);
		memcpy(segment, msg, len);
		sem_post(sd);
		return len;
	}

	// shared memory "receive"
	int shm_recv(void* msg, int len)
	{
		sem_wait(sd);
		memcpy(msg, segment, len);
		sem_post(rd);
		return len;
	}

	~SHMQ()
	{
		sem_close(sd);
		sem_close(rd);
		sem_unlink((name + "_rd").c_str());
		sem_unlink((name + "_sd").c_str());

		// deleting the mapping of the segment mapped previously
		munmap(segment, len);
		// unlinking shared memory object
		shm_unlink(name.c_str());
	}
};

class SHMRequestChannel : public RequestChannel
{
private:
	SHMQ* shmq1;
	SHMQ* shmq2;
	int len;
public:
	SHMRequestChannel(const string _name, const Side _side, const int _len);
	~SHMRequestChannel();

	int cread(void *msgbuf, int bufcapacity);
	int cwrite(void *msgbuf, int msglen);
};

#endif
