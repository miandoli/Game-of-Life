/*
* Author: Matt Iandoli
*/

#ifndef LIFE_H_
#define LIFE_H_

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#define MAXGRID 40
#define ASCII_ZERO 48
#define MAX_THREADS 10
#define GEN 1
#define COMP 2
#define DEAD 3
#define ALLDONE 4

typedef struct msg {
	int iSender; // Sender of the messege (mailbox id)
	int type; // Its type
	int start; // Starting range
	int end; // Ending range
	char **board1; // Game board
	char **board2;
	int bolVal; // Boolean value
} Msg;

typedef struct args {
	// List of arguments
	int arg1;
	char *arg2;
	int arg3;
	int arg4;
	int arg5;
} Args;

int rows, cols;
Msg **arrMail;
sem_t **arrSemS, **arrSemR;

char** readFile(char *inputFileName);
void startThread(int numThreads, char *inputFileName, int gens, int doPrint, int doPause);
void *mainThread(void *arg);
void *childThread(void *arg);
char checkCondition(char **board, int xLoc, int yLoc);
void printArr(char **arr, int numGen);
char** createArr(int x, int y);
void SendMsg(int iTo, Msg *pMsg);
void RecvMsg(int iFrom, Msg *pMsg);

#endif
