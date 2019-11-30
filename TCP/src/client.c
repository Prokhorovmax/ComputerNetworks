
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#define SERVER_PORT 7500        /* server port */
#define SERVER_IP "127.0.0.1"   /* server ip address */
#define MESSAGE_SIZE 100        /* message buffer size */

DWORD WINAPI messageHandler(void *s);

int readN(int socket, char *buf);

int main(int argc, char **argv) {

    WSADATA lpWSAData;
    if (WSAStartup(MAKEWORD(1, 1), &lpWSAData) != 0) return 0;

    struct sockaddr_in peer;
    peer.sin_family = AF_INET;

    char inputBuf[MESSAGE_SIZE];
    int s;

    for (;;) {
        printf("ENTER IP ADDRESS (0 - default):\n");
        fflush(stdout);
        memset(inputBuf, 0, sizeof(inputBuf));
        fgets(inputBuf, sizeof(inputBuf), stdin);
        inputBuf[strlen(inputBuf) - 1] = '\0';
        if (!strcmp("0", inputBuf)) peer.sin_addr.s_addr = inet_addr(SERVER_IP);
        else peer.sin_addr.s_addr = inet_addr(inputBuf);

        printf("ENTER SERVER PORT NUMBER (0 - default):\n");
        fflush(stdout);
        memset(inputBuf, 0, sizeof(inputBuf));
        fgets(inputBuf, sizeof(inputBuf), stdin);
        inputBuf[strlen(inputBuf) - 1] = '\0';
        if (!strcmp("0", inputBuf)) peer.sin_port = htons(SERVER_PORT);
        else peer.sin_port = htons(atoi(inputBuf));

        s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == INVALID_SOCKET) {
            printf("CANNOT CREATE SOCKET.\n");
            fflush(stdout);
            continue;
        }

        int rc = connect(s, (struct sockaddr *) &peer, sizeof(peer));
        if (rc == SOCKET_ERROR) {
            printf("CANNOT CONNECT TO SERVER.\n");
            fflush(stdout);
            continue;
        } else {
            printf("CONNECTED TO SERVER SUCCESSFULLY.\n");
            fflush(stdout);
            break;
        }
    }

    HANDLE thread = CreateThread(NULL, 0, &messageHandler, &s, 0, NULL);
    if (thread == NULL) {
        printf("CANNOT CREATE LISTENING THREAD.\n");
        fflush(stdout);
        exit(1);
    } else {
        printf("MESSAGES LISTENING STARTED.\n");
        fflush(stdout);
    }

    printf("INPUT MESSAGES:\n");
    fflush(stdout);
    for (;;) {
        fgets(inputBuf, sizeof(inputBuf), stdin);
        inputBuf[strlen(inputBuf) - 1] = '\0';
        if (!strcmp("-q", inputBuf)) {
            break;
        } else {
            if (send(s, inputBuf, sizeof(inputBuf), 0) <= 0) {
                printf("CANNOT REACH THE SERVER.\n");
                break;
            }
            memset(inputBuf, 0, MESSAGE_SIZE);
        }
    }
    shutdown(s, 2);
    closesocket(s);
    WaitForSingleObject(thread, INFINITE);
    printf("CLIENT TERMINATED.\n");
    fflush(stdout);
    return 0;
}

DWORD WINAPI messageHandler(void *s) {
    int socket = *((int *) s);
    char message[MESSAGE_SIZE] = {0};
    for (;;) {
        if (readN(socket, message) <= 0) {
            break;
        } else {
            printf("MESSAGE FROM SERVER: %s\n", message);
            fflush(stdout);
        }
        memset(message, 0, sizeof(message));
    }
    printf("SOCKET CLOSED. LISTENING ENDED.\n");
    fflush(stdout);
    ExitThread(0);
}

int readN(int socket, char *buf) {
    int result = 0;
    int readedBytes = 0;
    int messageSize = MESSAGE_SIZE;
    while (messageSize > 0) {
        readedBytes = recv(socket, buf + result, messageSize, 0);
        if (readedBytes <= 0) {
            return -1;
        }
        result += readedBytes;
        messageSize -= readedBytes;
    }
    return result;
}