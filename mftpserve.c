/* mftpserve.c
 * Ron Cotton
 * CS360 - Systems Programming - Spring 2017 - Dick Lang
 * ---
 */

/* defines */
#define TIME_BUFFER 26
#define MAXCONNECTIONS 4
#define NUMARGS 1

/* includes */
#include "mftp.h"
#include <stdio.h>					/* for printf(), fprintf(), snprintf(), perror() */
#include <stdlib.h>					/* for exit() */
#include <netinet/in.h>				/* for ??? */
#include <netinet/ip.h>				/* for ??? */
#include <sys/wait.h>				/* for waitpid() */

									/* herror() */

#include <arpa/inet.h>				/* for htons(), htonl(), ntohs() */
									/* ntohl() */
									/* (converts byte order) */
#include <errno.h>					/* for perror() */
#include <time.h>					/* for time() */
#include <string.h>					/* for memset(), memcpy(), strerror() */
#include <unistd.h>					/* for write() */
#include <netdb.h>					/* for herror() */
#include <signal.h>					/* for signal() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* globals */
const char *tag[] = {"Parent","Connection 1","Connection 2","Connection 3","Connection 4"};
bSockets S;										/* Berkeley Sockets struct */
int connection = -1;
																				/* Parent = 0, Connections = 1 - 4 */
/* function prototype */
void exitHandler();
void createListenSocket(bSockets *S);
void bindSocketPort(bSockets *S, int port);
void listenConnect(const bSockets *S);
void acceptConnect(bSockets *S);
void writeConnect(const bSockets *S, const char* str);
char* getHostName(const bSockets *S);
/* CCC = control connection commands */
void mftpListen(const bSockets *C);
void servCommand_cd(const bSockets *S, const char *fn);
void servCommand_ls(const bSockets *S, const char *fn);
void servCommand_data(const bSockets *S, const char *arg);

void servCommand_data(const bSockets *S, const char *arg) {
	bSockets D;
	char buf[BUFFERSIZE];

	if (debug) fprintf(stderr, "%s: Establishing data connection\n","Parent");
	createListenSocket(&D);
	bindSocketPort(&D, 0);
	memset(&D.clientAddr, 0, sizeof(D.clientAddr));
	//getSocketIPandPort(&D);
	if (debug) fprintf(stderr, "%s: data socket bound to port %d\n","Parent", D.port);
	listen(D.listenFD, 1);
	unsigned int len = sizeof(D.clientAddr);
	getsockname(D.listenFD, (struct sockaddr *) &(D.clientAddr), (unsigned int *) &len);
	memcpy(D.IP,inet_ntoa(D.clientAddr.sin_addr),(size_t)16);        // release mem when closing??
	D.port = (int) ntohs(D.clientAddr.sin_port);
	if (debug) fprintf(stderr, "%s: data socket bound to port %d\n","Parent", D.port);
	if (debug) fprintf(stderr, "%s: Sending acknowledgement ->  A%d\n","Parent", D.port);
	char port[6];
	snprintf(port,6,"%d",D.port);
	sendSocketConnection(S,"A");
	sendSocketConnection(S,port);
	sendSocketConnection(S,"\n");
	acceptConnect(&D);
	recvSocketConnection(S,buf);

	if (buf[0]=='L') {
		if (debug) fprintf(stderr, "%s: Sending positive acknowledgement\n","Parent");
		sendSocketConnection(S,"A\n");
		if (debug) fprintf(stderr, "%s: forking ls process\n","Parent");

		int childID;

		if ((childID = fork())) { /* FORK *** parent */
			wait(NULL);
		} else { /* child */
			close(STDOUT);
			dup(D.socketFD);
			execlp("ls","ls","-l",(char *)NULL);
			exit(1);
		} /* end fork() */


		if (debug) fprintf(stderr, "%s: ls command completed\n","Parent");
	} else if (buf[0]=='G') { /* get or show ... */
		if (debug) fprintf(stderr, "%s: Reading file %s\n","Parent", (buf + 1));
		if (fileOrDir((buf+1)) != 2) {
			if (debug) fprintf(stderr, "%s: Sending error\n","Parent");
			sendSocketConnection(S,"EFile isn't present or regular\n");
			close(D.socketFD);
			close(D.listenFD);
			return;
		}
		if (debug) fprintf(stderr, "%s: Sending positive acknowledgement\n","Parent");
		sendSocketConnection(S,"A\n");
		if (debug) fprintf(stderr, "%s: transmitting file %s to client\n","Parent", (buf + 1));

		int file = open((buf + 1), O_RDONLY);
		char ch;

		while ((read(file, &ch, 1)) > 0) {
			if (write(D.socketFD, &ch, 1) < 0) fprintf(stderr, "GET: error writing\n");
		}

		close(file);

		if (debug) fprintf(stderr, "%s: Done transmitting file to client.\n","Parent");
	} else if (buf[0]=='P') {
		if (debug) fprintf(stderr, "%s: Writing file %s\n","Parent", (buf + 1));
		if (fileOrDir((buf+1)) != 0) {
			if (debug) fprintf(stderr, "%s: Sending error\n","Parent");
			sendSocketConnection(S,"EFile or directory exists\n");
			close(D.listenFD);
			close(D.socketFD);
			return;
		}
		if (debug) fprintf(stderr, "%s: Sending positive acknowledgement\n","Parent");
		sendSocketConnection(S,"A\n");
		if (debug) fprintf(stderr, "%s: transmitting file %s to server\n","Parent", (buf + 1));

		int file = open((buf+1), O_CREAT | O_APPEND | O_WRONLY, 0444);
		char ch;

		while ((read(D.socketFD, &ch, 1)) > 0) {
			if (write(file, &ch, 1) < 0) fprintf(stderr, "PUT: error writing\n");
		}

		close(file);



		if (debug) fprintf(stderr, "%s: Done receiving file.\n","Parent");
	} else {
		if (debug) fprintf(stderr, "%s: Sending error\n","Parent");
		sendSocketConnection(S,"EInvalid Data command\n");
	}

	close(D.listenFD);
	close(D.socketFD);
} /* end servCommand_data() */

void servCommand_cd(const bSockets *S, const char *fn) {
	if (fileOrDir(fn) == 1) {
		if (chdir(fn) == 0) {
			if (debug) fprintf(stderr, "%s: Changed current directory to %s\n","Parent",fn);
			if (debug) fprintf(stderr, "%s: Sending positive acknowledgement\n","Parent");
			sendSocketConnection(S,"A\n");
			return;
		}
	}
	if (debug) fprintf(stderr, "%s: Sending acknowledgement -> ENo such file or directory\n", "Parent");
	if (debug) fprintf(stderr, "%s: cd to %s failed with error ENo such file or directory\n", "Parent", fn);
	sendSocketConnection(S,"ENo such file or directory\n");
} /* end cliCommand_cd */

/* exitHandler()
 * If SIGINT is sent to terminal, program exits gracefully
 */
void exitHandler() {
	if (debug) fprintf(stderr, "Parent: Killed zombie Process(es).\n");
	while( waitpid(-1,NULL,WNOHANG) > -1) { } /* stop "zombies" */
	close(S.socketFD);
	close(S.listenFD);												/* kill all sockets */
	exit(1);
} /* end exitHandler() */

/* createTCPSocket()
 * creates a TCP Socket
 * AF_INET is the domain -> Internet
 * SOCK_STREAM is the protocol family (TCP)
 * If the result is < 0, there is an error, use perror() to print the
 * message
 */
void createListenSocket(bSockets *S) {
	//int option = 1;
	if ((S->listenFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		errorHandler(PERROR, strerror(errno));

	//setsockopt(S->listenFD, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
} /* end createSocket() */

/* bindSocketPort()
 * bind() assigns the address specified by server address to the socket
 * referred to by the file descriptor.  Traditionally, this operation is
 * called “assigning a name to a socket”.
 */
void bindSocketPort(bSockets *S, int port) {

	memset(&(S->servAddr), 0, sizeof(S->servAddr)); /* sets all to zero */

	S->servAddr.sin_family = AF_INET;
	S->servAddr.sin_port = htons(port);
	if (debug) fprintf(stderr,"%s: socket bound to port %d\n", "Parent", port);
	S->servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if ( bind(S->listenFD, (struct sockaddr *) &(S->servAddr), sizeof(S->servAddr)) < 0)
		errorHandler(PERROR, strerror(errno));
} /* end bindSocketPort() */

/* listenConnect()
 * Sets up a connection queue one level deep
 */
void listenConnect(const bSockets *S) {
	if (listen(S->listenFD, MAXCONNECTIONS) < 0)
		errorHandler(PERROR, strerror(errno));
} /* end listenConnect() */

/* acceptConnect()
 * Waits (blocks) until a connection is established by a client
 * When that happens, a new descriptor for the connection is returned
 * If there is an error the result is < 0;
 * The client’s address is written into the sockaddr structure
 */
void acceptConnect(bSockets *S) {
	int length = sizeof(struct sockaddr_in);

	S->socketFD = accept(S->listenFD,
		(struct sockaddr *) &(S->clientAddr), (socklen_t *) &length);

	if (S->socketFD < 0) if (debug) fprintf(stderr, "Parent: Unable to connect socket\n");
} /* end acceptConnect() */

/* writeConnect()
 *
 */
void writeConnect(const bSockets *S, const char *str) {
	int errWrite = write(S->socketFD, str, strlen(str)+1);
	if (errWrite < 0)
		errorHandler(PERROR, strerror(errno));
} /* end writeConnect() */

/* getHostName()
 * Uses DNS to convert a numerical Internet address to a host name
 * Returns NULL if there is an error, use herror() to print the error
 * message, it has the same argument as perror()
 * NOTE: herror, hstrerror, and h_errno are obsolete
 */
char* getHostName(const bSockets *S) {
	struct hostent* hostEntry;

	hostEntry = gethostbyaddr(&(S->clientAddr.sin_addr),
		sizeof(struct in_addr), AF_INET);

	if (hostEntry==NULL) errorHandler(HERROR, hstrerror(h_errno));

	return (hostEntry->h_name); // HOSTNAME
} /* end getHostName() */

/* mftpListen
 * decide what commands are being send to the client and execute those commands
 * cd and ls not sent to server
 */
void mftpListen(const bSockets *C) {
	char buf[BUFFERSIZE];

	do {
		recvSocketConnection(C, buf);

		if (buf[0]=='D') servCommand_data(C, (buf + 1));
		else if (buf[0]=='C') servCommand_cd(C, (buf + 1));
		else if (buf[0]!='Q'); // unknown command
	} while (buf[0]!='Q');
if (debug) fprintf(stderr,"Server: Quitting\n");
if (debug) fprintf(stderr,"Server: Sending positive acknowledgement\n");
sendSocketConnection(C,"A\n");
} /* end mftpListen() */

/* dayserve - main
 *
 * socket(), bind(), listed(), accept(), fork()
 * parent closes childs socket and loops for another connection
 * child process services the client connection then exits
 * use waitpid() to prevent "zombie" processes
 */
int main(int argc, char *argv[]) {
	setbuf(stdout, NULL);				/* don't buffer IO Stream */
	server = true;						/* This is the server */

	if (argc > 1 && strcmp(argv[1],"-d")) {
		fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
		exit(1);
	} /* end if */

	int childID;

	if (argc == 2 && !strcmp(argv[1],"-d")) debug = true;

	if (debug) fprintf(stderr, "Parent: Debug output enabled.\n");

	signal(SIGINT, exitHandler);				/* exit server on control-C */

	createListenSocket(&S);						/* init command communication */
	bindSocketPort(&S, MY_PORT_NUMBER);
	listenConnect(&S);							/* makes max connections */

	if (debug) fprintf(stderr, "Parent: listening with connection queue of %d\n", MAXCONNECTIONS);
	while(1) {
		waitpid(-1,NULL,WNOHANG);
		acceptConnect(&S);
		if ((childID = fork()) > 0) { // child
			if (childID == -1) errorHandler(PERROR,strerror(errno));
			if (debug) fprintf(stderr, "Parent: spawned '%s', (Child %d) waiting for new connection\n", "Parent", getpid());
			if (debug) fprintf(stderr, "%s: started\n", "Parent");
			getSocketIPandPort(&S);
			if (debug) fprintf(stderr, "%s: Socket Address/Port => %s:%d\n", "Parent", S.IP, S.port);
			mftpListen(&S);
			closeConnect(&S);
			if (debug) fprintf(stderr, "%s: exiting normally.\n", "Parent");
			/* ack code from client would be nice here */
		} /* end fork() */
	} /* end while() */
} /* end main() */
