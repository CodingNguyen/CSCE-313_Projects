/*
    Cody Nguyen
    Department of Computer Science & Engineering
    Texas A&M University
    Date  : 9/4/20
 */
#include "common.h"
#include "FIFOreqchannel.h"
#include <sys/wait.h>

using namespace std;

int buffercapacity = MAX_MESSAGE;

int main(int argc, char *argv[]){
    
    // for time recording
    struct timeval start, end;
    // request a single data point from the command line in the format:
    // ./client -p <person no> -t <time in sec> -e <ecg no>
    // parse command-line arguments.
    int opt;
    datamsg d (0, 0, 0);

    bool tPresent = false;
    bool fPresent = false;
    bool cPresent = false;
    bool pPresent = false;
    bool ePresent = false;

    ofstream outputFile;
    
    // calls getopt until all options are read
    string filename;
    while ((opt=getopt(argc, argv, "cp:t:e:f:m:")) != -1)
    {
        switch (opt)
        {
        case ('p'):
            d.person = atoi(optarg);
            pPresent = true;
            break;
        case ('t'):
            d.seconds = atof(optarg);
            tPresent = true;
            break;
        case ('e'):
            d.ecgno = atoi(optarg);
            ePresent = true;
            break;
        case ('m'):
            buffercapacity = atoi(optarg);
            break;
        case ('f'):
            filename = optarg;
            fPresent = true;
            break;
        case('c'):
            cPresent = true;
            break;
        }
    }

    // running the server as a child process
    // starting another client process
    pid_t pid = fork();
    if(pid <= 0) // child, running server from here
    {
        string temp = "-m " + to_string(buffercapacity);
        if(-1==execlp("./server", "./server", (char*)temp.c_str(), (char*)NULL))
            perror("exec");
    }

    FIFORequestChannel chan ("control", FIFORequestChannel::CLIENT_SIDE);
    // handling of all options
    double result;
    if(tPresent)
    {   
        // returns a single datapoint
        gettimeofday(&start, NULL);
        chan.cwrite(&d, sizeof(datamsg));
        chan.cread(&result, sizeof(double));
        gettimeofday(&end, NULL);
        cout << "Server output: " << result << endl;
        
        // time taken calculation
        double time;
        time = (end.tv_sec - start.tv_sec) * 1e6; 
        time = (time + (end.tv_usec - start.tv_usec)) * 1e-6;
        cout << "data point retrieved in " << time << " seconds\n";
    }
    else if(pPresent && ePresent)
    {
        // returns the 1st 1000 datapoints to x1.csv in received
        outputFile.open("received/x1.csv");

        gettimeofday(&start, NULL);
        for(int i = 0; i < 1000; i++)
        {
            d.seconds = i * .004;
            chan.cwrite(&d, sizeof(datamsg));
            chan.cread(&result, sizeof(double));
            outputFile << result << endl;
        }
        gettimeofday(&end, NULL);
        outputFile.close();
        
        // time taken calculation
        double time;
        time = (end.tv_sec - start.tv_sec) * 1e6; 
        time = (time + (end.tv_usec - start.tv_usec)) * 1e-6;
        cout << "1st 1000 datapoints of ecg" << d.ecgno << " written to x1.csv in " << time << " seconds\n";
    }

    // copying given file from server to client
    if(fPresent)
    {
        // getting file size from the server
        filemsg f(0, 0);

        // need character buffer big enough to hold file message, filename, and null byte
        char sizeBuf[sizeof(filemsg) + filename.size() + 1];

        // copy object file message
        memcpy(sizeBuf, &f, sizeof(filemsg));
        // append filename to end of file message
        // destination is buffer plus size of file message because filemsg is already occupying that space
        // copy C string and null char into char array
        strcpy(sizeBuf + sizeof(filemsg), filename.c_str());

        // sending out buffer to server, sending size of buffer
        chan.cwrite(sizeBuf, sizeof(sizeBuf));

        // server returning length of file
        __int64_t fileLength;
        chan.cread(&fileLength, sizeof(__int64_t));

        // receiving packets of data from the server

        __int64_t receiveLength = 0;

        int windowSize = buffercapacity;

        // runs until the buffer is less than the buffercapacity
        gettimeofday(&start, NULL);

        // opening file to write to in binary format
        outputFile.open("received/" + filename, ios::binary);
        while (fileLength - receiveLength > buffercapacity)
        {
            // the amount of bytes being transferred from server to client
            f.offset = receiveLength;
            f.length = windowSize;

            // copy file message
            memcpy(sizeBuf, &f, sizeof(filemsg));
            // append filename to end of file message
            strcpy(sizeBuf + sizeof(filemsg), filename.c_str());
            // sending out buffer and size of buffer to server
            chan.cwrite(sizeBuf, sizeof(sizeBuf));

            // char array receiving the data
            char* recvBuf = new char[windowSize];

            // receiving windowSize bytes of data from the server
            chan.cread(recvBuf, buffercapacity);
            outputFile.write(recvBuf, buffercapacity);

            receiveLength += windowSize;
            delete[] recvBuf;
            recvBuf = NULL;
        }

        // Receives the last part of the data if there is any
        if (fileLength - receiveLength > 0)
        {
            windowSize = fileLength - receiveLength;
            // the amount of bytes being transferred from server to client
            f.offset = receiveLength;
            f.length = windowSize;

            // copy file message
            memcpy(sizeBuf, &f, sizeof(filemsg));
            // append filename to end of file message
            strcpy(sizeBuf + sizeof(filemsg), filename.c_str());
            // sending out buffer and size of buffer to server
            chan.cwrite(sizeBuf, sizeof(sizeBuf));

            // char array receiving the data
            char* recvBuf = new char[windowSize];

            // receiving windowSize bytes of data from the server
            chan.cread(recvBuf, windowSize);
            outputFile.write(recvBuf, windowSize);

            delete[] recvBuf;
            recvBuf = NULL;
        }
        gettimeofday(&end, NULL);

        outputFile.close();
        
        // time taken calculation
        double fileTime;
        fileTime = (end.tv_sec - start.tv_sec) * 1e6; 
        fileTime = (fileTime + (end.tv_usec - start.tv_usec)) * 1e-6;
        cout << "Wrote to received/" << filename << " in " << fileTime << " seconds\n";
    }
    
    // creates 2nd channel and performs operations on that channel, then closes it
    if(cPresent)
    {
        // requesting a new channel
        MESSAGE_TYPE newChanMsg = NEWCHANNEL_MSG;
        chan.cwrite(&newChanMsg, sizeof(MESSAGE_TYPE));

        // reading new channel name
        char newChanName[buffercapacity];
        chan.cread(newChanName, sizeof(newChanName));
        cout << "new channel name: " << newChanName << endl;
        FIFORequestChannel newChan(newChanName, FIFORequestChannel::CLIENT_SIDE);

        // testing a few data requests for new channel
        datamsg test(15, 0.008, 1);
        datamsg test1(7, 1.000, 2);
        datamsg test2(15, 0.004, 2);

        double serverOutput;

        chan.cwrite(&test, sizeof(datamsg));
        chan.cread(&serverOutput, sizeof(double));
        cout << "Server output for new channel: " << serverOutput << endl;

        chan.cwrite(&test1, sizeof(datamsg));
        chan.cread(&serverOutput, sizeof(double));
        cout << "Server output for new channel: " << serverOutput << endl;

        chan.cwrite(&test2, sizeof(datamsg));
        chan.cread(&serverOutput, sizeof(double));
        cout << "Server output for new channel: " << serverOutput << endl;

        // quitting new channel
        MESSAGE_TYPE q = QUIT_MSG;
        newChan.cwrite(&q, sizeof(MESSAGE_TYPE));
    }
    // sending data to server and printing the result
    

    // // testing hard-coded case---------------------------------------------------------------------------------
    // // hardcoded test case, patient 15, at .0008 seconds, 1st ECG value
    // datamsg d (15, 0.008, 1);
    // // Sending datamsg d to server using channel, given its address and size
    // chan.cwrite(&d, sizeof(datamsg));

    // // read a double from the server to result, given size expected back from server
    // double result;
    // // can use size of result or size of datatype, but size of datatype is preferable as it's always right
    // chan.cread(&result, sizeof(double));
    // cout << "Server output: " << result << endl;
    

    // // sending a non-sense message, you need to change this----------------------------------------------------
    // char buf [MAX_MESSAGE];

    // in this case x is a pointer to a datamsg in the heap
    // datamsg* x = new datamsg (10, 0.004, 2);
	
    // so x is the address
	// chan.cwrite (x, sizeof (datamsg));
	
    // int nbytes = chan.cread (buf, MAX_MESSAGE);
	// double reply = *(double *) buf;
	
	// cout << reply << endl;

    // closing the channel    
    MESSAGE_TYPE m = QUIT_MSG;
    chan.cwrite (&m, sizeof (MESSAGE_TYPE));

    if(pid) // if pid is nonzero ie the parent
    {
        wait(0); // waiting for child process to finish
    }
}
