#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "common.h"
#include "HistogramCollection.h"
#include "TCPreqchannel.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <iomanip>
#include <sys/epoll.h>
using namespace std;

// function for patient thread, takes in number of requests, patient num, and boundedbuffer ptr
void patient_thread_function(int n, int patientN, BoundedBuffer *requestBuffer)
{
    /* What will the patient threads do? */
    // collect data message
    datamsg d(patientN, 0.0, 1); // takes in patient num, time stamp, ecg number
    // double response = 0; // for cread to change
    for (int i = 0; i < n; i++)
    {
        requestBuffer->push((char *)&d, sizeof(datamsg)); // push data message to request buffer
        d.seconds += .004;                                // increment to next time stamp
    }
}

void event_polling_thread(int n, int p, int w, int m, TCPRequestChannel *wchans[], BoundedBuffer *request_buffer, HistogramCollection *hc, int mb)
{
    char buf[1024];
    double response;
    char recvbuf[mb];

    struct epoll_event ev;
    struct epoll_event events[w];

    bool quit_sent = false;

    // create empty epoll list
    int epollfd = epoll_create1(0); // returns file descriptor that is handle to epoll list
    if (epollfd == -1)
        EXITONERROR("epoll_create1");

    unordered_map<int, int> fd_to_index; // remembers what file descriptors corresponds to each index
    vector<vector<char>> state(w);       // remember what is written

    // priming, w requests, make sure all channels busy and adding each rfd to list
    int nsent = 0;
    int nrecv = 0;

    for (int i = 0; i < w; i++)
    {
        int sz = request_buffer->pop(buf, 1024);
        if (*(MESSAGE_TYPE *)buf == QUIT_MSG) // check if quit message
        {
            quit_sent = true;
            break;
        }
        wchans[i]->cwrite(buf, sz);             // does not decode message type, just send directly from request buffer
        state[i] = vector<char>(buf, buf + sz); // record the state [i]
        nsent++;
        int rfd = wchans[i]->getfd(); // add channel file descriptor to epoll list
        // notifies when activity level changes, need level trigger
        fcntl(rfd, F_SETFL, O_NONBLOCK); // has to be non blocking
        ev.events = EPOLLIN | EPOLLET;   // watch as input file | level trigger mode
        ev.data.fd = rfd;                // puts file descriptor in
        fd_to_index[rfd] = i;

        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, rfd, &ev) == -1) // adds rfd onto epollfd
            EXITONERROR("epoll_ctl: listen_sock");
    }
    // nsent = w, because all sent

    // infinite loop until quit message
    while (true)
    {
        if (quit_sent && nrecv == nsent)
            break;
        // polling synchronous,
        // wait before cread is issued because there are w channels, by blocking
        // tells how many channels have data, can be more than one
        int nfds = epoll_wait(epollfd, events, w, -1); // events contains the ready file descriptors
        if (nfds == -1)
            EXITONERROR("epoll_wait");

        // read from each ready file descriptor and start reusing them
        for (int i = 0; i < nfds; i++) // i does not correspond to which channel is which
        {
            int rfd = events[i].data.fd;
            int index = fd_to_index[rfd];

            int resp_sz = wchans[index]->cread(recvbuf, mb);
            nrecv++;

            // process recvbuf
            vector<char> req = state[index]; // retrieve request saved previously
            char *request = req.data();      // convert vector to char pointer

            // handle based on message type
            MESSAGE_TYPE *m = (MESSAGE_TYPE *)request;
            if (*m == DATA_MSG)
                hc->update(((datamsg *)request)->person, *(double *)recvbuf);
            else if (*m == FILE_MSG)
            {
                filemsg *fm = (filemsg *)request;
                string fname = (char *)(fm + 1);             // file name starts there
                int sz = sizeof(filemsg) + fname.size() + 1; // size of file msg + size of name + null byte
                
                wchans[index]->cread(recvbuf, mb);                    // read file message
                string recvName = "received/" + fname;
                FILE *fp = fopen(recvName.c_str(), "r+"); // opening file in C method, in read and write mode, expects file exists with correct name
                fseek(fp, fm->offset, SEEK_SET);          // chunk before may not have arrived yet when writing, so seek
                fwrite(recvbuf, 1, fm->length, fp);       // write content to file
                fclose(fp);
            }

            // reuse
            if (!quit_sent)
            {
                int req_sz = request_buffer->pop(buf, sizeof(buf));
                if (*(MESSAGE_TYPE *)buf == QUIT_MSG) // check if quit message
                    quit_sent = true;
                else
                {
                    wchans[index]->cwrite(buf, req_sz);
                    state[index] = vector<char>(buf, buf + req_sz);
                    nsent++;
                }
            }
        }
    }
}

void file_thread_function(string fileName, BoundedBuffer *requestBuffer, TCPRequestChannel *chan, int mb)
{
    // create file name
    string recvFileName = "received/" + fileName;

    // make as long as original length
    char buf[1024];                            // big enough for our application
    filemsg f(0, 0);                           // buffer should have a file message
    memcpy(buf, &f, sizeof(f));                // copy f to buffer buf
    strcpy(buf + sizeof(f), fileName.c_str()); // copy file name to back of file message

    chan->cwrite(buf, sizeof(f) + fileName.size() + 1);
    __int64_t fileLength;
    chan->cread(&fileLength, sizeof(fileLength)); // read file size

    FILE *fp = fopen(recvFileName.c_str(), "w"); // create in binary write mode
    fseek(fp, fileLength, SEEK_SET);
    fclose(fp);

    // generate all the file messages
    filemsg *fm = (filemsg *)buf;
    __int64_t remlen = fileLength;

    while (remlen > 0)
    {
        fm->length = min(remlen, (__int64_t)mb);
        requestBuffer->push(buf, sizeof(filemsg) + fileName.size() + 1);
        fm->offset += fm->length;
        remlen -= fm->length;
    }
}

int main(int argc, char *argv[])
{
    int n = 0;           //default number of requests per "patient"
    int p = 10;          // number of patients [1,15]
    int w = 100;         //default number of request channels, now 1 worker thread
    int b = 20;          // default capacity of the request buffer, you should change this default
    int m = MAX_MESSAGE; // default capacity of the message buffer
    HistogramCollection hc;
    bool fileTransfer = false;
    string fileName;
    string host, port;
    srand(time_t(NULL));

    int opt = -1;
    while ((opt = getopt(argc, argv, "m:n:b:w:p:f:h:r:")) != -1)
    {
        switch (opt)
        {
        case 'm':
            m = atoi(optarg);
            break;
        case 'n':
            n = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 'w':
            w = atoi(optarg);
            break;
        case 'p':
            p = atoi(optarg);
            break;
        case 'f':
            fileName = optarg;
            fileTransfer = true;
            break;
        case 'h':
            host = optarg;
            break;
        case 'r':
            port = optarg;
            break;
        }
    }

    BoundedBuffer request_buffer(b);

    // creating histograms and adding to collection of histograms, hc
    for (int i = 0; i < p; i++)
    {
        Histogram *h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }

    struct timeval start, end;
    gettimeofday(&start, 0);

    // creating a worker channel per worker
    // pointers so channels are not copied everytime
    cout << "Starting to connect workers...\n";
    TCPRequestChannel** wchans = new TCPRequestChannel *[w];
    for (int i = 0; i < w; i++)
        wchans[i] = new TCPRequestChannel(host, port);
    cout << "All connected!\n";
    /* Start all threads here */
    // creating a thread, a single sequence stream in the process, for each patient
    thread patient[p];
    for (int i = 0; i < p; i++)
        patient[i] = thread(patient_thread_function, n, i + 1, &request_buffer);

    // create 1 worker thread, event polling thread
    thread evp(event_polling_thread, n, p, w, m, wchans, &request_buffer, &hc, m);

    // create file thread and joining if -f flag is present
    if (fileTransfer)
    {
        thread fileThread(file_thread_function, fileName, &request_buffer, wchans[0], m);
        fileThread.join(); // join singular file thread
    }

    /* Join all threads here */
    for (int i = 0; i < p; i++)
        patient[i].join();

    cout << "Patient threads finished\n";

    // Clean up open channels after patient threads finish
    MESSAGE_TYPE q = QUIT_MSG;

    request_buffer.push((char *)&q, sizeof(q)); // last message on buffer
    evp.join();
    cout << "Worker thread finished\n";

    // clean up worker channels
    for (int i = 0; i < w; i++)
    {
        wchans[i]->cwrite(&q, sizeof(MESSAGE_TYPE)); // delete on server side
        delete wchans[i];                            // delete on client side
    }
    delete[] wchans;

    gettimeofday(&end, 0);
    // stopping timer

    // print the results
    hc.print();

    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) / (int)1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) % ((int)1e6);
    cout << "Took " << secs << "." << usecs << " seconds" << endl;

    cout << "All Done!!!" << endl;
}
