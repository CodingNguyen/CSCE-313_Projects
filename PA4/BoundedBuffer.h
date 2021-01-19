#ifndef BoundedBuffer_h
#define BoundedBuffer_h

#include <stdio.h>
#include <queue>
#include <string>
#include <thread>
#include <mutex>
#include <assert.h>
#include <vector>
#include <string.h>
#include <condition_variable>

using namespace std;

class BoundedBuffer
{
private:
	int cap; // max number of items in the buffer
	// queue because FIFO, vector of char because each message is variable size
	queue<vector<char>> q;	/* the queue of items in the buffer. Note
	that each item a sequence of characters that is best represented by a vector<char> for 2 reasons:
	1. An STL std::string cannot keep binary/non-printables
	2. The other alternative is keeping a char* for the sequence and an integer length (i.e., the items can be of variable length).
	While this would work, it is clearly more tedious */

	// add necessary synchronization variables and data structures 

	// for thread safety

	/* synchronization primitive that can be used to protect shared data from being simultaneously accessed by multiple threads */
	mutex m;

	/* used to block a thread, or multiple threads at the same time,
	until another thread both modifies a shared variable (the condition),
	and notifies the condition_variable.*/
	condition_variable dataAvailable; // waited on by pop, signaled by push function
	condition_variable slotAvailable; // waited on by push, signaled by pop function

public:
	BoundedBuffer(int _cap)
	{
		cap = _cap; // store desired capacity
	}

	~BoundedBuffer()
	{

	}

	void push(char* data, int len){
		//0. Convert the incoming byte sequence given by data and len into a vector<char>
		vector<char> d(data, data + len);

		//1. Wait until there is room in the queue (i.e., queue lengh is less than cap)
		// should wait NOT return s.t. push only needs to be called once
		unique_lock<mutex> l(m); // manages a mutex object with unique ownership in both states
		slotAvailable.wait(l, [this]{return q.size() < cap;}); // sleeps, only pushes if slot is available (q size is less than capacity)
		
		//2. Then push the vector at the end of the queue, watch for race condition
		// m.lock(); // locks mutex of current thread until another thread unlocks it
		q.push(d); // only one item should be pushed at a time
		l.unlock(); // unlocks mutex, releases ownership. Another blocked thread takes ownership and continues execution
		
		//3. Wake up potential sleeping pop() threads
		dataAvailable.notify_one(); // notify one thread that data is available
	}

	int pop(char* buf, int bufcap){
		//1. Wait until the queue has at least 1 item
		unique_lock<mutex> l(m); // manages a mutex object with unique ownership in both states
		dataAvailable.wait(l, [this]{return q.size() > 0;}); // sleeps, only pops if data is available
		
		//2. pop the front item of the queue. The popped item is a vector<char>, watch for race condition
		// m.lock(); // locks mutex of current thread until another thread unlocks it naive, do not lock again
		vector<char> d = q.front(); // save item at front
		q.pop();
		l.unlock(); // unlocks mutex, releases ownership. Another blocked thread takes ownership and continues execution

		//3. Convert the popped vector<char> into a char*, copy that into buf, make sure that vector<char>'s length is <= bufcap
		assert(d.size() <= bufcap); // terminates program if statement is false
		memcpy(buf, d.data(), d.size()); // copy data byte sequence from vector into buffer

		//4. Wake up potential sleeping push() threads
		slotAvailable.notify_one(); // only wake up one push function, will still work because it will just go back to sleep, but inefficient
		
		
		//5. Return the vector's length to the caller so that he knows many bytes were popped
		return d.size();
	}
};

#endif /* BoundedBuffer_ */
