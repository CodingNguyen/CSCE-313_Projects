#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "shellHandling.cpp"
#include "IORedirection.cpp"

// Handles commands given a string of inputs
void handleCommands(std::vector<std::string> &command)
{
    for (int i = 0; i < command.size(); i++)
    {
        // splitting input line by delimiter into a string vector
        std::vector<std::string> splitInput = split(command[i]);
        // set up pipe
        int fd[2];
        pipe(fd);

        int pid = fork();
        if (pid == 0)
        { //child process

            // handling IO Redirection
            handleRedirection(splitInput);

            // redirect output here to next level except for last level
            if (i < command.size() - 1)
                dup2(fd[1], 1); // redirect standard out to FD[1]

            // convert vector of input to char* array and pushing to exec
            char **args = vecToCharPtrs(splitInput);
            if (execvp(args[0], args) == -1)
            {
                // if child does not exec properly, error is thrown
                std::string execErr = strerror(errno);
                throw execErr;
            }
        }
        else
        {
            if (i == command.size() - 1)
                waitpid(pid, 0, 0);      //parents waits for child process

            // redirect input from child to parent
            dup2(fd[0], 0); // redirect standard in to FD[0]
            close(fd[1]);   // close, so next level does not wait
        }
    }
}