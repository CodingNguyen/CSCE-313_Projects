#include "common.h"
#include "BoundedBuffer.h"
#include "Histogram.h"
#include "common.h"
#include "HistogramCollection.h"
#include "FIFOreqchannel.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <iomanip>
using namespace std;

// FOR BONUS
HistogramCollection hc;
__int64_t fileLength;
__int64_t remainingLength;
bool fileTransfer = false;
string fileName;

// clears terminal window and prints histogram collection
void handlerFunction(int num)
{
    //system("clear"); // clear terminal window
    if(fileTransfer)
    {
        long double percent = (fileLength - remainingLength) / fileLength * 100;
        cout << "File transfer is " << fixed << setprecision(4) << percent << "% complete\n";
    }
    else if(hc.histogramAmount() > 0)
        hc.print(); // print histogram collection
}

// sends new channel message using the main channel
// and returns new channel on the client side
FIFORequestChannel *createNewChannel(FIFORequestChannel *mainChan)
{
    char name[1024];
    MESSAGE_TYPE m = NEWCHANNEL_MSG;
    // writes new channel message to server
    mainChan->cwrite(&m, sizeof(m));
    // read name of size 1024
    mainChan->cread(name, 1024);
    FIFORequestChannel *newChan = new FIFORequestChannel(name, FIFORequestChannel::CLIENT_SIDE);
    return newChan;
}

// function for patient thread, takes in number of requests, patient num, and boundedbuffer ptr
void patient_thread_function(int n, int patientN, BoundedBuffer *requestBuffer)
{
    /* What will the patient threads do? */
    // collect data message
    datamsg d(patientN, 0.0, 1); // takes in patient num, time stamp, ecg number
    // double response = 0; // for cread to change
    for (int i = 0; i < n; i++)
    {
        // commented solution can create stragglers
        // chan->cwrite(&d, sizeof(datamsg)); // write to server with data message
        // chan->cread(&response, sizeof(double)); // read response
        // hc->update(patientN, response); // update histogram of given patient
        requestBuffer->push((char *)&d, sizeof(datamsg)); // push data message to request buffer
        d.seconds += .004;                                // increment to next time stamp
    }
}

void worker_thread_function(FIFORequestChannel *chan, BoundedBuffer *requestBuffer, HistogramCollection *hc, int mb)
{
    // infinite loop because worker threads don't know how many requests it will process, some will process many more than others
    char buf[1024];
    double response;
    char recvbuf[mb];

    while (true)
    {
        requestBuffer->pop(buf, 1024);         // popping item from request buffer, placing it in a sufficiently sized char array
        MESSAGE_TYPE *m = (MESSAGE_TYPE *)buf; // save the message type
        // handle based on type of message
        if (*m == DATA_MSG)
        {
            chan->cwrite(buf, sizeof(datamsg));             // write to server with data message
            chan->cread(&response, sizeof(double));         // read response
            hc->update(((datamsg *)buf)->person, response); // update histogram of given patient with read response
        }
        else if (*m == QUIT_MSG)
        {
            chan->cwrite(m, sizeof(MESSAGE_TYPE));
            delete chan;
            break;
        }
        else if (*m == FILE_MSG)
        {
            filemsg *fm = (filemsg *)buf;                // points to beginning of buffer
            string fname = (char *)(fm + 1);             // file name starts there
            int sz = sizeof(filemsg) + fname.size() + 1; // size of file msg + size of name + null byte
            chan->cwrite(buf, sz);                       // write file message
            chan->cread(recvbuf, mb);                    // read file message
            cout << fname << endl;
            string recvName = "recv/" + fname;
            FILE *fp = fopen(recvName.c_str(), "r+"); // opening file in C method, in read and write mode, expects file exists with correct name
            fseek(fp, fm->offset, SEEK_SET);          // chunk before may not have arrived yet when writing, so seek
            fwrite(recvbuf, 1, fm->length, fp);       // write content to file
            fclose(fp);
        }
    }
}

void file_thread_function(string fileName, BoundedBuffer *requestBuffer, FIFORequestChannel *chan, int mb)
{
    // create file name
    string recvFileName = "recv/" + fileName;

    // make as long as original length
    char buf[1024];                            // big enough for our application
    filemsg f(0, 0);                           // buffer should have a file message
    memcpy(buf, &f, sizeof(f));                // copy f to buffer buf
    strcpy(buf + sizeof(f), fileName.c_str()); // copy file name to back of file message

    chan->cwrite(buf, sizeof(f) + fileName.size() + 1);
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
        remainingLength = remlen;
    }
}

int main(int argc, char *argv[])
{
    // registering signal-handler function as handler for SIGALRM
    struct sigaction sa;
    sa.sa_handler = handlerFunction;
    if(sigaction(SIGALRM, &sa, NULL))
        perror("Sigaction");

    // setting signal handler parameters
    struct sigevent se;
    se.sigev_notify = SIGEV_SIGNAL; // Notifies process by sending signal in sigev_signo
    se.sigev_signo = SIGALRM; // The notification signal

    // creating timer
    timer_t timer1;
    timer_create(CLOCK_REALTIME, &se, &timer1);

    // setting timer to 2 second intervals
    struct itimerspec its;
    its.it_value.tv_sec = 2; // set interval to 2 seconds
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    int n = 0;         //default number of requests per "patient"
    int p = 10;          // number of patients [1,15]
    int w = 100;         //default number of worker threads
    int b = 20;          // default capacity of the request buffer, you should change this default
    int m = MAX_MESSAGE; // default capacity of the message buffer
    srand(time_t(NULL));

    int opt = -1;
    while ((opt = getopt(argc, argv, "m:n:b:w:p:f:")) != -1)
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
        }
    }

    int pid = fork();
    if (pid == 0)
    {
        // modify this to pass along m
        execl("server", "server", "-m", (char *)to_string(m).c_str());
    }

    FIFORequestChannel *chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    BoundedBuffer request_buffer(b);

    // creating histograms and adding to collection of histograms, hc
    for (int i = 0; i < p; i++)
    {
        Histogram *h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }
    timer_settime(timer1, 0, &its, NULL);
    
    struct timeval start, end;
    gettimeofday(&start, 0);

    // creating a worker channel per worker
    // pointers so channels are not copied everytime
    FIFORequestChannel *wchans[w];
    for (int i = 0; i < w; i++)
        wchans[i] = createNewChannel(chan);

    /* Start all threads here */
    // creating a thread, a single sequence stream in the process, for each patient
    thread patient[p];
    for (int i = 0; i < p; i++)
        patient[i] = thread(patient_thread_function, n, i + 1, &request_buffer);

    thread workers[w];
    for (int i = 0; i < w; i++)
        workers[i] = thread(worker_thread_function, wchans[i], &request_buffer, &hc, m);

    // create file thread and joining if -f flag is present
    if (fileTransfer)
    {
        thread fileThread(file_thread_function, fileName, &request_buffer, chan, m);
        fileThread.join(); // join singular file thread
    }

    /* Join all threads here */
    for (int i = 0; i < p; i++)
        patient[i].join();

    cout << "Patient threads finished\n";

    // Clean up open channels after patient threads finish
    MESSAGE_TYPE q = QUIT_MSG;
    for (int i = 0; i < w; i++)
        request_buffer.push((char *)&q, sizeof(q));

    for (int i = 0; i < w; i++)
        workers[i].join();

    cout << "Worker threads finished\n";

    gettimeofday(&end, 0);
    // stopping timer

    // print the results
    timer_delete(timer1);
    // hc.print();
    handlerFunction(0);
    int secs = (end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) / (int)1e6;
    int usecs = (int)(end.tv_sec * 1e6 + end.tv_usec - start.tv_sec * 1e6 - start.tv_usec) % ((int)1e6);
    cout << "Took " << secs << "." << usecs << " seconds" << endl;

    chan->cwrite((char *)&q, sizeof(MESSAGE_TYPE));
    cout << "All Done!!!" << endl;
    delete chan;
    wait(0);
}
