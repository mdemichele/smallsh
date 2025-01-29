# smallsh
Implementation of a small shell program in C

# Project Description 
The project implements a subset of features of well-known shells, such as bash. The project was originally built for CS 344 - Operating Systems I. Now, I just maintain this for fun, adding features occasionally that I think would be fun to use. Among other things, the project implements the following features: 

- Provides a prompt for running commands 
- Handles blank lines and comments (i.e. lines beginning with the # character)
- Provides expansion for the variable $$ 
- Executes 3 commands (exit, cd, and status) via code built directly into the shell 
- Executes all other commands by creating new processes using the execvp function 
- Supports input and output redirection 
- Supports running commands in foreground and background processes 
- Implements custom handlers for the 2 signals, SIGINT and SIGTSTP 

# Concepts Demonstrated 
This project demonstrates understanding of a number of concepts related to Unix Operating Systems:

- usage of the Unix process API
- ability to write programs using the Unix process API
- understanding of the concept of signals and their uses
- understanding of I/O redirection
- understanding of the difference between foreground and background processes  

# Running the Program 
To compile the file correctly use the command:

`gcc -std=gnu99 -g -Wall -o smallsh main.c`

A Makefile is also included, so you can also use this command to compile the program:

`make`

Then, once you have the executable, call the command:

`./smallsh`

# User Guide

The display prompt shown to the user is a colon `: `. The program accepts input directly afterwards.

The program will skip a line if it begins with a blank space or the `#` character.

There are three commands accepted by the program:
1. `exit`
2. `cd`
3. `status`

# Exit 
To exit the program, type `exit` and hit enter.

# CD

To change the directory, type `cd <directory_path>` and hit enter.

# Status

To see the status of a command, type `status` and hit enter.

