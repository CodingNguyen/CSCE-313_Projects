#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

// Redirects from standard input to file
void inputRedirect(std::vector<std::string>& vec, int& inPos, int& outPos)
{
    if (inPos > 0 && inPos + 1 < vec.size())
    {
        // retrieve file to write to
        const char* filename = vec[inPos+1].c_str();

        // open file in read only, creates it if it doesn't exists
        // permissions allows owner of file to read and write to file
        // allows group owner and other users to read file
        int fd = open(filename, O_CREAT|O_RDONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

        // overwrite standard output with file
        dup2(fd, 0);

        // erasing the input redirect and its argument, moving index of output redirect in needed
        vec.erase(vec.begin() + inPos, vec.begin() + inPos + 2);
        if (outPos > inPos)
            outPos -= 2;
        inPos = -1;
    }   
}

// Redirects from standard output to file
void outputRedirect(std::vector<std::string>& vec, int& inPos, int& outPos)
{
    if (outPos > 0 && outPos + 1 < vec.size())
    {
        // retrieve file to write to
        const char* filename = vec[outPos+1].c_str();

        // open file in write only, creates it if it doesn't exists, overwrites if it does
        // permissions allows owner of file to read and write to file
        // allows group owner and other users to read file
        int fd = open(filename, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

        // overwrite standard output with file
        dup2(fd, 1);

        // erasing the input redirect and its argument, moving index of output redirect in needed
        vec.erase(vec.begin() + outPos, vec.begin() + outPos + 2);
        if (inPos > outPos)
            inPos -= 2;
        outPos = -1;
    }   
}

// parses vector for > or < characters, handling redirection accordingly
void handleRedirection(std::vector<std::string>& vec)
{
    // parse vector to find position of < and >
    int inPos = -1;
    int outPos = -1;

    for(int i = 0; i < vec.size(); i++)
    {
        if (vec[i]=="<")
            inPos = i;
        if (vec[i]==">")
            outPos = i;
    }
    
    inputRedirect(vec, inPos, outPos);
    outputRedirect(vec, inPos, outPos);
}