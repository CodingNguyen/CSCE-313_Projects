/*
    Cody Nguyen
    Department of Computer Science & Engineering
    Texas A&M University
    Date  : 9/4/20
 */
#include <errno.h>
#include <string.h>

//#include "shellHandling.cpp"
//#include "IORedirection.cpp"
#include "shellPipe.cpp"

int main()
{
    try
    {
        std::vector<int> BGprocesses; // vector to keep track of background processes

        char buf[256];
        getcwd(buf, 256);
        std::string prev = buf; // holds the last directory navigated to

        int backup = dup(0);
        while (true)
        {
            dup2(backup, 0);
            // check background processes
            backgroundWait(BGprocesses);

            std::cout << formatPrompt(); //print the prompt

            std::string inputline;
            std::getline(std::cin, inputline); //get a line from standard input
            if (inputline.length() > 0)
            {
                if (inputline == std::string("exit"))
                {
                    std::cout << "\033[;35mEnd of shell\033[0m" << std::endl;
                    break;
                }

                // splitting by commands by delimiter, '|', into a string vector
                std::vector<std::string> command = split(inputline, false, '|');

                if (command.size() == 1)
                {
                    // splitting input line by delimiter into a string vector
                    std::vector<std::string> splitInput = split(command[0], true);

                    // checks to see if command should run in background
                    bool performBG = checkBackgroundArg(splitInput);

                    // checks and changes the directory if cd was input
                    // forks and executes otherwise
                    if (!changeDirectory(splitInput, prev))
                    {
                        int pid = fork();
                        if (pid == 0)
                        { //child process

                            // handling IO Redirection
                            handleRedirection(splitInput);

                            // convert vector of input to char* array and pushing to exec
                            char **args = vecToCharPtrs(splitInput);
                            if (execvp(args[0], args) == -1)
                            {
                                // if child does not exec properly, error is outputted and child ends
                                std::string execErr = strerror(errno);
                                throw execErr;
                            }
                        }
                        else
                        {
                            if (!performBG)
                                waitpid(pid, 0, 0); //parent waits for child process
                            else
                            {
                                BGprocesses.push_back(pid); // special case for a command running in the background
                            }
                        }
                    }
                }
                else if (command.size() > 1)
                {
                    handleCommands(command);
                }
            }
        }
    }
    catch (std::string e)
    {
        std::cerr << "Error: " << e << std::endl;
    }
}