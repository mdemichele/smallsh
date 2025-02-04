/*******************************************************************
 * Title: smallsh						   *
 * Author: Matthew DeMichele					   *
 * Description: smallsh is an implementation of a basic shell      *
 * written in C.		 			 	   *
 * Created: 1 November 2021	                                   *
 * Last Updated: 26 January 2025				   *
 *******************************************************************/


#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>


/********************************************************************
* Global Variables
*********************************************************************/
int allowBackground = 1;

/*******************************************************************
* Command Structure: Struct definition
********************************************************************/
struct command {
	char *commandName;
	char *arguments[512];
	int numArgs;
	char *inputFile;
	char *outputFile;
	int backgroundFlag; 
};

/******************************************************************
* Running Processes: Struct definition to keep track of currently
*					 running processes
******************************************************************/
struct processes {
	pid_t currentProcesses[512];
	int numProcesses;
};

/******************************************************************
* Prototypes
*******************************************************************/
void handle_parent_SIGTSTP(int signo);
int check_for_skips(char *input);
void change_directories(struct command enteredCommand);
void display_status(int exitStatus);
struct command convert_input(char *input);
int execute_command(struct command enteredCommand, struct sigaction foreground_child_SIGINT, struct sigaction child_SIGTSTP, struct processes*);
void prompt_loop();


/******************************************************************
* Main Function 
*******************************************************************/
int main(int argc, char* argv[]) {
	// This is the main loop that runs the shell
	prompt_loop();

	return 0;
}


/******************************************************************
* Prompt Loop: Gets input from user 
*******************************************************************/
void prompt_loop() {
	// Initialize runningProcesses structure to keep track of currently running background processes 
	struct processes *runningProcesses = malloc(sizeof(struct processes));

	// Set initial values of running processes structure
	memset(runningProcesses->currentProcesses, '\0', 512);
	runningProcesses->numProcesses = 0;

	// Declare buffer variable to hold user input
	size_t bufferSize = 2050; // Maximum command line length is 2048 characters 
	char* buffer;
	buffer = (char *)malloc(bufferSize * sizeof(char)); // Allocate 2048 characters worth of memory space 

	/* Handle memory allocation error */
	if (buffer == NULL) {
		perror("Unable to allocate line buffer\n");
		exit(0);
	}

	/***************************************************************************************************
	* SIGINT Signal 1 (^C): Used by Foreground Child Processes																	
	* 	1. The shell, i.e. the parent process, must ignore SIGINT
	*	2. Any children running as background processes must ignore SIGINT
	*	3. A child running as a foreground process must terminate itself when it receives SIGINT
	*		3a. Parent must not attempt to terminate the foreground child process; instead, 
	*			the foreground child (if any) must terminate itself on receipt of this signal 
	*		3b. If a child foreground process is killed by a signal, the parent must immediately 
	*			print out the number of the signal that killed its foreground child process before 
	*			prompting the user for the next command. 
	***************************************************************************************************/
	struct sigaction foreground_child_SIGINT = {{0}}; // Initialize foreground_child_SIGINT struct to be empty
	foreground_child_SIGINT.sa_handler = SIG_DFL; // Register SIG_DFL as the signal handler. SIG_DFL allows default behavior for SIGINT signals 
	sigfillset(&foreground_child_SIGINT.sa_mask); // Block all catchable signals while sa_handler is running 
	foreground_child_SIGINT.sa_flags = SA_RESTART; // Set SA_RESTART flag  
	
	/**************************************************************************************************
	* SIGTINT Signal 2 (^C): Used by Parent and Background Processes
	**************************************************************************************************/
	struct sigaction parent_SIGINT = {{0}};		// Initialize parent_SIGINT structure to be empty 
	parent_SIGINT.sa_handler = SIG_IGN;			// Register SIG_IGN as signal handler. SIG_IGN ignores SIGINT signals 
	sigaction(SIGINT, &parent_SIGINT, NULL);	// Will be installed for parent and background child processes. Ignores SIGINT signals 

	/****************************************************************************************************
	* SIGTSTP Signal Handler (^Z) 1: Used By Parent Processes
	*	1. A child, if any, running as either a foreground or background process must ignore SIGSTSTP
	*	2. When the parent process running the shell received SIGTSTP, program does the following:
	*		2a. Shell must display an informative message immediately if it's sitting at the prompt, or 
	*			immeditately after any currently running foreground process has terminated.
	*		2b. The shell then enters a state where subsequent commands can no longer be run in the background
	*		2c. In this state, the & operater should simply be ignored, i.e. all such commands are run as if 
	*			they were foreground processes
	*****************************************************************************************************/
	struct sigaction parent_SIGTSTP = {{0}}; // Declare empty sigaction struct 
	parent_SIGTSTP.sa_handler = handle_parent_SIGTSTP; // Register handle_parent_SIGTSTP as the signal handler for parent processes
	sigfillset(&parent_SIGTSTP.sa_mask); // Set sa_mask: block certain signals 
	parent_SIGTSTP.sa_flags = SA_RESTART;  // Set SA_RESTART flag
	sigaction(SIGTSTP, &parent_SIGTSTP, NULL);  // Set the sigaction function to use the handle_parent_SIGTSTP structure we just initialized 

	/**************************************************************
	* SIGTSTP Signal Handler (^Z) 2: Used by Child Processes 
	* Description: This handler just ignores ^Z signals
	***************************************************************/
	struct sigaction child_SIGTSTP = {{0}}; 	// Initialize empty sigaction struct 
	child_SIGTSTP.sa_handler = SIG_IGN; 	   // Ignores ^Z signals 

	/**************************************************************
	* Useful Variables:
	*	1. continueLoop: used to run the main loop.
	*	2. exitStatus: used to print out the status
	***************************************************************/
	int continueLoop = 1;
	int terminationStatus = -5; // -5 means that no foreground process has run yet

	/**************************************************************
	* Main Loop: Continues as long as continueLoop variable
	*			 is set to 1  
	***************************************************************/
	while (continueLoop) {
		// Display prompt to user 
		printf(": ");
		fflush(stdout);

		// Accept input from user 
		getline(&buffer, &bufferSize, stdin);

		// Handle blank lines and comments 
		if (check_for_skips(buffer) == 1) {
			// Do Nothing 
		} else {
			// Break line into a command struct 
			// Will reset the newCommand structure every time through
			struct command newCommand = convert_input(buffer);

			// Check if command is "exit"
			if (strcmp(newCommand.commandName, "exit") == 0) {
				// Loop through the runningProcesses numProcesses array and kill any running processes
				for (int i = 0; i < runningProcesses->numProcesses; i++) {
					kill(runningProcesses->currentProcesses[runningProcesses->numProcesses], SIGTERM);
				}

				// Exit Loop
				continueLoop = 0;
			} 
			// Check if command is "cd"
			else if (strcmp(newCommand.commandName, "cd") == 0) {
				change_directories(newCommand);
			}
			// Check if command is "status"
			else if (strcmp(newCommand.commandName, "status") == 0) {
				display_status(terminationStatus);
			}
			// If not built-in, execute with command struct with "execute_command"
			else {
				terminationStatus = execute_command(newCommand, foreground_child_SIGINT, child_SIGTSTP, runningProcesses); 
			}
		}
	}
	
}

/******************************************************************
* Handle SIGTSTP signal
*******************************************************************/
void handle_parent_SIGTSTP(int signo) {

	// If background processes are currently allowed...
	if (allowBackground == 1) {
		// Display message about foreground-only mode
		char* display = "Entering foreground-only mode (& is now ignored)\n";
		write(1, display, 49);
		fflush(stdout);

		// reset allowBackground flag
		allowBackground = 0;
	} 
	// If background processes are NOT currently allowed...
	else {
		// Display message about leaving foreground-only mode
		char* display = "Exiting foreground-only mode\n";
		write(1, display, 30);
		fflush(stdout);

		// set allowBackground flag to 1
		allowBackground = 1;
	}
}


/******************************************************************
* Check For Skips: check for situations that should be skipped. 
*	1. Blank lines 
*	2. comment lines 
*******************************************************************/
int check_for_skips(char *input) {
	// Use these two variables to check for blank lines and comments 
	char blankMarker = 10;
	char commentMarker = '#';
	
	// Get the first character of the input string 
	char firstCharacter = input[0];
	
	// This line check for 1.) Whole line blank 2.) first character equals #  
	if ((*input == blankMarker) || (firstCharacter == commentMarker)) {
		return 1;
	} else {
		return 0;
	} 
}


/*******************************************************************************
* Change Directories: Executes the cd command 
* 	Rule 1. If no arguments, change to the directory specified in HOME 
*			environment variable. 
*
*	INPUT:  
		struct command enteredCommand	 The path of the directory to change to
*
********************************************************************************/
void change_directories(struct command enteredCommand) {
	// If no arguments specified, change to home directory 
	if (enteredCommand.arguments[1] == NULL) {
		chdir(getenv("HOME"));
	} 
	// If path specified, change to path
	else {
		int cdResult = chdir(enteredCommand.arguments[1]);
		// Send error message if cd doesn't work 
		if (cdResult == -1) {
			printf("No such directory exists.\n");
			fflush(stdout);
		}
	}
}


/******************************************************************************
* Display Status: Executes the status command
*******************************************************************************/
void display_status(int terminationStatus) {
	// Initialize buffer for status output 
	int messageLength = 0;
	char statusMessage[100];
	memset(statusMessage, '\0', 100);

	// If exitStatus == -5, then no foreground process has run yet
	if (terminationStatus == -5) {
		write(1, "exit value 0\n", 13);
		fflush(stdout);
	}
	// If last process was terminated normally, then print exit status
	else if (WIFEXITED(terminationStatus)) {
		sprintf(statusMessage, "exit status %d\n", WEXITSTATUS(terminationStatus));
		messageLength = strlen(statusMessage);
		write(1, statusMessage, messageLength);
		fflush(stdout);
	} 
	// If last process was terminated because of a signal, print signal
	else if (WIFSIGNALED(terminationStatus)) {
		sprintf(statusMessage, "terminated by signal %d\n", WTERMSIG(terminationStatus));
		messageLength = strlen(statusMessage);
		write(1, statusMessage, messageLength);
		fflush(stdout);
	} 
	else {

	}
}


/********************************************************************************
* Convert Input: Converts the input entered by the user into a command structure 
*********************************************************************************/
struct command convert_input(char *input) {
	// Create a new struct variable 
	struct command currCommand;

	// Fill arguments array with Null values. Will erase any arguments preivously used if any 
	for (int i = 0; i < 512; i++) {
		currCommand.arguments[i] = NULL;
	}

	// Reset inputFile and outputFile
	currCommand.inputFile = (char*)NULL;
	currCommand.outputFile = (char*)NULL;
	currCommand.backgroundFlag = 0;
	currCommand.numArgs = 0;

	// Will use numArgs to keep track of current index of arguments array 
	int numArgs = 0;

	// Get the pid and convert to string for use in expanding variable $$
	pid_t currpid = getpid();
	char *pidstring = malloc(13);
	snprintf(pidstring, 13, "%d", currpid);

	// Use convertedString in variable $$ expansion
	char *convertedString;
	int converted = 0;

	// Declare variables to use with strtok_r and parsing the input string
	char *saveptr = input;
	char *readString;
	char *token;
	int iterator;

	// Use these flags for determining how to parse the string 
	int setCommand = 0;
	int setArgs = 0;
	int setInput = 0;
	int setOutput = 0;
	int setBackground = 0;

	// Loop through input and parse the input string 
	for (iterator = 1, readString = input; iterator < 100; iterator++, readString = NULL) {
		// Get token from readString, using space as the delimiter
		token = strtok_r(readString, " \n\t\r\a", &saveptr);

		// CHECK 01. Break out of for loop if you reach the end of the line 
		if (token == NULL) {
			break;
		}

		// CHECK 02. Set background flag if token is &
		if (*token == 38) {
			setBackground = 1;
			setOutput = 1;
			setInput = 1;
			setArgs = 1;
		} 

		// CHECK 03. Set output flag if token is >
		if (*token == 62) {
			setOutput = 1;
			setInput = 1;
			setArgs = 1;
		}

		// CHECK 04. Set input flag if token is <
		if (*token == 60) {
			setInput = 1;
			setArgs = 1;
		}

		// SET 01. Set currCommand background flag if setBackground flag is set 
		if ( (setBackground == 1) && (setOutput == 1) && (setInput == 1)) {
			currCommand.backgroundFlag = 1;
		}

		// SET 02. Set currCommand output file if setOutput flag is set 
		if ( (setOutput == 1) && (setInput == 1) && (setBackground == 0) && (*token != 62) ) {
			currCommand.outputFile = calloc(strlen(token) + 1, sizeof(char));
			strcpy(currCommand.outputFile, token);
		}

		// SET 03. Set currCommand input file if setInput flag is set
		if ( (setInput == 1) && (setOutput == 0) && (setBackground == 0) && (*token != 60) ) {
			currCommand.inputFile = calloc(strlen(token) + 1, sizeof(char));
			strcpy(currCommand.inputFile, token);
		}

		// SET 04. Set args of currCommand if setBackground, setOutput, and setArgs flag NOT set 
		if ( (setBackground != 1) && (setOutput != 1) && (setArgs != 1)) {
			
			// Check the token string for any $$ variables 
			for (int i = 0; i < strlen(token) - 1; i++) {
				if ( token[i] == '$' && token[i + 1] == '$' ) {
					// Declare a variable to hold what comes before the $$ variable 
					convertedString = malloc(strlen(token) + 14);

					// copy the first part of token into firstHalf 
					strncpy(convertedString, token, i);

					// concat the pid and firstHalf string together 
					strcat(convertedString, pidstring);

					// Set converted flag
					converted = 1;
					
				}
			}

			// Use convertedString if a variable was found, use regular token if not
			if (converted == 1) {
				currCommand.arguments[numArgs] = strdup(convertedString);
			} else {
				currCommand.arguments[numArgs] = strdup(token);
			}
			
			numArgs += 1;
		}

		// SET 05. Set commandName of currCommand if no flags set yet 
		if (setCommand == 0) {
			currCommand.commandName = calloc(strlen(token) + 1, sizeof(char));
			// Check the token string for any $$ variables 
			for (int i = 0; i < strlen(token) - 1; i++) {
				if ( token[i] == '$' && token[i + 1] == '$' ) {
					// Declare a variable to hold what comes before the $$ variable 
					convertedString = malloc(strlen(token) + 14);

					// copy the first part of token into firstHalf 
					strncpy(convertedString, token, i);

					// concat the pid and firstHalf string together 
					strcat(convertedString, pidstring);

					// Set converted flag
					converted = 1;
				}
			}

			// Use convertedString if a variable was found, use regular token if not
			if (converted == 1) {
				strcpy(currCommand.commandName, convertedString);
			} else {
				strcpy(currCommand.commandName, token);
			}

			setCommand = 1;
		}
	}

	// Set the total number of arguments 
	currCommand.numArgs = numArgs;

	// Return newly created struct pointer 
	return currCommand;
}


/**********************************************************************************
* Execute Command: Executes any command not built-in to the shell 
***********************************************************************************/
int execute_command(struct command enteredCommand, struct sigaction foreground_child_SIGINT, struct sigaction child_SIGTSTP, struct processes* runningProcesses) {
	/*********** Initialize necessary variables ********************/
	pid_t childPid = -5;	// Used to keep track of current childPid
	int input = 0;			// Used for redirecting input 
	int inputResult = 0;	// Also used for redirecting input
	int output = 0;			// Used for redirecting output
	int outputResult;		// Also used for redirecting output
	int childExitStatus = 0; // Used for printing the child exit status
	int lastForegroundStatus = 0;						// Used for storing the last foregound child exit status
	char *signalMessage = "terminated by signal ";		// Used to display termination signal message
	char statusMessage[100];							// Used to display exit status message
	int statusLength = 0;

	// fork the running process 
	childPid = fork();

	// CHILD PROCESS 
	if (childPid == 0) {

		// Install child_SIGTSTP signal handler, so that all child processes ignore ^Z signals 
		sigaction(SIGTSTP, &child_SIGTSTP, NULL);

		// Child processes running in foreground terminate themselves when they receive SIGINT 
		// Child processes running as a background command continue to ignore SIGINT
		if (enteredCommand.backgroundFlag == 0) {
			sigaction(SIGINT, &foreground_child_SIGINT, NULL);
		} 

		// Handle input file redirection if any
		if (enteredCommand.inputFile != NULL) {
			// Open input file for read only 
			input = open(enteredCommand.inputFile, O_RDONLY);

			// If shell can't open file for reading, print error message and set exit status to 1 
			if (input == -1) {
				perror("Smallsh can't open file\n");
				exit(1);
			}

			// Redirect stdin to input file 
			inputResult = dup2(input, 0);

			// If shell can't assign input, print error message and set exit status to 1
			if (inputResult == -1) {
				perror("Smallsh can't redirect input to input file\n");
				exit(1);
			}

		}

		// Handle output file redirection if any
		if (enteredCommand.outputFile != NULL) {
			// Open output file for writing only. Truncate if it already exists. Create if it doesn't exist.
			output = open(enteredCommand.outputFile, O_WRONLY | O_TRUNC | O_CREAT, 0666);

			// If shell can't open file for writing, print error message and set exit status to 1
			if (output == -1) {
				perror("Smallsh can't open file.\n");
				exit(1);
			}

			// Rediect stdout to output file 
			outputResult = dup2(output, 1);

			// If shell can't assign output, print error message and set exit status to 1
			if (outputResult == -1) {
				perror("Smallsh can't redirect output to output file.\n");
				exit(1);
			}

		}

		// If no input specified for a background command, handle input redirect 
		if ( (enteredCommand.backgroundFlag == 1) && (allowBackground == 1) && (enteredCommand.inputFile == NULL) ) {
			// Open /dev/null for writing only 
			input = open("/dev/null", O_RDONLY);

			// If shell can't open file for reading, print error message and set exit status to 1 
			if (input == -1) {
				perror("Smallsh can't open file\n");
				exit(1);
			}

			// Redirect stdin to input file 
			inputResult = dup2(input, 0);

			// Close input file?
		}

		// If not output specified for a background command, handle output redirect 
		if ( (enteredCommand.backgroundFlag == 1) && (allowBackground == 1) && (enteredCommand.outputFile == NULL) ) {
			// Open dev/null for writing only. Truncate if it exists. Create if it doesn't exist.
			output = open("/dev/null", O_WRONLY | O_TRUNC | O_CREAT, 0666);

			// If shell can't open file for writing, print error 
			if (output == -1) {
				perror("Smallsh can't open file.\n");
				exit(1);
			}

			// Redirect stdout
			outputResult = dup2(output, 1);

			// If shell can't assign output, print error message and set exit status to1
			if (outputResult == -1) {
				perror("Smallsh can't redirect output to output file.\n");
				exit(1);
			}
		}

		// Execute the Child process
		if (execvp(enteredCommand.arguments[0], (char* const*)enteredCommand.arguments) == -1) {
			perror("There was an error");
			exit(1);
		}
		exit(0);
	} 
	// ERROR HANDLING
	else if (childPid < 0) {
		// Error forking 
		perror("Error forking");
		exit(1);
	} 
	// PARENT PROCESS
	else {
		// Execute process in the background if backgroundFlag is set and background mode is enabled
		if ( (enteredCommand.backgroundFlag == 1) && (allowBackground == 1) ) {
			waitpid(childPid, &childExitStatus, WNOHANG);
			printf("background pid is: %d\n", childPid);
			fflush(stdout);

			// add the running background process to the running processes structure 
			runningProcesses->currentProcesses[runningProcesses->numProcesses] = childPid;
			runningProcesses->numProcesses += 1;
		} 
		// Execute process in the foreground if backgroundFlag not set 
		else {
			childPid = waitpid(childPid, &childExitStatus, 0);

			// Check for any foreground child process that terminate due to a SIGINT signal 
			if (WIFSIGNALED(childExitStatus)) {
				write(1, signalMessage, 24);
				fflush(stdout);
			}

			// Store the exit status of most current child process 
			lastForegroundStatus = childExitStatus;
		}

		// Check for any background child process that finish
		while ( (childPid = waitpid(-1, &childExitStatus, WNOHANG)) > 0) {
			// Write output into a string, and then write to stdout
			sprintf(statusMessage, "background pid %d is done: ", childPid);
			statusLength = strlen(statusMessage);
			write(1, statusMessage, statusLength);
			fflush(stdout);

			// Display the exit status 
			display_status(childExitStatus);
		}
	}
	return lastForegroundStatus;
}
