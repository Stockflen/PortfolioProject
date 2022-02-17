#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include "functions.h"

#define MAX_ARGS 512
#define MAX_BUF 200

//Setting a flag to check if the user should be prompted for input
bool flag = true;

//Setting a flag to see if terminal stop has been set
bool termStop = false;

//Creating a variable for the status to be output
int outputStatus;

//Initializing SIGINT_action and SIGTSTP structs to be empty
struct sigaction SIGINT_action = {0};
struct sigaction SIGTSTP_action = {0};

//Creating a struct to use for the different parts of the users input string
//COMMAND - Will hold the main command
//ARGS[] - Holds up to 512 supplemental arguments
//INPUTFILE - Stores the file used for stdin
//OUTPUT -  Stored the file used for stdout
//BG - Flag to indicate the presences ampersand
struct userCommand{

    char *command;
    char *args[MAX_ARGS];
    char *inputFile;
    char *outputFile;
    bool bg;

};

//Creating a function that parses the users input into the userCommand struct
struct userCommand *createCommand(char *currLine){
    //Constants used to signify stdin/stdout redirection, the end of a line or a background direction
    const char *nullTerm = "\n";
    const char *output = ">";
    const char *input = "<";
    const char *background = "&";
    int i = 0;

    //A struct to hold the current command
    struct userCommand *currCommand = malloc(sizeof(struct userCommand));

    //Initializing the argument array
    for(int x = 0; x < MAX_ARGS; x++){
        currCommand->args[x] = NULL;
    }


    char *saveptr;
    //Tokenizing the current line
    char *token = strtok_r(currLine," ",&saveptr);
    currCommand -> command = calloc(strlen(token)+1,sizeof(char));
    strcpy(currCommand->command,token);

    //Adding the command 
    currCommand->args[i] = calloc(strlen(token)+1,sizeof(char));
    strcpy(currCommand->args[i],token);
    i++;
    

    token = strtok_r(NULL," \n",&saveptr);

    //If the user has entered arguments to the command, they are added to the struct
    while(token != NULL && i < MAX_ARGS){

        //If its the end of the line there are no more arguments
       if(strcmp(token,nullTerm) == 0){
           break;
       }

       //If a < is present, the proceeding token is saved as the inputFile
        else if(strcmp(token,input) == 0){
           token = strtok_r(NULL," \n",&saveptr);
           currCommand->inputFile = calloc(strlen(token)+1,sizeof(char));
           strcpy(currCommand->inputFile,token);
        } 

        //If a > is present the proceeding token is saved as the outputFile
        else if(strcmp(token,output) == 0){
           token = strtok_r(NULL," \n",&saveptr);
           currCommand->outputFile = calloc(strlen(token)+1,sizeof(char));
           strcpy(currCommand->outputFile,token);
        }

        //If an ampersand is present, the background boolean is toggled to true
        else if(strcmp(token,background) == 0){
           currCommand->bg = true;
        }

        //If none of the special tokens are found, the token is added to the argument array
       else{
            currCommand->args[i] = calloc(strlen(token)+1,sizeof(char));
            strcpy(currCommand->args[i],token);
            i++;
       }
       token = strtok_r(NULL," \n",&saveptr);
    }

    //NULL is added to the end of the args array to satisfy the execvp call
    currCommand->args[i] = (char *) NULL;

    return currCommand;
}

//Function used to handle the built in commands - exit, cd and status
void useBuiltin(struct userCommand *currCommand){
    const char *exit = "exit";
    const char *status = "status";
    char path[MAX_BUF];

    //This code is adapted from the code provided in the exploration - Process API - Monitoring Child Processes
    //If the command is 'status' the value of the status is printed out. 
    //If the command is 'exit' the program exits 
    //Otherwise the directory is changed to either 'home' or the desired path
    if(strcmp(currCommand->command,status) == 0){
       printf("exit value %d\n",outputStatus); 
       fflush(stdout);
    }
    else if(strcmp(currCommand->command,exit) == 0){
        flag = false;
        return;
    }
    else{
        if(currCommand->args[1] == NULL){
            chdir(getenv("HOME"));
        }
        else{
            chdir(currCommand->args[1]);
        }
        getcwd(path,MAX_BUF);
    }

}

//This function processes all commands that are not handled in useBuiltIn
void otherCommands(struct userCommand *currCommand){
    //Variables to hold the status of a child process and the foreground pid
    int childStatus;
    pid_t fgpid;
 
    //Creating a fork
    pid_t spawnPid = fork();

    //A switch statement to execute new processes
    //Case -1 handles fork failure

    //Case 0 handles a child process
    //Ctrl C is set to its default value for child processes
    //If there is and input/output file the stdin/stdout is redirected
    //execvp is called using the command and user provided arguements
    //If execvp fails the user is notified
    
    //Default case handles the parent processes
    //If the bg flag is set and terminal stop is not, a background process is initiated
    //Otherwise a forground process is intiated
    //The exit and terminate signals are set

    switch(spawnPid){
        case -1:
            perror("fork()\n");
            fflush(stdout);
            exit(1);
            break;
        case 0:
            SIGINT_action.sa_handler = SIG_DFL;
            sigaction(SIGINT,&SIGINT_action,NULL);
            //The following two sections were adapted from the code provided in the class explorations
            if(currCommand->outputFile != NULL){
                int output_fd = open(currCommand->outputFile, O_WRONLY | O_CREAT | O_TRUNC,0600);
                if(output_fd == -1){
                    perror("Error");
                    fflush(stdout);
                    exit(1);
                }
                dup2(output_fd,STDOUT_FILENO);
            }
            if(currCommand->inputFile != NULL){
                int input_fd = open(currCommand->inputFile, O_RDONLY,0600);
                if(input_fd == -1){
                    perror("Error");
                    fflush(stdout);
                    exit(1);
                }
                dup2(input_fd,STDIN_FILENO);
            }
            if(execvp(currCommand->command,&(currCommand->args[0]))){
                perror(currCommand->command);
                fflush(stdout);
                exit(2);
            }
            break;
        default:
            fgpid = spawnPid;
            if(currCommand->bg == 1 && termStop == 0){
                spawnPid = waitpid(fgpid, &childStatus, WNOHANG);
                printf("background pid is %d\n",fgpid);
                fflush(stdout);
            }
            else{
                spawnPid = waitpid(fgpid, &childStatus, 0);
                if(WIFEXITED(childStatus)){
		            outputStatus = WEXITSTATUS(childStatus);
	            } else{
		            outputStatus = WTERMSIG(childStatus);
	            }
            }

        //A while loop used to collect zombies
        //The exit and terminate signals are saved    
        while((spawnPid = waitpid(-1,&childStatus,WNOHANG)) > 0){
            if(WIFEXITED(childStatus)){
		        printf("background pid %d is done: exit value %d\n",spawnPid, WEXITSTATUS(childStatus));
                fflush(stdout);
	        } else{
		        printf("background pid %d is done: terminated by signal %d\n",spawnPid, WTERMSIG(childStatus));
                fflush(stdout);
	        }
            }
    }

}

//A functions used to determine whether a builtin comment, comment or other command was entered
void processCommand(struct userCommand *currCommand){
    const char *space = "\n";
    const char *comment = "#";
    const char *changeDir = "cd";
    const char *exit = "exit";
    const char *status = "status";
    
    //Comparing the command to either a blank line or a comment
    //If it is either, then the user is reprompted to enter input
    //If cd, exit or status are entered, useBuiltin function is called
    //Otherwise otherCommands function is called
    if( (strcmp(currCommand->command,space) == 0) | 
    (strncmp(currCommand->command,comment,1) == 0)){
        getInput();
    }
    else if((strcmp(currCommand->command,changeDir) == 0) |
    (strcmp(currCommand->command,exit) == 0) | 
    (strcmp(currCommand->command,status) == 0)){
        useBuiltin(currCommand);
    }
    else{
        otherCommands(currCommand);
    }
}

//Creating a function that goes back and frees the allocated memory for the struct
void freeCommand(struct userCommand *currCommand){

    //Freeing the command memory, args memory and struct memory
    free(currCommand->command);
    int i = 0;
    while(currCommand->args[i] != NULL){
      free(currCommand->args[i]);
      i++;
    }
    free(currCommand->inputFile);
    free(currCommand->outputFile);
    free(currCommand);

}

//A function used to check if variable expansion is needed
char* checkExpansion(char *inputString){
    const char expandVars[] = "$$";
    int pid = getpid();
    char replace[200];
    char *ptr;

    //pid is turned into a string
    sprintf(replace,"%d",pid);

    //the inputString is compared to the expansion variables
    //A ptr is set to the location is the variables are found
    ptr = strstr(inputString,expandVars);

    //If the pointer is found, the location is replaced with the pid
    if(ptr){
        strcpy(ptr,replace);
    }
    
    return inputString;

}

//A function used to handle the SIGTSTP signal
void handle_SIGTSTP(int signo){
    char backgroundOff[] = "Entering foreground-only mode (& is now ignored)\n";
    char backgroundOn[] = "Exiting foreground-only mode\n";
    //If the signal is set the user is advised and the background function is turned off
    //The flag is toggled back on
    if(termStop == 0){
        write(STDOUT_FILENO,backgroundOff,sizeof backgroundOff-1);
        tcflush(1, TCIOFLUSH);
        termStop = 1;
    }
    //If the signal is not set then the user is notifed the background function is turned back on
    //The flag is toggled off
    else{
        write(STDOUT_FILENO,backgroundOn,sizeof backgroundOn -1);
        tcflush(1, TCIOFLUSH);
        termStop = 0;
    }
}

//The following function prompts the user for input using the ':'
int getInput(){

    char* buffer;
    char* command;
    //setting the buffer size to 2048 characters per assignment specifications
    size_t bufsize = 2048;
    struct userCommand *newCommand;


    //The following two SIG directions were adapted from the code provided in the explorations
    //SIGINT is initialized to ignore ctrl C
    //SIGTSTP is used to activate or deactivate background processes
    SIGINT_action.sa_handler = SIG_IGN;
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGINT,&SIGINT_action,NULL);

    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigemptyset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;
    sigaction(SIGTSTP,&SIGTSTP_action,NULL);


    //allocating memory for the size of the user input string
    buffer = (char*)malloc(bufsize*sizeof(char));
    //advising user unable to allocate memory
    if(buffer == NULL){
        perror("Unable to allocate buffer");
        exit(1);
    }

    //As long is the flag is not set, the user is prompted for input
    //The /n is stripped off the user input and then checked for expansion
    //Input is then added to the struct and processed for further direction
    while(flag){
        printf(":");
        getline(&buffer,&bufsize,stdin);
        strtok(buffer,"\n");
        command = checkExpansion(buffer);
        newCommand = createCommand(command);
        processCommand(newCommand);
    }

    //Memory is freed
    freeCommand(newCommand);
    free(buffer);
    return(0);
}
