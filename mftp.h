/* mftp.h
 * Ron Cotton
 * CS360 - Systems Programming - Spring 2017 - Dick Lang
 * ---
 */

 /* defines */
 #define _BSD_SOURCE
 #define MY_PORT_NUMBER 49999
 #define IPV4SIZE 16
 #define BUFFERSIZE 4096
 #define PERROR 1
 #define HERROR 2
 #define STDIN 0
 #define STDOUT 1
 #define STDERR 2

/* includes */
#include <sys/socket.h>             /* for socket(), setsockopt(), bind(), accept() */
									/* gethostbyaddr(), getsockname(), inet_ntoa(), */
                                    /* hstrerror() */
#include <sys/types.h>				/* for socket(), setsockopt(), bind() */
#include <sys/stat.h>               /* for stat() */
#include <string.h>                 /* for memset() */
#include <arpa/inet.h>              /* for htons(), inet_ntoa() */
#include <netinet/in.h>             /* for inet_ntoa(), herrror() */
#include <netdb.h>                  /* for herror() */
#include <stdbool.h>				/* for bool() */
#include <stdio.h>                  /* for perror(), fprintf() */
#include <stdlib.h>                 /* for exit() */
#include <unistd.h>                 /* for close() */
#include <error.h>                  /* for perror() */
#include <errno.h>                  /* for errno */


/* globals */
bool debug = false;
bool server;

/* Structures - Berkeley Sockets Connection  */
 typedef struct _bSockets {
    bool server;                                /* if server, true else false */
 	int socketFD;
 	int listenFD;								/* listenFD - used by server */
 	char hostname[BUFFERSIZE];
 	char IP[IPV4SIZE];
 	int port;
 	struct sockaddr_in servAddr;
 	struct sockaddr_in clientAddr;			 /* clientAddr -  used by server */
 } bSockets; /* end typedef struct */

 /* function prototypes */
void BerkeleyTCPSocketConnection(bSockets *B, const char* hostname, const int port);
void createTCPSocket(bSockets *B);
void setupSocketAddr(bSockets *B, const char* hostname, int port);
void getSocketIPandPort(bSockets *B);
void socketConnect(bSockets *B);
void sendSocketConnection(const bSockets *B, const char *send);
void recvSocketConnection(const bSockets *B, char *recv);
int fileOrDir(const char* fn);
void closeConnect(bSockets *B);
void errorHandler(int errtype, const char* msg);
int pipeHandler(int initialFD, int streamFD, int finalFD);
void pipeHandlerClose(int close1, int close2);

/* function definitions */
void pipeHandlerClose(int close1, int close2) {
	close(close1);
	close(close2);
} /* end pipeHandlerClose() */

int pipeHandler(int initialFD, int streamFD, int finalFD) {
	int dupfd;

	close(initialFD);		/* close initial FD */
	close(streamFD);		/* close stream FD */
	dupfd=dup(finalFD);	/* replaces the FD of stream with final */
	close(finalFD);		  /* close the final fd */
	return dupfd;
} /* end pipeManagement() */

int fileOrDir(const char* fn) {
    struct stat s;
    if (stat(fn, &s) != 0) return 0; // does not exist
    if (S_ISDIR(s.st_mode)) return 1;        // is a directory
    if (S_ISREG(s.st_mode)) return 2;        // is a reg file
    return -1;                               // unknown
}

void BerkeleyTCPSocketConnection(bSockets *B, const char* hostname, const int port) {
 	createTCPSocket(B);
 	setupSocketAddr(B, hostname, port);
 	getSocketIPandPort(B);
 	if (debug) fprintf(stderr, "Socket Address/Port => %s:%d\n", B->IP, B->port);
 	if (debug) fprintf(stderr, "Attempting to establish Data Connection...\n");
 	socketConnect(B);
 	if (debug) fprintf(stderr, "Data connection to server established\n");
 } /* end BerkleyTCPSocketConnection() */

 /* createTCPSocket()
  * creates a TCP Socket
  * AF_INET is the domain -> Internet
  * SOCK_STREAM is the protocol family (TCP)
  * If the result is < 0, there is an error, use perror() to print the
  * message
  */
 void createTCPSocket(bSockets *B) {
 	int option = 1;
    B->server = server;                         /* is this a server or not? */
 	if ((B->socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
 		errorHandler(PERROR, strerror(errno));
 	if (debug) fprintf (stderr, "Created socket with descriptor %d\n", B->socketFD);
 	setsockopt(B->socketFD, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
 } /* end createTCPSocket() */

 /* setupSocketAddrServer()
  * Plug in the family and port number
  * Get the numeric form of the server address
  * Copy it into the address structure
  */
 void setupSocketAddr(bSockets *B, const char* hostname, int port) {
 	struct hostent* hostEntry;
 	struct in_addr **pptr;

 	memset(&(B->servAddr), 0, sizeof(B->servAddr));
 	B->servAddr.sin_family = AF_INET;
 	B->servAddr.sin_port = htons(port);

 	strncpy(B->hostname, hostname, strlen(hostname));   // release mem when closing?
 	hostEntry = gethostbyname(hostname);

 	/* test for error using herror */
 	if (hostEntry==NULL) {
 		close(B->socketFD);
 		errorHandler(HERROR, hstrerror(h_errno));
 	} /* end if */

 	/* this is magic, unless you want to dig into the man pages */
 	pptr = (struct in_addr **) hostEntry->h_addr_list;
 	memcpy(&(B->servAddr.sin_addr), *pptr, sizeof(struct in_addr));
 	/* end magic */
 } /* end setupSocketAddr() */

 /* getSocketIPandPort()
  * getsockname() call that returns the IP as a string and the port as an int
  */
 void getSocketIPandPort(bSockets *B) {

     if (!server) {
         getsockname(B->socketFD, (struct sockaddr *) &(B->servAddr), (unsigned int *) sizeof(B->servAddr));
         memcpy(B->IP,inet_ntoa(B->servAddr.sin_addr),(size_t)16);        // release mem when closing??
         B->port = (int) ntohs(B->servAddr.sin_port);
     } else {
         getsockname(B->listenFD, (struct sockaddr *) &(B->clientAddr), (unsigned int *) sizeof(B->clientAddr));
         memcpy(B->IP,inet_ntoa(B->clientAddr.sin_addr),(size_t)16);        // release mem when closing??
         B->port = (int) ntohs(B->clientAddr.sin_port);
     }
 } /* end getSocketIPandPort() */

 /* socketConnect()
  *  A connection is established, after which I/O may be performed on
  * socket file descriptor. This is the file I/O equivalent of "open()"
  * Or, there is an error which is indicated by the return being < 0
  * Always check for an error... */
 void socketConnect(bSockets *B) {
   int errConnect = connect(B->socketFD, (struct sockaddr *) &(B->servAddr),
       sizeof(B->servAddr));

   if (errConnect < 0) {
       close(B->socketFD);
       errorHandler(PERROR, strerror(errno));
   } /* end if */
 } /* end socketConnect() */

 /*
  *
  */
 void sendSocketConnection(const bSockets *B, const char *send) {
 	int index = 0;
 	int errWrite = 0;
 	int sendlen = strlen(send);
 	int packet = sendlen;
 	/* 0 = done reading from socket // >0 = reading // >0 = error */

 	if (packet > BUFFERSIZE)
 		packet = BUFFERSIZE;

 	while (sendlen) {
 			errWrite = write(B->socketFD, send + index, packet);
 			if (errWrite < 0) {
 				close(B->socketFD);
 				if (debug) fprintf(stderr, "Unable to send on Connection. Connection Closed and exiting.\n");
 				exit(1);
 			}
 			sendlen = sendlen - errWrite; /* write returns number of bytes send */
 			index =+ errWrite;
 		}
 } /* end sendSocketConnection() */

/* recvSocketConnection()
 * sends data over connection, needs refactoring
 */
 void recvSocketConnection(const bSockets *B, char *recv) {
 	int errRead;

 	/* 0 = done reading from socket // >0 = reading // >0 = error */
 	if (debug) {
        if (server) fprintf(stderr, "Server: Sending ACK response\n");
        else fprintf(stderr, "Sender ACK response\n");
    }

 			for (int index = 0; index < BUFFERSIZE; ++index) {
 				errRead = read(B->socketFD, recv + index, 1);
 				if (errRead < 0) { close(B->socketFD); if (debug) fprintf(stderr, "Unable to recieve on Connection. Connection Closed and exiting.\n"); exit(1);}
 				if (recv[index]=='\n') { recv[index]='\0'; break; }
 			}
 	if (debug) fprintf(stderr, "Received ACK response: '%c'\n", recv[0]);
 } /* end recvSocketConnection() */

/* closeConnect()
 *
 */
void closeConnect(bSockets *B) {
    if (server) {
        close(B->listenFD);
        close(B->socketFD);
    } else {
        close(B->socketFD);
    }
} /* end closeConnect() */

 /* errorHandler()
  * handles perror and herror
  */
 void errorHandler(int errtype, const char* msg) {
 	fprintf (stderr, "ERROR: %s\n", __FILE__);
 	switch (errtype) {
 		case PERROR: perror(msg); fprintf(stderr, "\n"); break;
 		case HERROR: herror(msg); fprintf(stderr, "\n"); break;
 		default : fprintf(stderr, "ERROR: Unknown Error.\n");
 	} /* end switch() */
 	exit(1);
 } /* end errorHandler */
