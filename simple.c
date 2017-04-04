
/* Example server responds to hello, path, and goodby */
/* Use make to build, test with 'telnet localhost 6666 */

#include "easysvr.h"

/* The easysvr routine keeps track of clients and dispatching
 * client requests, but a handler is required to process them. */

void
client_handler(int event, struct ipclient * cl)
{
    char * msg;

    switch(event) {

      /* Client is connecting */  
      case CLIENT_CONNECT:

        printf("Client %d connect %s\n",cl->cid,inet_ntoa(cl->ip));
        msg = "Greetings\n> ";
        send(cl->sock,msg,strlen(msg),0);
        break;

      /* Client has been disconnected for some reason. */
      case CLIENT_EOD:
      case CLIENT_ERROR:
      case CLIENT_TIMEOUT:

        printf("Client %d dropped\n",cl->cid);
        break;

      /* Client sent something. Respond as required */
      case CLIENT_DATA:

	    if (strncmp(cl->badr,"hello", 5) == 0) {
            msg = "Hello\n> ";
            send(cl->sock,msg,strlen(msg),0);

	    } else if (strncmp(cl->badr,"path", 4) == 0) {
            msg = getenv("PATH");
            send(cl->sock,msg,strlen(msg),0);
            send(cl->sock,"\n> ",3,0);

        } else if (strncmp(cl->badr,"goodby", 6) == 0) {
            printf("Client %d said goodby\n",cl->cid);
            easysvr_drop(cl);

        } else {
            msg = "Huh?\n> ";
            send(cl->sock,msg,strlen(msg),0);
        }
        break;
    }
}

/* main calls the easysvr routine */

int main() {
    easysvr(6666,&client_handler); /* never returns */
    return 1;
}

