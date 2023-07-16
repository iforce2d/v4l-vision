
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

#include "qv4l2.h"

#include "json/json.h"

#include "network.h"
#include "vision.h"

#define HOSTNAME_LEN    256
#define VERSION_LEN     32
#define INBUF_LEN       256
#define OUTBUF_LEN      256
//#define PATH_MAX        256

typedef struct {
  int cliSock;
  char hostName[HOSTNAME_LEN];
  char version[VERSION_LEN];
  bool linked;
  bool echo;
  bool verbose;
  bool enabled;
  int commMode;
  int commProt;
  char inBuf[INBUF_LEN];
  char outBuf[OUTBUF_LEN];
  char progName[PATH_MAX];

  bool printUpdates;
} connectionRecType;

std::vector<connectionRecType*> currentConnections;

int server_sockfd;
int port = 5008;


socklen_t server_len, client_len;
struct sockaddr_in server_address;
struct sockaddr_in client_address;
bool useSockets = true;
int tokenIdx;
const char *delims = " \n\r\0";
int enabledConn = -1;
char pwd[16] = "EMC\0";
char enablePWD[16] = "EMCTOO\0";
char serverName[24] = "EMCNETSVR\0";
int sessions = 0;
int maxSessions = -1;


int initSockets()
{
  int optval = 1;
  int err;

  server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);
  server_address.sin_port = htons(port);
  server_len = sizeof(server_address);
  err = bind(server_sockfd, (struct sockaddr *)&server_address, server_len);
  if (err) {
      printf("error initializing sockets: %s\n", strerror(errno));
      return err;
  }

  err = listen(server_sockfd, 5);
  if (err) {
      printf("error listening on socket: %s\n", strerror(errno));
      return err;
  }

  // ignore SIGCHLD
  {
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGCHLD, &act, NULL);
  }

  return 0;
}

void strtoupper(char * t, int n) {
  char *s = t;
  int i = 0;
  while (*s && i < n) {
    *s = toupper((unsigned char) *s);
    s++;
    i++;
  }
}

typedef enum {
    cmdStatus = 0,
    cmdOn,
    cmdOff,
    cmdColor,
    cmdGray,
    cmdThreshold,
    cmdUnknown
} commandTokenType;

const char *commands[] = {"STATUS", "ON", "OFF", "COLOR", "GRAY", "THRESHOLD", ""};

int lookupToken(char *s)
{
    int i = 0;
    while (i < cmdUnknown) {
        if (strcmp(commands[i], s) == 0)
            return i;
        i++;
    }
    return i;
}

void replyStatus(connectionRecType *context) {
    Json::Value val(Json::objectValue);

    val["color"] = visionParams.color;
    val["threshold"] = visionParams.threshold;
    val["maskSize"] = visionParams.maskSize;
    val["minPixels"] = visionParams.minPixels;
    val["maxPixels"] = visionParams.maxPixels;
    val["minWidth"] = visionParams.minWidth;
    val["maxWidth"] = visionParams.maxWidth;
    //val["maxAspectDiff"] = visionParams.maxAspectDiff;
    val["displayType"] = visionParams.displayType;
    val["overlayElements"] = visionParams.overlayElements;

    val["focus"] = visionParams.focus;
    val["zoom"] = visionParams.zoom;

    val["updates"] = context->printUpdates;

    Json::FastWriter writer;
    std::string str = writer.write( val ) + "\n";
    write(context->cliSock, str.c_str(), str.size());
}

#define GET_VISION_PARAM(theParam,def)\
    if ( incomingValue.isMember( #theParam ) )\
        visionParams.theParam = incomingValue.get( #theParam, def).asInt();

int parseJSON(char* inBuf) {
    Json::Value incomingValue;
    Json::Reader reader;
    if ( ! reader.parse( inBuf, incomingValue) ) {
        printf("Could not parse JSON\n");
        return -1;
    }

    GET_VISION_PARAM(color,VISION_DEFAULT_COLOR);
    GET_VISION_PARAM(threshold,VISION_DEFAULT_THRESHOLD);
    GET_VISION_PARAM(maskSize,VISION_DEFAULT_MASKSIZE);
    GET_VISION_PARAM(minPixels,VISION_DEFAULT_MINPIXELS);
    GET_VISION_PARAM(maxPixels,VISION_DEFAULT_MAXPIXELS);
    GET_VISION_PARAM(minWidth,VISION_DEFAULT_MINWIDTH);
    GET_VISION_PARAM(maxWidth,VISION_DEFAULT_MAXWIDTH);
    //GET_VISION_PARAM(maxAspectDiff,VISION_DEFAULT_MAXASPECTDIFF);
    GET_VISION_PARAM(displayType,VISION_DEFAULT_DISPLAYTYPE);
    GET_VISION_PARAM(overlayElements,0xFFFFFFFF);

    if ( incomingValue.isMember( "focus" ) ) {
        visionParams.focus = incomingValue.get( "focus", VISION_DEFAULT_FOCUS).asInt();
        g_mw->setVal(VISION_CTRL_FOCUS_ID, visionParams.focus);
        int v = g_mw->getVal(VISION_CTRL_AUTOFOCUS_ID);
        if ( v ) {
            g_mw->setVal(VISION_CTRL_AUTOFOCUS_ID, 0);
            g_mw->updateCtrl(VISION_CTRL_AUTOFOCUS_ID);
        }
    }
    if ( incomingValue.isMember( "zoom" ) ) {
        visionParams.zoom = incomingValue.get( "zoom", VISION_DEFAULT_ZOOM).asInt();
        g_mw->setVal(VISION_CTRL_ZOOM_ID, visionParams.zoom);
    }

    return 0;
}

int parseCommand(connectionRecType *context)
{
    int ret = 0;

    printf("Parsing: %s\n", context->inBuf);

    char originalInBuf[INBUF_LEN];
    strncpy(originalInBuf, context->inBuf, INBUF_LEN-1);

    strtoupper(context->inBuf, INBUF_LEN-1);

    if ( context->inBuf[0] == '{' ) {
        return parseJSON(originalInBuf);
    }

    char* pch = strtok(context->inBuf, delims);

        if (pch != NULL) {

            switch (lookupToken(pch)) {

                case cmdStatus:
                    replyStatus(context);
                    break;
                case cmdOn:
                    context->printUpdates = true;
                    break;
                case cmdOff:
                    context->printUpdates = false;
                    break;

                case cmdColor:
                    visionParams.displayType = VDT_COLOR;
                    break;
                case cmdGray:
                    visionParams.displayType = VDT_GREY;
                    break;
                case cmdThreshold:
                    visionParams.displayType = VDT_THRESHOLD;
                    break;

//                case cmdGetAPinState:
//                    getAPinState(context);
//                    break;
//                case cmdFinishMoves:
//                    replyFinishMoves(context);
//                    break;
//                case cmdAbort:
//                    doAbort(context);
//                    break;
//                case cmdHome:
//                    doHomeAll(context);
//                    break;
//                case cmdManual:
//                    setModeManual(context);
//                    break;
//                case cmdMdi:
//                    setModeMdi(context);
//                    break;
//                case cmdEnable:
//                    estopOffAndMachineOn(context);
//                    break;
//                case cmdOpenProgram:
//                    doOpenProgram(context, originalInBuf);
//                    break;
//                case cmdShowFile:
//                    doShowFile(context);
//                    break;
//                case cmdRunProgram:
//                    doRunProgram(context);
//                    break;
//                case cmdPause:
//                    doPause(context);
//                    break;
//                case cmdResume:
//                    doResume(context);
//                    break;
//                default:
//                    doMDI(context, originalInBuf);
            }
    }

    return ret;
}

void addConnection(connectionRecType* c) {
    currentConnections.push_back(c);
}

void removeConnection(connectionRecType* c) {
    std::vector<connectionRecType*>::iterator it = std::find(currentConnections.begin(), currentConnections.end(), c);
    if (it != currentConnections.end())
        currentConnections.erase(it);
}

void *readClient(void *arg)
{
  char buf[1600];
  int context_index;
  int i;
  int len;
  connectionRecType *context = (connectionRecType *)arg;

  context_index = 0;

  while (1) {
    // We always start this loop with an empty buf, though there may be one
    // partial line in context->inBuf[0..context_index].
    len = read(context->cliSock, buf, sizeof(buf));
    if (len < 0) {
      fprintf(stderr, "Error reading from client: %s\n", strerror(errno));
      goto finished;
    }
    if (len == 0) {
      //printf("EOF from client\n"); fflush(stdout);
      goto finished;
    }

    if (context->echo && context->linked)
      if(write(context->cliSock, buf, len) != (ssize_t)len) {
        fprintf(stderr, "write() failed: %s", strerror(errno));
      }

    for (i = 0; i < len; i ++) {
        if ((buf[i] != '\n') && (buf[i] != '\r')
) {
            context->inBuf[context_index] = buf[i];
            context_index ++;
            continue;
        }

        // if we get here, i is the index of a line terminator in buf

        if (context_index > 0) {
            // we have some bytes in the context buffer, parse them now
            context->inBuf[context_index] = '\0';

            // The return value from parseCommand was meant to indicate
            // success or error, but it is unusable.  Some paths return
            // the return value of write(2) and some paths return small
            // positive integers (cmdResponseType) to indicate failure.
            // We're best off just ignoring it.
            (void)parseCommand(context);

            context_index = 0;
        }
    }
  }

finished:
  printf("Disconnecting client %s (%s)\n", context->hostName, context->version); fflush(stdout);
  close(context->cliSock);
  removeConnection(context);
  free(context);
  pthread_exit((void *)0);
  sessions--;  // FIXME: not reached
}

void* sockMain(void*)
{
    int res;

    while (1) {
      int client_sockfd;

      client_len = sizeof(client_address);
      client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_address, &client_len);
      if (client_sockfd < 0)
          exit(0);
      sessions++;
      if ((maxSessions == -1) || (sessions <= maxSessions)) {
        pthread_t *thrd;
        connectionRecType *context;

        thrd = (pthread_t *)calloc(1, sizeof(pthread_t));
        if (thrd == NULL) {
          fprintf(stderr, "Out of memory\n");

          exit(1);
        }

        context = (connectionRecType *) malloc(sizeof(connectionRecType));
        if (context == NULL) {
          fprintf(stderr, "Out of memory\n");
          exit(1);
        }

        context->cliSock = client_sockfd;
        context->linked = false;
        context->echo = true;
        context->verbose = false;
        strncpy(context->version, "1.0", VERSION_LEN-1);
        strncpy(context->hostName, "Default", HOSTNAME_LEN-1);
        context->enabled = false;
        context->commMode = 0;
        context->commProt = 0;
        context->inBuf[0] = 0;

        context->printUpdates = true;
//        batchEntries.clear();
//        insideBatch = false;

        printf("Connection received\n"); fflush(stdout);

        addConnection( context );

        res = pthread_create(thrd, NULL, readClient, (void *)context);
      } else {
        res = -1;
      }
      if (res != 0) {
        close(client_sockfd);
        sessions--;
      }
    }
}

pthread_t listenThread;

void startListening() {
    //thrd = (pthread_t *)calloc(1, sizeof(pthread_t));
    pthread_create(&listenThread, NULL, sockMain, (void*)NULL);
}

void shutdownServer() {
    //pthread_join(listenThread, NULL);
}

void doNetworkReport() {
    std::vector<connectionRecType*>::iterator it = currentConnections.begin();
    while (it != currentConnections.end()) {
        connectionRecType* context = *it;
        it++;

        if ( ! context->printUpdates )
            continue;

        Json::Value val(Json::objectValue);

        if ( br.bestblob ) {
            val["dx"] = br.bbdx;
            val["dy"] = br.bbdy;
        }

        Json::FastWriter writer;
        std::string str = writer.write( val ) + "\n";
        write(context->cliSock, str.c_str(), str.size());
    }
}
