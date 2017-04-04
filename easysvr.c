/* easysvr.c:   Easy to use multi client ip server           

   Copyright (c) 2010, 2016  Edward J Jaquay 

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   EDWARD JAQUAY BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of Edward Jaquay shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Edward Jaquay.

   24-Feb-13 EJJ Adapt for Windows Sockets API 

   This module implements a tcpip service that accepts ascii strings as
   messages from clients and passes the strings to a user supplied dispatch 
   function for processing. Messages are terminated by CR, LF, or EOF (^D)

   The easysvr routine accepts two arguments, an integer port number and a 
   pointer to a user supplied dispatch routine.

   The dispatch routine receives an event type and a pointer to an ipclient 
   structure.

   The ipclient structure contains the client number, client buffer pointers, 
   socket descriptor, client address, length of buffer contents, and a timout 
   count.  The dispatch routine must not alter the contents of this structure.

   The event type is one of CLIENT_CONNECT, CLIENT_DATA, CLIENT_TIMEOUT,
   CLIENT_EOD, CLIENT_ERROR, CLIENT_OVERFL, CLIENT_REJECT, or TIMER_EXPIRED.  

   In the case of CLIENT_CONNECT or CLIENT_DATA the socket descriptor can be 
   used to send replies to the client, which must be ready to receive them.

   In the case of CLIENT_TIMEOUT, CLIENT_EOD, CLIENT_ERROR, or CLIENT_OVERFL
   the client is dropped and the socket descriptor is not valid.  

   Regardless of the number of client connections the dispatch routine is 
   called approximately once a minute with a TIMER_EXPIRED event code.  In this
   case the ipclient structure pointer is NULL.  The  TIMER_EXPIRED event
   can be used to perform time based tasks, for instance re-opening a log file.

   The one minute clock is also used by easysvr to decrement the time out
   count, which is done before calling the dispatch routine.  If there is no
   activity for a client for approximately 10 minutes the client is dropped 
   and the dispatch routine is called with CLIENT_TIMEOUT event code.

   There is no return from the easysvr function.  Program termination is done
   by the c standard exit function, which calls exit handlers established by
   atexit().  If the dispatch routine desires to cause program termination 
   it should do so by calling exit(status)
      
   Informational and error messages are written to stderr.
*/

#include "easysvr.h"

struct ipclient IPCL[MAXCL];    /* IP clients  */
fd_set RDSET;			/* Current read set  */

/* Find terminator while ignoring nulls in buffer */
char *
easysvr_findterm(char *buf, int len) {
   while (len-- > 0) {
     if (*buf == '\n')   return buf;
     if (*buf == '\r')   return buf;
     if (*buf == '\004') return buf;
     buf++;
   }
   return 0;
}

/* Drop a client */
void
easysvr_drop(struct ipclient * cl) {
  int csock;
  if (csock = cl->sock) {
    FD_CLR(csock, &RDSET);
    shutdown(csock,2);
    closesocket(csock);
    cl->sock = 0;
  }
}


/* Exit the server */
void
easysvr_exit(void)
{
  int clnt;
  for (clnt=0; clnt<MAXCL; clnt++) {
    if(IPCL[clnt].badr) free(IPCL[clnt].badr);
    easysvr_drop(&IPCL[clnt]);
  }
#ifdef WIN32
  WSACleanup();
#endif
}

void
easysvr(int port, void (*dispatch)(int, struct ipclient *)) 
{
  struct sockaddr_in lsadr;	/* Listen sock addr  */
  struct sockaddr_in csadr;	/* Client sock addr  */
  struct timeval tmo;           /* Timeout value     */
  fd_set fds;			/* Generic fd set    */
  time_t ctime;			/* Current unix time */
  time_t ptime;			/* Prev. unix time   */
  char *bptr;		        /* Buffer pointer    */
  char *tptr;			/* Termnator pointer */
  char tchr;			/* Input terminator  */
  int  bfree;			/* Buffer free space */
  int  clnt;			/* Client number     */
  int  csock;			/* Client socket     */
  int  lsock;			/* Listen socket     */
  int  setbits;                 /* Set true          */
  int  cnt;			/* Number of bytes   */
  int  selcnt;			/* Number selected   */
  int  retries;			/* Bind retry ctr    */
  int  ret;                     /* Return code       */
  
/***********************************/
/*         Initialize              */
/***********************************/

  memset(&IPCL, 0, sizeof(IPCL));
  FD_ZERO(&RDSET);

  /* Set previous time to the start of prev minute */
  time(&ctime);
  ptime = ctime - (ctime % 60); 

#ifdef WIN32
  if (WSAStartup(0x0101, &WSAData)) {
    fprintf(stderr,"WSAStartup failed\n");
    exit(1);
  }
#endif
  
  /* Establish exit handler */
  atexit(easysvr_exit);
  
/***********************************/
/* Set up listen on specified port */
/***********************************/

#ifdef WIN32
  if ((lsock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
    fprintf(stderr, "Create error: %s\n", strerror(errno));
    exit(1);
  }
#else
  if ((lsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "Create error: %s\n", strerror(errno));
    exit(1);
  }
#endif

  /* Allow reuse of address */
  setbits = -1;
  setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, (char *)&setbits, sizeof setbits);

  /* Bind to the port */
  lsadr.sin_family = AF_INET;
  lsadr.sin_port = htons(port);
  lsadr.sin_addr.s_addr = INADDR_ANY;

  if (bind(lsock, (struct sockaddr *) &lsadr, sizeof(lsadr))) {
    fprintf(stderr, "Bind error port %d: %s\n", port, strerror(errno));
    perror("");
    exit(1);
  }

  /* Listen on socket */
  if (listen(lsock, 5) < 0) {	/* backlog is 5 */
    fprintf(stderr, "Listen error port %d: %s\n", port, strerror(errno));
    exit(1);
  }

  /* Add the listen socket to the reading set */
  FD_SET(lsock, &RDSET);
  fprintf(stderr,"Listening on port %d socket %d\n", port, lsock);

/***********************************/
/*    Loop forever getting input   */
/***********************************/

  while (1) {

    fds = RDSET;
    tmo.tv_sec  = 1;
    tmo.tv_usec = 0;
    selcnt = select(MAXCL+8, &fds, NULL, NULL, &tmo);
    if (selcnt < 0) {
      fprintf(stderr, "Select error %s\n", strerror(errno));
      exit(1);
    }

/***********************************/
/*  Check for timeout              */
/***********************************/

    time(&ctime);

    if (ctime-ptime >= 60) {
      ptime = ctime - (ctime % 60);

      /* Drop timed out clients */
      for (clnt = 0; clnt < MAXCL; clnt++) {
        if (csock = IPCL[clnt].sock) {
          if ((IPCL[clnt].toct += 1) > 9) {
            (*dispatch)(CLIENT_TIMEOUT,&IPCL[clnt]);
             easysvr_drop(&IPCL[clnt]);
	  }
	}
      }

      /* Dispatch a timeout */
      (*dispatch)(TIMER_EXPIRED,NULL);
    }

    if (selcnt < 1) continue;

/***********************************/
/*    Connection request           */
/***********************************/

    if (FD_ISSET(lsock, &fds)) {

      /* Accept the connection */
      cnt = sizeof(csadr);
      csock = accept(lsock, (struct sockaddr *) &csadr, &cnt);

      /* Find slot for the socket */
      for (clnt=0; clnt < MAXCL; clnt++) {
        if (IPCL[clnt].sock == 0) break;
      }
         
      if (clnt >= MAXCL) {
        fprintf(stderr,"Max clients exceeded %d\n", csock);
	shutdown(csock,2);
	closesocket(csock);
	
      } else {

        /* Input buffers are allocated dynamically as they are needed. */  
        /* Once used buffers are not freed until program exit.         */
        if (IPCL[clnt].badr == NULL) {
	  if (bptr = malloc(BSIZ)) {
            IPCL[clnt].badr = bptr;
          } else {
            fprintf(stderr,"Memory error %s\n", strerror(errno));
            exit(1);
          }
        }

        IPCL[clnt].cid  = clnt + 1;
	IPCL[clnt].sock = csock;
	IPCL[clnt].ip   = csadr.sin_addr;
	IPCL[clnt].port = csadr.sin_port;
        IPCL[clnt].term = bptr;
	IPCL[clnt].bcnt = 0;
        IPCL[clnt].toct = 0;
	FD_SET(csock, &RDSET);

        (*dispatch)(CLIENT_CONNECT,&IPCL[clnt]);

      }
    }

/***********************************/
/*    Data from client             */
/***********************************/

    /* Search clients for input activity */
    for (clnt = 0; clnt < MAXCL; clnt++) {

      if (!(csock = IPCL[clnt].sock)) continue; /* Skip unused client slots */
      if (!FD_ISSET(csock, &fds)) continue;     /* Skip inactive cliets     */ 
      
      /* Insure buffer address is not NULL */
      if ((bptr = IPCL[clnt].badr) == NULL) {
        fprintf(stderr,"Internal buffer error\n");
	exit(1);
      }

      /* Check for space in buffer */ 
      bfree = BSIZ - IPCL[clnt].bcnt; 
      if (bfree < 1) {
        IPCL[clnt].term = bptr;
        (*dispatch)(CLIENT_OVERFL,&IPCL[clnt]);
	IPCL[clnt].bcnt = 0;
	continue;
      }

      /* Get data from client socket */
      cnt = recv(csock, bptr + IPCL[clnt].bcnt, bfree, 0);
      if (cnt > 0) {
        IPCL[clnt].bcnt += cnt;           /* Add to buffer content     */
        IPCL[clnt].toct = 0;              /* Clear timout counter      */

      } else {
	if (cnt < 0) perror("read");     
        easysvr_drop(&IPCL[clnt]);
        (*dispatch)(CLIENT_ERROR,&IPCL[clnt]);
	continue;
      }

      /* Look for a terminator */
      if (IPCL[clnt].term = easysvr_findterm(bptr,IPCL[clnt].bcnt)) {
        
        /* Drop client if EOD */
	if (*IPCL[clnt].term == '\004') { 
          easysvr_drop(&IPCL[clnt]);
	  if (IPCL[clnt].term > bptr) {
              (*dispatch)(CLIENT_DATA,&IPCL[clnt]);
          }
          (*dispatch)(CLIENT_EOD,&IPCL[clnt]);
	} else {
	   (*dispatch)(CLIENT_DATA,&IPCL[clnt]);
        }

	IPCL[clnt].bcnt = 0;

      }	/* End if terminator */
    }	/* End search descriptors */
  }	/* End loop getting input */
}	/* End main */
