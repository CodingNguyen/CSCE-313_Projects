#include <chrono>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// formats a prompt and returns as a string
std::string formatPrompt()
{
    std::string user = getenv("USER");
    // calculating current time and removing newline at the end of time
    auto currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::string time = ctime(&currentTime);
    time.pop_back();

    std::string prompt = "\033[;31m" + user + "\033[0m(" + time + "):~";

    // getting current working directory and adding it to prompt
    char buf[256];
    getcwd(buf, 256);
    prompt = prompt + "\033[;34m" + buf + "\033[0m" + "$ ";

    return prompt;
}

// convert vector to an array of char pointers
char **vecToCharPtrs(std::vector<std::string> &vec)
{
    // creating array of char pointers size of vector vec + 1 (to account for null terminated string)
    char **converted = new char *[vec.size() + 1];

    // iterate through vector vec, coneverting each string to c-strings and adding it to converted
    for (int i = 0; i < vec.size(); i++)
        converted[i] = (char *)vec[i].c_str();
    // make sure the end of the char* array is null terminated
    converted[vec.size()] = nullptr;

    return converted;
}

// remove extraneous whitespace from a string
std::string trim(std::string str)
{
    int first = str.find_first_not_of(" ");
    int last = str.find_last_not_of(" ");
    return str.substr(first,last-first+1);
}

// remove extraneous quotes from a string
void removeQuotes(std::string& str)
{
    if (str.length() > 2)
    {
        if (str.at(0) == '\"' && str.at(str.length()-1) == '\"')
            str = str.substr(1,str.length()-2);
        else if (str.at(0) == '\'' && str.at(str.length()-1) == '\'')
            str = str.substr(1,str.length()-2);
    }
}

// splits the input string into vector based on demiliter, whitespace by default
// if change quotes is true, quoted text is removed and added to the end of the vector
std::vector<std::string> split(std::string str, bool changeQuotes = false, char delimiter = ' ')
{
    std::vector<std::string> splitInput;

    int i = 0;
    int start = 0;

    std::string temp;

    bool inQuote = false;
    bool quoted = false;
    while (i < str.length())
    {
        if (str.at(i) == '\"' || str.at(i) == '\'')
        {
            inQuote = !inQuote;
            quoted = true;
        }

        if (str.at(i) == delimiter && inQuote==false)
        {
            temp = trim(str.substr(start, i - start));
            removeQuotes(temp);
            splitInput.push_back(temp);
            start = i + 1;
        }
        i++;
    }
    temp = trim(str.substr(start, i - start));
    removeQuotes(temp);
    splitInput.push_back(temp);

    return splitInput;
}

// handle directory changing, returns true if directory was changed
bool changeDirectory(std::vector<std::string> splitInput, std::string& prev)
{
    if (splitInput.size() > 0 && splitInput[0] == std::string("cd"))
    {
        if (splitInput.size() == 2)
        {
            std::string dir = splitInput[1];
            if(splitInput[1] == "-")
                dir = prev;

            char buf[256];
            getcwd(buf, 256);
            prev = buf;

            if(chdir((char *)dir.c_str()) != 0)
                perror("Error");
            else
                return true;
        }
        else if (splitInput.size() > 2)
        {
            std::cerr << "Error: Too many arguments\n";
        }
        else
        {
            chdir("");
            return true;
        }
    }
    return false;
}

// checks for & at end of string vector, removes it and returns true if present, false otherwise
bool checkBackgroundArg(std::vector<std::string> &vec)
{
    if (vec.size()-1 != 0 && vec[vec.size() - 1] == "&")
    {
        vec.pop_back();
        return true;
    }
    return false;
}

// keeps track of child processes so that zombies are removed before parent exit
void backgroundWait(std::vector<int>& BGprocesses)
{
    for(int i = 0; i < BGprocesses.size(); i++)
        {
            // check PID status and returns immediately if no child exited
            if (waitpid(BGprocesses[i], 0, WNOHANG) == BGprocesses[i])
            {
                std::cout << "Finished Process: " << BGprocesses[i] << std::endl;
                // stops tracking if child exited
                BGprocesses.erase(BGprocesses.begin() + i);
                i--; // keep increment at same spot
            }
        }
}