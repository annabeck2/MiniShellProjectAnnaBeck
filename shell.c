#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>


// global define of buffer size (part2)
#define BUFFER_SIZE 80
#define MAX_COMMANDS 16
#define MAX_ARGS 32
#define MAX_HIST 1000

typedef void (*FUNC_PTR)(char **args);


typedef struct
{
	char command[80];
	FUNC_PTR func_ptr;	
	
} CommandArray;

// fork does not work for piping - everything const size arrays, all local vars, nothing on heap
// func for redirect calls - argv, output/input decriptor
// another func for two argv - then pipe it 


// history implementation


char *histArray[MAX_HIST];
int histCount = 0;

void addHist(char *arg) {
	int index = 0;
	if (histCount < MAX_HIST) {
		// malloc length of string and a '+ 1' for null character
		histArray[histCount] = (char *)malloc(strlen(arg) + 1);
		strcpy(histArray[histCount++], arg);
	}
	
	else {
		free(histArray[0]);
		for (index = 1; index < MAX_HIST; index++) {
			histArray[index - 1] = histArray[index];
		}
	        histArray[MAX_HIST - 1] = (char *)malloc(strlen(arg) + 1);
		strcpy(histArray[MAX_HIST - 1], arg);
	}	
}

void 
cd_function(char **argv)
{
//	printf("MY CD FUNCTION\n");
	// User typed just cd -- go to home directory
	if(argv[1] == NULL)
	{
		chdir(getenv("HOME"));
	}
	else
	{
		chdir(argv[1]);
	}
}

void 
help_function(char **argv)
{
//	printf("HELP FUNCTION\n");
	printf("command   -   history                 -  displays list of previous commands used\n");
	printf("command   -   cd 'location'           -  change the directory you are in\n");
	printf("command   -   exit                    -  quit the shell\n");
}

void 
exit_function(char **argv)
{
	exit(1);
}

void
history_function(char **argv) {
	int i = 0;
	for (i = 0; i < histCount; i++) {
		printf("%d   %s\n", i+1 , histArray[i]);
	}
}

// FUNCTION TEMP
void 
temp_function(char **argv)
{
	printf("TEMP FUNCTION\n");
}

CommandArray  commandArray[MAX_COMMANDS] = 
{
	{ "help", help_function },
	{ "exit", exit_function },
	{ "cd", cd_function },
	{ "history", history_function },
	{ "", temp_function }
};


// Signal Handler (part3)
void sigint_handler(int sig) {
	char *msg = "\nTerminating through signal handler\n";
	write(1, msg, strlen(msg)); 
	exit(0);
}

// Parse Function to get up to 2 commands based on |
void parseCommands(char* input, char **commands) 
{

	//called p_token - pointer to the token
	// each word is considered a token in our text
	char* p_token;
	int i = 0;
	
	// delimiter is a pipe
	p_token = strtok(input,"|");

	while (p_token != NULL)
	{
		commands[i++] = p_token;
        	p_token = strtok(NULL, "|");
	}
    	return;
}

// Parse Function to get all arguments of a command
void parseCommandArgs(char* command, char **args) 
{
	//called p_token - pointer to the token
	// each word is considered a token in our text
	char* p_token;
	int i = 0;
	
	// delimiter is a pipe
	p_token = strtok(command," \t");

	while (p_token != NULL)
	{
		args[i++] = p_token;
        	p_token = strtok(NULL, " \t");
	}

	args[i] = NULL; // indicates no more args exec likes that
    	return;
}

void fork_one_command(char **args)
{
	pid_t pid;

	pid = fork();

	if (pid == 0) {
		if (execvp(args[0], args) < 0) {
			printf("%s:  command not found\n", args[0]);
		}
		exit(1);
	}
	else if( pid < 0)
	{
		printf("System Errors Fork Failed\n");
	}
	else {
		wait(NULL);
	}
}

//forking with pipe
void fork_with_pipe(char** argsCommandOne, char **argsCommandTwo) {

	int status; // for waitpid

	pid_t pid1; // command one pid
	pid_t pid2; // command two (piped) pid
	int fd[2];  // pipe file descriptors

	// Create a pipe to pass output of first process
	// to input of second process
	// needs to be before fork 
	if (pipe(fd) < 0) { 
		printf("Severe ERROR pipe\n");
		exit(1);
	} 
  
	pid1 = fork(); 
	if (pid1 < 0) { 
		printf("Fork FAILURE - Severe"); 
		return; 
	} 

	if (pid1 == 0) {
		// Child write output to stdout for other process
		// stdin is a FILE *
		// dup2 takes an int file descriptor
		// use fileno to convert stdout to an int
		close(fd[0]);
		dup2(fd[1], fileno(stdout)); 
		close(fd[1]);
		if (execvp(argsCommandOne[0], argsCommandOne) < 0) { 
			printf("%s  command not found\n", argsCommandOne[0]); 
			exit(1); 
		} 
	} 
	else 
	{ 
		// start the second command
		pid2 = fork(); 
  
		if (pid2 < 0) { 
			printf("2nd Command Fork FAILURE - Severe"); 
			return; 
		} 
		if (pid2 == 0) { 
			// same conversion as above
			close(fd[1]);
			dup2(fd[0], fileno(stdin)); 
			close(fd[0]);
			if (execvp(argsCommandTwo[0], argsCommandTwo) < 0) { 
				printf("%s command not found\n", argsCommandTwo[0]); 
				exit(1); 
			} 
        	} 
		else 
		{ 
			close(fd[0]); 
  			close(fd[1]); 
			waitpid(pid1, &status, 0); 
			waitpid(pid2, &status, 0); 
		}
	}
}




// Handles the input (part4)
// return 1 if found any possible command
// zero if nothing all spaces
int handle_command(char *input) 
{
	int rc = 0;
	int i;
	int found_pipe = 0;
	char tmp_input[BUFFER_SIZE + 1];
	
	// arguments for a single non piped command
	char *myArgOne[MAX_ARGS];

	// arguments for the second piped command
	char *myArgTwo[MAX_ARGS];

	// handle up to 2 commands
	char *commands[2] = {"", ""};

	if(input[0] == 0)
		return rc;

	// break out commands into to their own strings
	// user input --> ls -ltr | more
	// .i.e commands[0] --> ls -ltr
	//      commands[1] --> more
	//using tmp_input parse will modify contents
	strcpy(tmp_input, input);
	parseCommands(tmp_input, commands);

	//printf("command[0]=%s\n", commands[0]);
	//printf("command[1]=%s\n", commands[1]);
	
	//parse a command based on spaces
	//.i.e --> ls -ltr
	//         myArgOne[0] --> ls
	//         myArgOne[1] --> -ltr
	//         myArgOne[2] --> NULL
	parseCommandArgs(commands[0], myArgOne);
	if(strcmp(commands[1], "") != 0)
	{
		found_pipe = 1;
		parseCommandArgs(commands[1], myArgTwo);
	}

	// Test code to print args
	/*
	int j = 0;	
	while(myArgOne[j])
	{
		printf("argOne %d --> %s\n", j, myArgOne[j]);
		j++;
	}
	j = 0;	
	while(myArgTwo[j])
	{
		printf("argTwo %d --> %s\n", j, myArgTwo[j]);
		j++;
	}
	*/

	// User entered spaces 
	if(!myArgOne[0])
		return rc;

	rc = 1;  // got something

	int my_command = 0;
	for(i = 0; i < MAX_COMMANDS; i++)
	{
		if(strcmp(myArgOne[0], commandArray[i].command) == 0)
		{
			commandArray[i].func_ptr(myArgOne);
			my_command = 1;
			break;
		}
	}
	if(my_command == 0)
	{
		// if no | - basic for and exec
		if(strcmp(commands[1], "") != 0)
		{
			// Two forks with pipe()/dup()
			fork_with_pipe(myArgOne, myArgTwo);			
		}
		else
		{
			fork_one_command(myArgOne);
		}
	}
	return rc;
}


int main(int argc, char** argv){

	char inputBuffer[BUFFER_SIZE];	
	
	// set the host name (part1)
	char hostName[BUFFER_SIZE];
	gethostname(hostName, BUFFER_SIZE);


	// Install the signal Handler
	signal(SIGINT, sigint_handler);
	
	// infinite loop
	while(1) {
		//minishell (part1) and prints hostName
		printf("[%s] mini-shell> ", hostName);
		fgets(inputBuffer, sizeof(inputBuffer), stdin);
		// strip off new line
		inputBuffer[strlen(inputBuffer) - 1] = 0;
		if(inputBuffer)
		{
			if(handle_command(inputBuffer) == 1)
			{
				addHist(inputBuffer);
			}
		}
	}

	//Call parse and pass in the array (inputBuffer)
	parse(inputBuffer);
	return 0;
}
