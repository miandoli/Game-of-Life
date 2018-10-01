/*
* Author: Matt Iandoli
*/

#include "life.h"

/** Main function.
 * @param argc Number of words on the command line.
 * @param argv Array of pointers to character strings containing the
 *    words on the command line.
 * @return 0 if success, 1 if invalid command line or unable to open file.
 *
 */
int main(int argc, char **argv) {
	int threads;
	char *inputFileName;
	int gens;
	int doPrint;
	int doPause;

	// Check if correct number of inputs
	if ((argc < 6) || (argc > 8)) {
		printf("Usage:\n");
		printf("  ./life filename rows columns generations threads [print] [input]\n");
		return 1;
	}

	// Assigns inputs to variables
	inputFileName = argv[1];
	rows = atoi(argv[2]);
	cols = atoi(argv[3]);
	gens = atoi(argv[4]);
	threads = atoi(argv[5]);
	if (threads > MAX_THREADS) {
		threads = MAX_THREADS;
	}
	// Checks to see if optional inputs were entered
	if (argc > 6 && *argv[6] == 'y') {
		doPrint = 1;
	} else {
		doPrint = 0;
	}
	if (argc > 7 && *argv[7] == 'y') {
		doPause = 1;
	} else {
		doPause = 0;
	}

	// Checks to see if "n y" was entered
	if (!doPrint && doPause) {
		printf("Cannot pause wihtout printing!\n");
		return 1;
	}

	// Starts the thread of execution
	startThread(threads, inputFileName, gens, doPrint, doPause);

	return 0;
}

/**
* Reads the file and assigns it centered in the array
* @param inputFileName The filename the user entered
* @return the read in array, centered
*/
char** readFile(char *inputFileName) {
	// Opens file and checks if it was valid
	FILE *input = fopen(inputFileName, "r");
	if (!input) {
		printf("Unable to open input file: %s\n", inputFileName);
		exit(1);
	}

	// Reads in each line, stores them in a temporary array
	int numCols = 0; // Relative size, used to calculate center
	int numRows = 0;
	char **holder = createArr(rows, cols);
	for (int i = 0; i < rows; i++) {
		// Read and analyze one line
		char *oneLine = malloc(cols * sizeof(char));
		fgets(oneLine, cols, input);

		int rowLength = 0;
		for (int j = 0; j < cols; j++) {
			if (oneLine[j] == 'o' || oneLine[j] == 'x') {
				rowLength++;
				holder[i][j] = oneLine[j];
				if (j == 0) {
					numRows++;
				}
			}
		}
		if (rowLength > numCols) {
			numCols = rowLength;
		}
	}

	// Allocate and fill in "blank" board
	char **arrChar = createArr(rows, cols);
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < cols; j++) {
			arrChar[i][j] = 'o';
		}
	}

	// Center board
	int rowStarter = (rows - numRows) / 2;
	int colStarter = (cols - numCols) / 2;
	for (int i = 0; i < rows - rowStarter; i++) {
		for (int j = 0; j < cols - colStarter; j++) {
			arrChar[rowStarter + i][colStarter + j] = holder[i][j];
		}
	}
	return arrChar;
}

/**
* Starts the thread of execution for the parent thread and begins the simulation
* @param numThreads Number of threads
* @param inputFileName Filename to read from
* @param gens Max number of generations to simulate
* @param doPrint Boolean to print out each generation
* @param doPause Boolean to wait for input in between generations
*/
void startThread(int numThreads, char *inputFileName, int gens, int doPrint, int doPause) {
	// Allocate n+1 mailboxes for the threads
	arrMail = (Msg **) malloc((numThreads + 1) * sizeof(Msg *));
	for (int i = 0; i < numThreads + 1; i++) {
		Msg *newMail = (Msg *) malloc(sizeof(Msg));
		arrMail[i] = newMail;
	}
	pthread_t idmain;
	Args *arg = (Args *) malloc(sizeof(Args));
	arg->arg1 = numThreads;
	arg->arg2 = inputFileName;
	arg->arg3 = gens;
	arg->arg4 = doPrint;
	arg->arg5 = doPause;
	if (pthread_create(&idmain, NULL, mainThread, (void *)arg) != 0) {
		perror("pthread_create");
		exit(1);
	}
	(void) pthread_join(idmain, NULL);
}

/**
* The main thread of execution that runs the simulation and creates the child threads
* @param arg Pointer to the struct of arguments
* @return Null pointer
*/
void *mainThread(void *arg) {
	// Get all starting info
	Args *info = (Args *) arg;
	int numThreads = info->arg1;
	char *inputFileName = info->arg2;
	int gens = info->arg3;
	int doPrint = info->arg4;
	int doPause = info->arg5;
	// Allocate n semaphores for mailboxes
	arrSemS = (sem_t **) malloc((numThreads + 1) * sizeof(sem_t *));
	arrSemR = (sem_t **) malloc((numThreads + 1) * sizeof(sem_t *));
	for (int i = 0; i <= numThreads; i++) {
		arrSemS[i] = (sem_t *) malloc(sizeof(sem_t));
		arrSemR[i] = (sem_t *) malloc(sizeof(sem_t));
		if (sem_init(arrSemS[i], 0, 1) < 0) {
			perror("sem_init");
			exit(1);
		}
		if (sem_init(arrSemR[i], 0, 0) < 0) {
			perror("sem_init");
			exit(1);
		}
	}
	// Keep track of the child threads' ids
	pthread_t *idchilds = (pthread_t *) malloc(numThreads * sizeof(pthread_t));
	for (long i = 0; i < numThreads; i++) {
		pthread_t idchild;
		if (pthread_create(&idchild, NULL, childThread, (void *)(i + 1)) != 0) {
			perror("pthread_create");
			exit(1);
		}
		idchilds[i] = idchild;
	}

	// Create set of three grids for simulation
	char **currGrid = readFile(inputFileName);
	char **prevGrid = createArr(rows, cols);
	char **newest = createArr(rows, cols);
	int numGen = 0;
	int len = rows / numThreads;

	// Print out generation 0
	printArr(currGrid, numGen);

	// Keep going until one of the end conditions is met
	int repeat = 0;
	while (!repeat && numGen < gens) {
		// Clean grid of 0's
		newest = createArr(rows, cols);

		// Next generation
		for (int i = 0; i < numThreads; i++) {
			Msg *sMsg = (Msg *) malloc(sizeof(Msg));
			sMsg->iSender = 0;
			sMsg->type = GEN;
			sMsg->start = len * i;
			sMsg->end = (i < numThreads - 1) ? len * (i + 1) : rows;
			sMsg->board1 = currGrid;
			SendMsg(i + 1, sMsg);
		}
		for (int i = 0; i < numThreads; i++) {
			Msg rMsg;
			RecvMsg(0, &rMsg);
			int start = rMsg.start;
			int end = rMsg.end;
			char **newBoard = rMsg.board1;
			for (int j = 0; j < end - start; j++) {
				newest[start + j] = newBoard[j];
			}
		}

		// Compare new and current
		int bolSame = 1;
		for (int i = 0; i < numThreads; i++) {
			Msg *sMsg = (Msg *) malloc(sizeof(Msg));
			sMsg->iSender = 0;
			sMsg->type = COMP;
			sMsg->start = len * i;
			sMsg->end = (i < numThreads - 1) ? len * (i + 1) : rows;
			sMsg->board1 = newest;
			sMsg->board2 = currGrid;
			SendMsg(i + 1, sMsg);
		}
		for (int i = 0; i < numThreads; i++) {
			Msg rMsg;
			RecvMsg(0, &rMsg);
			bolSame = bolSame && rMsg.bolVal;
		}

		// Compare new and previous
		int bolPSame = 1;
		for (int i = 0; i < numThreads; i++) {
			Msg *sMsg = (Msg *) malloc(sizeof(Msg));
			sMsg->iSender = 0;
			sMsg->type = COMP;
			sMsg->start = len * i;
			sMsg->end = (i < numThreads - 1) ? len * (i + 1) : rows;
			sMsg->board1 = newest;
			sMsg->board2 = prevGrid;
			SendMsg(i + 1, sMsg);
		}
		for (int i = 0; i < numThreads; i++) {
			Msg rMsg;
			RecvMsg(0, &rMsg);
			bolPSame = bolPSame && rMsg.bolVal;
		}

		// Check if all are dead
		int bolDead = 1;
		for (int i = 0; i < numThreads; i++) {
			Msg *sMsg = (Msg *) malloc(sizeof(Msg));
			sMsg->iSender = 0;
			sMsg->type = DEAD;
			sMsg->start = len * i;
			sMsg->end = (i < numThreads - 1) ? len * (i + 1) : rows;
			sMsg->board1 = newest;
			SendMsg(i + 1, sMsg);
		}
		for (int i = 0; i < numThreads; i++) {
			Msg rMsg;
			RecvMsg(0, &rMsg);
			bolDead = bolDead && rMsg.bolVal;
		}

		// Compare all conditions
		repeat = bolSame || bolPSame || bolDead;

		// Wait for input if true
		if (doPause) {
			char ch;
			scanf("%c", &ch);
		}

		// Print out if true
		if (!repeat) {
			numGen++;
			if (doPrint) {
				printArr(newest, numGen);
			}
		}
		// Rotate grids
		prevGrid = currGrid;
		currGrid = newest;
	}

	// Send out signal to end child threads and then wait for them to finish
	for (int i = 0; i < numThreads; i++) {
		Msg *sMsg = (Msg *) malloc(sizeof(Msg));
		sMsg->iSender = 0;
		sMsg->type = ALLDONE;
		SendMsg(i + 1, sMsg);
		(void) pthread_join(idchilds[i], NULL);
	}
	// Delete all of the semaphores
	for (int i = 0; i <= numThreads; i++) {
		(void) sem_destroy(arrSemS[i]);
		(void) sem_destroy(arrSemR[i]);
	}
	// Print out last generation if false
	if (!doPrint) {
		printArr(currGrid, numGen);
	}
	printf("Finished after %d generations.\n", numGen);
	pthread_exit(NULL);
	return 0;
}

/**
* Child thread of execution, does small tasks that mostly involve checking portions
* of arrays and returning the result to the main thread
* @param arg Pointer to the struct of arguments
* @return Null pointer
*/
void *childThread(void *arg) {
	// Get the mailbox id
	int mid = (long) arg;

	// Loop until main thread gives the signal to end
	int bolLoop = 1;
	while(bolLoop) {
		// Wait to recieve a message from the main thread
		Msg rMsg;
		RecvMsg(mid, &rMsg);
		int type = rMsg.type;
		int start = rMsg.start;
		int end = rMsg.end;
		// Handle the routine determined by the message type
		if (type == GEN) {
			// Determine the next generation
			char **board = rMsg.board1;
			char **newBoard = createArr(end - start, cols);
			for (int i = 0; i < end - start; i++) {
				for (int j = 0; j < cols; j++) {
					newBoard[i][j] = checkCondition(board, start + i, j);
				}
			}
			Msg *sMsg = (Msg *) malloc(sizeof(Msg));
			sMsg->iSender = mid;
			sMsg->type = ALLDONE;
			sMsg->start = start;
			sMsg->end = end;
			sMsg->board1 = newBoard;
			SendMsg(0, sMsg);
			//free(board);
		} else if (type == COMP) {
			// Compare if two arrays are the same
			char **board1 = rMsg.board1;
			char **board2 = rMsg.board2;
			int bolSame = 1;
			for (int i = start; i < end; i++) {
				for (int j = 0; j < cols; j++) {
					bolSame = bolSame && (board1[i][j] == board2[i][j]);
				}
			}
			Msg *sMsg = (Msg *) malloc(sizeof(Msg));
			sMsg->iSender = mid;
			sMsg->type = ALLDONE;
			sMsg->bolVal = bolSame;
			SendMsg(0, sMsg);
		} else if (type == DEAD) {
			// Determine if a generation is all dead
			char **board = rMsg.board1;
			int bolDead = 0;
			for (int i = start; i < end; i++) {
				for (int j = 0; j < cols; j++) {
					bolDead = bolDead || board[i][j];
				}
			}
			Msg *sMsg = (Msg *) malloc(sizeof(Msg));
			sMsg->iSender = mid;
			sMsg->type = ALLDONE;
			sMsg->bolVal = !bolDead;
			SendMsg(0, sMsg);
		} else {
			// Recieved signal to end running
			bolLoop = 0;
		}
	}
	return 0;
}

/**
* Checks neighbors at a position and determines its outcome
* @param board The board to look at
* @param xLoc The x location to check on the board
* @param yLoc The y location to check on the board
* @return the outcome of the new generation at that position
*/
char checkCondition(char **board, int xLoc, int yLoc) {
	int neighCount = 0;
	int occupied = (board[xLoc][yLoc] == 'x');
	// Check 8 surronding elements and count
	for (int i = xLoc - 1; i <= xLoc + 1; i++) {
		for (int j = yLoc - 1; j <= yLoc + 1; j++) {
			if (i >= 0 && i < rows && j >= 0 && j < cols){
				if ((i != xLoc || j != yLoc) && board[i][j] == 'x') {
					neighCount++;
				}
			}
		}
	}

	// Determine the outcome based on occupied and # of neighbors
	if (occupied && (neighCount <= 1 || neighCount >= 4)) {
		return 'o';
	} else if (occupied && (neighCount == 2 || neighCount == 3)) {
		return 'x';
	} else if (!occupied && neighCount == 3) {
		return 'x';
	} else {
		return 'o';
	}
}

/**
* Prints the array passed in it out to the console
* @param arr The array that is requested to be printed
* @param numGen Number of generations elasped
*/
void printArr(char **arr, int numGen) {
	printf("Generation %d:\n", numGen);
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < cols; j++) {
			if (arr[i][j] == 'x') {
				printf("%c", 'x');
			} else {
				printf("%c", ' ');
			}
		}
		printf("\n");
	}
}

/**
* Allocates memory for given array size for ints
* @param x Number of rows
* @param y Number of columns
* @return Allocated array
*/
char** createArr(int x, int y) {
	char **arr = (char **) malloc(x * sizeof(char *));
	for (int i = 0; i < x; i++) {
		arr[i] = (char *) malloc(y * sizeof(char));
	}
	return arr;
}

/**
* Sends message, blocks if mailbox is not empty
* @param iTo Mailbox id to send to
* @param pMsg Pointer to a message struct that contains the needed info
*/
void SendMsg(int iTo, Msg *pMsg) {
	sem_wait(arrSemS[iTo]);
	arrMail[iTo] = pMsg;
	sem_post(arrSemR[iTo]);
}

/**
* Recieves message, blocks if mailbox is empty
* @param iFrom Mailbox id to recieve from
* @param pMsg Pointer to a message struct that contains the needed info
*/
void RecvMsg(int iFrom, Msg *pMsg) {
	sem_wait(arrSemR[iFrom]);
	*pMsg = *arrMail[iFrom];
	sem_post(arrSemS[iFrom]);
}
