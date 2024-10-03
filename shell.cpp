#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <ctime>
#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    // use dup(...) to perserve standard in and out
    FILE* stdin = fdopen(dup(0), "r");
    FILE* stdout = fdopen(dup(1), "w");


    // get the name of the user 
    //      use getenv(...)
    char* user = getenv("USER");


    // get the previous working directory
    //      use getcwd(...)
    char cwd[1024];
    string prev_cwd;
    getcwd(cwd, sizeof(cwd));

    // create an array or vector to store the pids of the background processes
    vector<pid_t> pid_back;

    // create an array or vector to store the pids of interrmediate processes
    vector<pid_t> pid_inter;

    while(true) {
        // need date/time, username, and absolute path to current dir
        time_t curr_time = time(nullptr);
        char* time_str = ctime (&curr_time);
        cout << YELLOW << time_str << " "<< user << ":"<<cwd <<"Shell$" << NC;
        
        // get user inputted command
        string input;
        getline(cin, input);

        // reap background processes
        for (auto pid = pid_back.begin(); pid != pid_back.end();){
            if (waitpid(*pid, nullptr, WNOHANG) > 0){ // using WNOHANG makes it not wait
                cout << GREEN << "Reaped background process with PID: " << *pid << NC << endl;    //print output
                pid = pid_back.erase(pid);
            }
            else{
                pid++;
            }
        }
        for (auto pid = pid_inter.begin(); pid != pid_inter.end();){
            if (waitpid(*pid, nullptr, WNOHANG) > 0){ // using WNOHANG makes it not wait
                cout << GREEN << "Reaped background process with PID: " << *pid << NC << endl;    //print output
                pid = pid_inter.erase(pid);
            }
            else{
                pid++;
            }
        }
        if (input == "exit") { 
            // print exit message and break out of infinite loop
            // reap all remaining background processes
            //  similar to previous reaping, but now wait for all to finish
            //      in loop use argument of 0 instead of WNOHANG

            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            for(pid_t pid : pid_back){
                waitpid(pid, nullptr, 0);
            }
            break;
        }

        // [optional] do dollar sign expansion

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()) {  // continue to next prompt if input had an error
            continue;
        }

            // print out every command token-by-token on individual lines
            // prints to cerr to avoid influencing autograder
        //for (auto cmd : tknr.commands) {
        //    cerr<< "Command: ";
        //    for (auto str : cmd->args) {
        //        cerr << "|" << str << "| ";
        //    }
        //    if (cmd->hasInput()) {
        //        cerr << "in< " << cmd->in_file << " ";
        //    }
        //    if (cmd->hasOutput()) {
        //        cerr << "out> " << cmd->out_file << " ";
        //    }
        //    cerr << endl;
        //}

        // handle directory processing
        if (tknr.commands.size() == 1 && tknr.commands.at(0)->args.at(0) == "cd") {
            // use getcwd(...) and chdir(...) to change the path
            // if the path is "-" then print the previous directory and go to it
            // otherwise just go to the specified directory
            const string& path = tknr.commands.at(0)->args.at(1);
            if(path == "-"){
                cout << prev_cwd << endl;
                chdir(prev_cwd.c_str());
            }
            else{
                prev_cwd = cwd;
                if(chdir(path.c_str())==0){
                    getcwd(cwd, sizeof(cwd));
                }
                else{
                    perror("cd failed");
                }
            }
            continue;
        }

        // loop over piped commands
        for (size_t i = 0; i < tknr.commands.size(); i++) {
            // get the current command
            Command* currentCommand = tknr.commands.at(i);

            // pipe if the command is piped
            //  (if the number of commands is > 1)
            // fork to create child
            pid_t pid = fork();
            if (pid < 0) {  // error check
                perror("fork");
                exit(2);
            }

            if (pid == 0) {  // if child, exec to run command
                // run single commands with no arguments
                // change to instead create an array from the vector of arguments
                vector<char*> args;
                for(auto& arg : currentCommand->args){
                    args.push_back((char*)arg.c_str());                    
                }
                args.push_back(nullptr);

                // duplicate the standard out for piping

                // do I/O redirection  
                // the Command class has methods hasInput(), hasOutput()
                //     and member variables in_file and out_file

                if(currentCommand->hasInput()){
                    freopen(currentCommand->in_file.c_str(), "r", stdin);
                }
                else if(currentCommand->hasOutput()){
                    freopen(currentCommand->out_file.c_str(), "w", stdout);
                }
                
                if (execvp(args[0], args.data()) < 0) {  // error check
                    perror("execvp");
                    exit(2);
                }
            }
            else {  // if parent, wait for child to finish
                // if the current command is a background process then store it in the vector
                //  can use currentCommand.isBackground()

                // else if the last command, wait on the last command pipline
                int status = 0;
                if(!currentCommand->isBackground()){
                    waitpid(pid, &status, 0);
                    if (status > 1) {  // exit if child didn't exec properly
                        exit(status);
                    }
                    // else add the process to the intermediate processes
                    }
                    else{
                        pid_back.push_back(pid);
                    }
                // dup standardin
                pid_inter.push_back(pid);
            }
        }

        // restore standard in and out using dup2(...)
        dup2(fileno(stdin), 0);
        dup2(fileno(stdout), 1);

        // print out the reaped background processes

        // reap intermediate processes
    }
    fclose(stdin);
    fclose(stdout);
    return 0;
}
