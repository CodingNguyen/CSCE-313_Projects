/*
    Cody Nguyen
    Department of Computer Science & Engineering
    Texas A&M University
    Date  : 10/2/20
 */
#include "common.h"
#include <sys/wait.h>
#include "FIFOreqchannel.h"
#include "MQreqchannel.h"
#include "SHMreqchannel.h"
#include <sys/wait.h>

using namespace std;

int main(int argc, char *argv[])
{
    int c;
    int buffercap = MAX_MESSAGE;
    int p = 0, ecg = 1;
    double t = -1.0;
    bool isnewchan = false;
    bool isfiletransfer = false;
    string filename;
    string ipcmethod = "f"; // IPC method to use, FIFO by default
    int nchannels = 1;      // number of channels to create, 1 by default

    struct timeval start, end;
    double timingArr[nchannels];

    while ((c = getopt(argc, argv, "p:t:e:m:f:c:i:")) != -1)
    {
        switch (c)
        {
        case 'p':
            p = atoi(optarg);
            break;
        case 't':
            t = atof(optarg);
            break;
        case 'e':
            ecg = atoi(optarg);
            break;
        case 'm':
            buffercap = atoi(optarg);
            break;
        case 'c':
            isnewchan = true;
            nchannels = atoi(optarg);
            break;
        case 'f':
            isfiletransfer = true;
            filename = optarg;
            break;
        case 'i':
            ipcmethod = optarg;
            break;
        }
    }

    // fork part
    if (fork() == 0)
    { // child

        // passing all args to server
        char *args[] = {"./server", "-m", (char *)to_string(buffercap).c_str(), "-i", (char *)ipcmethod.c_str(), NULL};
        if (execvp(args[0], args) < 0)
        {
            perror("exec filed");
            exit(0);
        }
    }

    // supports FIFO, MQ, and SHM
    RequestChannel *control_chan;

    // creating control channel based on ipc method
    if (ipcmethod == "f") // FIFO
        control_chan = new FIFORequestChannel("control", RequestChannel::CLIENT_SIDE);
    else if (ipcmethod == "q") // MQ
        control_chan = new MQRequestChannel("control", RequestChannel::CLIENT_SIDE);
    else if (ipcmethod == "s") // SHM
        control_chan = new SHMRequestChannel("control", RequestChannel::CLIENT_SIDE, buffercap);

    RequestChannel *chan = control_chan;
    // creating array of channel pointers to keep track of created channels
    RequestChannel *chanArr[nchannels];
    if (isnewchan)
    {
        cout << "Using the new channel everything following" << endl;
        // new channel based on ipc method
        for (int i = 0; i < nchannels; i++)
        {
            MESSAGE_TYPE m = NEWCHANNEL_MSG;
            control_chan->cwrite(&m, sizeof(m));
            char newchanname[100];
            control_chan->cread(newchanname, sizeof(newchanname));
            // create new channel based on IPC method
            if (ipcmethod == "f")
                chan = new FIFORequestChannel(newchanname, RequestChannel::CLIENT_SIDE);
            else if (ipcmethod == "q") // MQ
                chan = new MQRequestChannel(newchanname, RequestChannel::CLIENT_SIDE);
            else if (ipcmethod == "s") // SHM
                chan = new SHMRequestChannel(newchanname, RequestChannel::CLIENT_SIDE, buffercap);

            chanArr[i] = chan; // add channel ptr to array
            cout << "New channel by the name " << newchanname << " is created" << endl;
        }
        cout << "All further communication will happen through each channel instead of the main channel" << endl;
    }

    if (!isfiletransfer)
    { // requesting data msgs
        if (t >= 0)
        { // 1 data point
            datamsg d(p, t, ecg);
            double ecgvalue;

            if (nchannels > 1)
            {
                // print the same point for all channels except control
                for (int i = 0; i < nchannels; i++)
                {
                    chanArr[i]->cwrite(&d, sizeof(d));
                    chanArr[i]->cread(&ecgvalue, sizeof(double));
                    cout << chanArr[i]->name() << ": Ecg " << ecg << " value for patient " << p << " at time " << t << " is: " << ecgvalue << endl;
                }
            }
            else
            {
                // if there is only one channel, control channel retrieves point
                chan->cwrite(&d, sizeof(d));
                chan->cread(&ecgvalue, sizeof(double));
                cout << "Ecg " << ecg << " value for patient " << p << " at time " << t << " is: " << ecgvalue << endl;
            }
        }
        else
        { // bulk (i.e., 1K) data requests
            double ts = 0;
            datamsg d(p, ts, ecg);
            double ecgvalue;

            if (nchannels > 1)
            {
                // print the same 1k points for all channels except control
                for (int i = 0; i < nchannels; i++)
                {
                    gettimeofday(&start, NULL); // start timing
                    cout << chanArr[i]->name() << ":\n";
                    for (int i = 0; i < 1000; i++)
                    {
                        chan->cwrite(&d, sizeof(d));
                        chan->cread(&ecgvalue, sizeof(double));
                        d.seconds += 0.004; //increment the timestamp by 4ms
                        cout << ecgvalue << endl;
                    }
                    gettimeofday(&end, NULL); // end timing

                    // time taken calculation
                    double time;
                    time = (end.tv_sec - start.tv_sec) * 1e6;
                    time = (time + (end.tv_usec - start.tv_usec)) * 1e-6;
                    timingArr[i] = time;
                }

                for (int i = 0; i < nchannels; i++)
                    cout << chanArr[i]->name() << ": Executed in " << fixed << timingArr[i] << " seconds\n";
            }
            else
            {
                // if there is only one channel, control channel retrieves 1k points
                gettimeofday(&start, NULL); // start timing
                cout << chan->name() << ":\n";
                for (int i = 0; i < 1000; i++)
                {
                    chan->cwrite(&d, sizeof(d));
                    chan->cread(&ecgvalue, sizeof(double));
                    d.seconds += 0.004; //increment the timestamp by 4ms
                    cout << ecgvalue << endl;
                }
                gettimeofday(&end, NULL); // end timing

                // time taken calculation
                double time;
                time = (end.tv_sec - start.tv_sec) * 1e6;
                time = (time + (end.tv_usec - start.tv_usec)) * 1e-6;
                timingArr[0] = time;
                cout << "control: Executed in " << fixed << timingArr[0] << " seconds\n";
            }
        }
    }
    else if (isfiletransfer)
    {
        // timing
        gettimeofday(&start, NULL);
        // part 2 requesting a file
        filemsg f(0, 0);                                      // special first message to get file size
        int to_alloc = sizeof(filemsg) + filename.size() + 1; // extra byte for NULL
        char *buf = new char[to_alloc];
        memcpy(buf, &f, sizeof(filemsg));
        strcpy(buf + sizeof(filemsg), filename.c_str());
        chan->cwrite(buf, to_alloc);

        __int64_t filesize;
        chan->cread(&filesize, sizeof(__int64_t));
        cout << "File size: " << filesize << endl;

        filemsg *fm = (filemsg *)buf;
        __int64_t rem = filesize;
        string outfilepath = string("received/") + filename;
        FILE *outfile = fopen(outfilepath.c_str(), "wb");
        fm->offset = 0;

        // split each file between each channel: filesize / nchannels
        __int64_t fileSizeSplit = filesize / nchannels;

        char *recv_buffer = new char[MAX_MESSAGE];
        int j = 0;
        while (rem > 0)
        {
            if (nchannels <= 1)
            {
                fm->length = (int)min(rem, (__int64_t)MAX_MESSAGE);
                chan->cwrite(buf, to_alloc);
                chan->cread(recv_buffer, MAX_MESSAGE);
                fwrite(recv_buffer, 1, fm->length, outfile);
                rem -= fm->length;
                fm->offset += fm->length;
            }
            else
            {
                if (j >= nchannels)
                    j = 0;
                fm->length = (int)min(rem, (__int64_t)MAX_MESSAGE);

                chanArr[j]->cwrite(buf, to_alloc);
                chanArr[j]->cread(recv_buffer, MAX_MESSAGE);
                // write to file
                fwrite(recv_buffer, 1, fm->length, outfile);
                rem -= fm->length;
                fm->offset += fm->length;
                cout << chanArr[j]->name() << ": " << fm->offset << endl;
                j++;
            }
        }
        fclose(outfile);
        delete recv_buffer;
        delete buf;
        cout << "File transfer completed" << endl;
        gettimeofday(&end, NULL); // end timing

        // time taken calculation
        double time;
        time = (end.tv_sec - start.tv_sec) * 1e6;
        time = (time + (end.tv_usec - start.tv_usec)) * 1e-6;
        timingArr[0] = time;
        cout << "control: Executed in " << fixed << timingArr[0] << " seconds\n";
    }

    MESSAGE_TYPE q = QUIT_MSG;
    if (nchannels > 1)
        for (int i = 0; i < nchannels; i++)
        {
            chanArr[i]->cwrite(&q, sizeof(MESSAGE_TYPE));
            delete chanArr[i];
        }

    // this means that the user requested a new channel, so the control_channel must be destroyed as well
    control_chan->cwrite(&q, sizeof(MESSAGE_TYPE));

    // wait for the child process running server
    // this will allow the server to properly do clean up
    // if wait is not used, the server may sometimes crash
    wait(0);
}