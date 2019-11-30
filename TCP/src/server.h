
#ifndef SERVER_SERVER_H
#define SERVER_SERVER_H

#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <unistd.h>

#define PORT 7500                     /* server port */
#define MESSAGE_SIZE 100              /* message buffer size */
#define DEFAULT_EVENTS_NUMBER 15
#define DEFAULT_SUBSCRIBERS_NUMBER 5

struct CLIENT {
    int number;
    HANDLE thread;
    int socket;
    char *address;
    int port;
};

struct EVENT {
    int id;
    int repeatPeriod;                /* in seconds, 0 - one-time event */
    char *event;
    int numberOfSubscribers;
    int *clientIDs;                  /* all subscribed clients (array of IDs) */
};

DWORD WINAPI listeningThread(void *info);

DWORD WINAPI clientHandler(void *clientIndex);

DWORD WINAPI eventHandler(void *eventInfo);

int readN(int socket, char *buf);

void *readEvents();

void *newEvent(int period, char *string);

void *readMessages();

void *kickClient(int number);

void *closeEvent(int id);

void *sendEventList(int socket);

void *saveEvents();

char *parseStreamLine(FILE *file);

#endif //SERVER_SERVER_H
