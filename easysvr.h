/* easysvr.h:   Header file for easy to use multi client ip server           

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
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#ifdef WIN32
#include <winsock.h>
#include <process.h>
#define WSAerrno (WSAGetLastError())
WSADATA WSAData;
#else
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define  closesocket close
#endif

/* Structure for client control data. Passed to dispatch routine */ 
struct ipclient {
  int    cid;	       /* Client ID number   */
  int    sock;         /* Client socket      */
  char   *badr;        /* Buffer address     */
  char   *term;        /* Terminator address */
  struct in_addr ip;   /* Client IP addr     */
  int    port;         /* Client port        */
  int    bcnt;         /* Buffer contents    */
  int    toct;         /* Time out counter   */
};

/* BSIZ establishes the maximum size of client messages. If */
/* a '\n' or ^D is not seen before the buffer is full the   */
/* client is disconnected.  Allocation is dynamic.          */

#define BSIZ   1024     /* Size of buffers */

/* Max clients establishes the number of ipclient slots to  */
/* declare. The sets the maximum connected clients.         */

#define MAXCL 32	/* Max clients */

/* Event types sent to dispatch routine */

#define TIMER_EXPIRED  0
#define CLIENT_CONNECT 1
#define CLIENT_DATA    2
#define CLIENT_EOD     3
#define CLIENT_ERROR   4
#define CLIENT_OVERFL  5
#define CLIENT_TIMEOUT 6

/* The server */
void easysvr(int, void (*)(int, struct ipclient *));

/* Drop this client */
void easysvr_drop(struct ipclient *);

/* Terminate the service */
void easysvr_exit(void);

