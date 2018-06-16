/* mftp.c
 * Ron Cotton
 * CS360 - Systems Programming - Spring 2017 - Dick Lang
 * ---
 */

/* defines */
#define NUMARGS 3
#define ERROR_RESP 1
#define ACK_RESP 0
#define CLI_EXIT 0
#define CLI_LS 1
#define CLI_RLS 2
#define CLI_CD 3    // <pathname>
#define CLI_RCD 4	// <pathname>
#define CLI_GET 5   // <pathname + filename>
#define CLI_SHOW 6 // <pathname + filename>
#define CLI_PUT 7 // <pathname + filename>

//#define S_ISREG(m) (((m) & 0xf000) == 0x0000)
#define S_ISDIR(m) (((m) & 0xf000) == 0x3000)

/* includes */
#include "mftp.h"
#include <stdio.h>					/* for printf(), fprintf(), perror() */
#include <stdlib.h>					/* for exit() */
#include <string.h>					/* for memset(), memcpy(), strerror() */
#include <netdb.h>					/* for gethostbyname(), herror() */
#include <errno.h>					/* for perror() */
#include <unistd.h>					/* for read(), write() */
#include <string.h>					/* strlen() */
#include <ctype.h>					/* tolower(), isspace() */
#include <unistd.h>					/* execlp(), chdir(), stat() */

#include <fcntl.h>          /* for open() */
#include <arpa/inet.h>			/* for htons() */
#include <sys/stat.h>				/* for stat() */
#include <sys/types.h>			/* for stat() */
#include <sys/wait.h>				/* for wait() */

typedef struct _inputResults {
	char *first;
	char *second;
} inputResults;

/* globals */

/* function prototype */
void errorServer(const bSockets *C, const char *msg);
void DataConnection(const bSockets *C, bSockets *D);
/* CCC = control connection commands */
bool recvServerErrHandler(const bSockets *C, char *msg);
void mftpPrompt(const bSockets *C);
void returnResults(inputResults *iR, char *input);
void cliCommand_exit(bSockets *C);
void cliCommand_cd(const inputResults *iR);
void cliCommand_rcd(const bSockets *C, const inputResults *iR);
void cliCommand_ls();
void cliCommand_rls(const bSockets *C);
void cliCommand_get(const bSockets *C, const inputResults *iR);
void cliCommand_show(const bSockets *C, const inputResults *iR);
void cliCommand_put(const bSockets *C, const inputResults *iR);

void errorServer(const bSockets *C, const char *msg) {
	sendSocketConnection(C, "E");
	sendSocketConnection(C, msg);
	sendSocketConnection(C, "\n");
} /* end errorServer() */

bool recvServerErrHandler(const bSockets *C, char *msg) {
	/* check for errors */
	if (msg[0]=='E') {
		if (debug) fprintf(stderr, "Recieved server response: '%s'\n", msg);
		fprintf(stderr, "Error response from server: %s\n", (msg + 1));
		return true;
	} else {
		return false;
	}
} /* end recvServerErrHandler() */

void DataConnection(const bSockets *C, bSockets *D) {
	char buf[BUFFERSIZE];
	int port;

	sendSocketConnection(C, "D\n");
	if (debug) fprintf(stderr, "Sent D command to server\n");
	recvSocketConnection(C, buf);
	if (debug) fprintf(stderr, "Received server response: '%s'\n",buf);

	/* check for errors */
	if (recvServerErrHandler(C, buf)) {
		close(D->socketFD);
		return;
	}

	/* ack, convert string to number strtol()*/
	port = strtol((buf + 1),NULL,0); /* string, end of the string if bounds issue, 0 = no conversion */

	if (debug) fprintf(stderr, "Obtained port number %d from server\n", port);
	/* now create a data socket */
	BerkeleyTCPSocketConnection(D, C->hostname, port);
} /* end DataConnection() */

void cliCommand_exit(bSockets *C) {
	if (debug) fprintf(stderr, "Exit command encountered\n");

	char buf[BUFFERSIZE];

	sendSocketConnection(C, "Q\n");
	recvSocketConnection(C, buf);
	if (debug) fprintf(stderr, "Client exiting normally\n");
	closeConnect(C);
	exit(1);
} /* cliCommand_exit() */

void cliCommand_cd(const inputResults *iR) {
	if (debug) fprintf(stderr, "cd command encountered\n");

	struct stat s;

	if (stat(iR->second, &s) != 0) {
		fprintf(stderr, "Change directory: Directory does not exist\n");
	}

	if (chdir(iR->second) < 0) fprintf(stderr, "Change directory: No such directory\n");
} /* end cliCommand_cd */

void cliCommand_rcd(const bSockets *C, const inputResults *iR) {
	if (debug) fprintf(stderr, "rcd command encountered\n");
	char buf[BUFFERSIZE];

	sendSocketConnection(C, "C");
	sendSocketConnection(C, iR->second);
	sendSocketConnection(C, "\n");
	recvSocketConnection(C, buf);
	if (recvServerErrHandler(C, buf)) return;

} /* end cliCommand_rcd */

void cliCommand_ls() {
	if (debug) fprintf(stderr, "ls command encountered\n");
	// if the command does not require fork, run here, return
	// when done
	// exit, cd, rcd
	// check for ack if server or error locally

	int child_pid1 = 0,child_pid2 = 0, pfd[2], returnpipe;
	returnpipe = pipe(pfd);				/* pfd[0] - prdr - read end of pipe */
																/* pfd[1] - pwtr - write end of pipe */
	int prdr = pfd[0], pwtr = pfd[1];

	if (returnpipe<0) errorHandler(PERROR, strerror(errno));

	if ((child_pid1 = fork())) { /* FORK *** parent1 */
		if (debug) fprintf(stderr, "Client parent waiting on child process %d to run ls locally\n", child_pid1);
		if (debug) fprintf(stderr, "Client child process %d exec'ing local ls | more\n", child_pid1);
		if (debug) fprintf(stderr, "Child process %d starting more\n", child_pid1);
		pipeHandlerClose(prdr,pwtr);
		wait(NULL);
	} else { /* child1 */
		if ((child_pid2 = fork())) { /* FORK *** parent2 */
			if (debug) fprintf(stderr, "Child process %d starting ls\n", child_pid2);
			pipeHandler(pwtr, STDIN, prdr);
			execlp("more","more","-20",(char *)NULL);
			exit(1);
		} else { /* child2 */
				pipeHandler(prdr, STDOUT, pwtr);
				execlp("ls","ls","-la",(char *)NULL);
				exit(1);
		} /* end NESTED second fork() */
		/* child1 continues */
		exit(1);
	} /* end first fork() */
	if (debug) fprintf(stderr, "ls execution completed\n");
} /* end cliCommand_ls() */

void cliCommand_rls(const bSockets *C) {
	if (debug) fprintf(stderr, "Executing remote ls command\n");
	char buf[BUFFERSIZE];
	bSockets D = { .socketFD = 0, .hostname = "\0" };

	DataConnection(C, &D);

	sendSocketConnection(C, "L\n");
	recvSocketConnection(C, buf);

	if (recvServerErrHandler(C, buf)) {
		close(D.socketFD);
		return;
	}

	if (debug) fprintf(stderr, "Displaying data from server & forking to 'more'...\n");

	int child_pid;

	if ((child_pid = fork())) { /* FORK *** parent */
		if (debug) fprintf(stderr, "Waiting for child process %d to complete execution of more\n", child_pid);
		wait(NULL);
	} else { /* child */
		close(STDIN);
		dup2(D.socketFD,STDIN);
		//pipeHandler(STDIN,D.socketFD,STDIN);
		execlp("more","more","-20",(char *)NULL);
		close(D.socketFD);
		exit(1);
	} /* end fork() */

	close(D.socketFD);
	if (debug) fprintf(stderr, "Data display & more command completed.\n");
} /* end cliCommand_rls() */

void cliCommand_get(const bSockets *C, const inputResults *iR) {
	if (debug) fprintf(stderr, "Getting %s from server and storing to %s\n", iR->second, iR->second);
	bSockets D = { .socketFD = 0, .hostname = "\0" };
	char buf[BUFFERSIZE];

	DataConnection(C, &D);
	// get put - does not fork
	// rls show - forks

	sendSocketConnection(C, "G");
	sendSocketConnection(C, iR->second);
	sendSocketConnection(C, "\n");
	recvSocketConnection(C, buf);

	if (recvServerErrHandler(C, buf)) {
		close(D.socketFD);
		return;
	}

	int bytesread = 0;
	int ch;

	FILE* getServerFile = fopen(iR->second,"wb");

	if (getServerFile==NULL) fprintf(stderr, "file: %s cannot be created or opened\n", iR->second);

	while ((read(D.socketFD, &ch, 1)) > 0) {
		++bytesread;
		fputc(ch,getServerFile);
	}

	if (debug) fprintf(stderr, "Read %d bytes from server, writing to local file\n", bytesread);
	fclose(getServerFile);
	close(D.socketFD);
} /* end cliCommand_get() */

void cliCommand_show(const bSockets *C, const inputResults *iR) {
	if (debug) fprintf(stderr, "Showing file %s\n", iR->second);
	bSockets D = { .socketFD = 0, .hostname = "\0" };
	char buf[BUFFERSIZE];

	DataConnection(C, &D);
	// get put - does not fork
	// rls show - forks

	sendSocketConnection(C, "G");
	sendSocketConnection(C, iR->second);
	sendSocketConnection(C, "\n");
	//sendSocketConnection(C, "\n");
	recvSocketConnection(C, buf);

	if (recvServerErrHandler(C, buf)) {
		close(D.socketFD);
		return;
	}

	int child_pid1 = 0, pfd[2], returnpipe = pipe(pfd);
	/* pfd[0] - prdr - read end of pipe */
	/* pfd[1] - pwtr - write end of pipe */
	int prdr = pfd[0], pwtr = pfd[1];

	if (returnpipe<0) errorHandler(PERROR, strerror(errno));

	if ((child_pid1 = fork())) { /* FORK *** parent */
		pipeHandlerClose(prdr,pwtr);
		if (debug) fprintf(stderr, "Waiting for child process %d to complete execution of more\n", child_pid1);
		wait(NULL);
	} else { /* child */
		close(STDIN);
		dup2(D.socketFD,STDIN);
		//pipeHandler(pwtr, D.socketFD, prdr);
		execlp("more","more","-20",(char *)NULL);
		exit(1);
	} /* end fork() */

	close(D.socketFD);

		if (debug) fprintf(stderr, "Data display & more command completed.\n");
} /* end cliCommand_show() */

void cliCommand_put(const bSockets *C,const inputResults *iR) {
	if (debug) fprintf(stderr, "putting file %s to %s\n", iR->second, iR->second);
	bSockets D = { .socketFD = 0, .hostname = "\0" };
	char buf[BUFFERSIZE];

	DataConnection(C, &D);

	sendSocketConnection(C, "P");
	sendSocketConnection(C, iR->second);
	sendSocketConnection(C, "\n");
	recvSocketConnection(C, buf);

	if (debug) fprintf(stderr, "Getting %s from server and storing to %s\n", iR->second, iR->second);

	if (recvServerErrHandler(C, buf)) {
		close(D.socketFD);
		return;
	}

	FILE* putServerFile = fopen(iR->second,"rb");
	int byteswrite = 0;
	int ch;

	struct stat s;
	if (stat(iR->second, &s) != 0) { fprintf(stderr, "file %s does not exist\n", iR->second); close(D.socketFD); return; }
	if (S_ISREG(s.st_mode)==0) { fprintf(stderr, "file %s is not regular\n", iR->second); close(D.socketFD); return; }


	if (debug) fprintf(stderr, "Opened local file %s for reading\n", iR->second);

	while ((ch=fgetc(putServerFile))!=EOF) {
		++byteswrite;
		if (write(D.socketFD, &ch, 1) < 0) break;
	}

	if (debug) fprintf(stderr, "Writing %d bytes to server\n", byteswrite);
	if (debug) fprintf(stderr, "Closing Local File\n");
	fclose(putServerFile);
	close(D.socketFD);
} /* end cliCommand_put() */

void returnResults(inputResults *iR, char *input) {
	const char *delimiters = " ";

	for (int i = 0; i < (strlen(input)); ++i) {
		if (input[i] == '\n') input[i] = '\0';
	}

	iR->first = strtok(input, delimiters);

	if (iR->first==NULL) {
		iR->second = NULL;
	} else {
		if (iR->first!=NULL) iR->second = strtok(NULL, delimiters);
	}

	if (debug) {
		if (iR->first!=NULL) fprintf(stderr, "Command string = '%s'", iR->first);
		if (iR->second!=NULL) fprintf(stderr, " with parameter = '%s'", iR->second);
		fprintf(stderr, "\n");
	}
} /* end returnResults() */

void mftpPrompt(const bSockets *C) {
	char input[BUFFERSIZE];
	inputResults iR = { .first = NULL, .second = NULL };

	do {
		printf("MFTP> ");
		if (fgets(input, sizeof(input), stdin)==NULL) continue;

		returnResults(&iR,input);

		if (iR.first==NULL) iR.first=" ";
		if ((!strcmp(iR.first,"cd"))&&iR.second!=NULL) cliCommand_cd(&iR);
		else if (!strcmp(iR.first,"rcd")&&iR.second!=NULL) cliCommand_rcd(C, &iR);
		else if (!strcmp(iR.first,"ls")&&iR.second==NULL) cliCommand_ls();
		else if (!strcmp(iR.first,"rls")&&iR.second==NULL) cliCommand_rls(C);
		else if (!strcmp(iR.first,"get")&&iR.second!=NULL) cliCommand_get(C, &iR);
		else if (!strcmp(iR.first,"show")&&iR.second!=NULL) cliCommand_show(C,&iR);
		else if (!strcmp(iR.first,"put")&&iR.second!=NULL) cliCommand_put(C,&iR);
		else if (debug&&strcmp(iR.first,"exit")) fprintf(stderr, "Command error: expecting a parameter.\n");
	} while ((strcmp(iR.first,"exit")));
} /* end mftpPrompt() */

/* main()
 */
int main(int argc, char *argv[]) {
	setbuf(stdout, NULL);				/* don't buffer IO Stream */
	server = false;						/* This is client, not server */

	if (argc > NUMARGS||argc==1||(argc==2&&!strcmp(argv[1],"-d"))) {
		fprintf(stderr, "Usage: %s [-d] <hostname | IP address>\n", argv[0]);
		exit(1);
	} /* end if */

	if (argc==3&&!strcmp(argv[1],"-d")) debug = true;

	/* declare and initialize bSockets */
	bSockets C = { .socketFD = 0, .hostname = "\0" };

	if (!strcmp(argv[1],"-d")) BerkeleyTCPSocketConnection(&C, argv[2], MY_PORT_NUMBER);
	else BerkeleyTCPSocketConnection(&C, argv[1], MY_PORT_NUMBER);


	if (!strcmp(argv[1],"-d")) printf("Connected to server %s\n", argv[2]);
	else printf("Connected to server %s\n", argv[1]);

	mftpPrompt(&C);
	cliCommand_exit(&C);

	closeConnect(&C);
} /* end main() */
