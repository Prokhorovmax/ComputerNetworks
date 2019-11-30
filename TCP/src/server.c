
#include "server.h"

struct CLIENT *clients;
struct EVENT *events;

int numberOfClients = 0;
int numberOfEvents = 0;

HANDLE *eventThreads;
HANDLE mutex;

int main(int argc, char **argv) {

//    if (argc != 3) {
//        printf("WRONG COMMAND LINE FORMAT (server.exe ip port).\n");
//        exit(1);
//    }

    struct sockaddr_in local;
    local.sin_family = AF_INET;
//    local.sin_port = htons(atoi(argv[2]));
//    local.sin_addr.s_addr = inet_addr(argv[1]);
    local.sin_port = htons(PORT);
    local.sin_addr.s_addr = htonl(INADDR_ANY);

    WSADATA lpWSAData;
    if (WSAStartup(MAKEWORD(1, 1), &lpWSAData) != 0) return 0;

    int listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket == INVALID_SOCKET) {
        perror("CANNOT CREATE SOCKET TO LISTEN: ");
        exit(1);
    } else {
        printf("LISTENING SOCKET CREATED SUCCESSFULLY.\n");
        fflush(stdout);
    }

    int rc = bind(listeningSocket, (struct sockaddr *) &local, sizeof(local));
    if (rc == SOCKET_ERROR) {
        perror("CANNOT BIND SOCKET: ");
        exit(1);
    } else {
        printf("LISTENING SOCKET BINDED SUCCESSFULLY.\n");
        fflush(stdout);
    }

    if (listen(listeningSocket, 5) == SOCKET_ERROR) {
        perror("CANNOT LISTEN SOCKET: ");
        exit(1);
    } else {
        printf("LISTENING INPUT MESSAGES STARTED.\n");
        fflush(stdout);
    }

    mutex = CreateMutex(NULL, FALSE, NULL);
    if (mutex == NULL) {
        perror("FAILED TO CREATE MUTEX: ");
        exit(1);
    }

    // thread for listen
    HANDLE thread = CreateThread(NULL, 0, &listeningThread, &listeningSocket, 0, NULL);
    if (thread == NULL) {
        printf("CANNOT CREATE LISTENING THREAD.\n");
        fflush(stdout);
        exit(1);
    }

    // basic event from events.txt
    readEvents();

    // console commands parsing and input messages reading
    printf("WAITING FOR INPUT MESSAGES OR COMMANDS...\n");
    fflush(stdout);

    readMessages();

    shutdown(listeningSocket, 2);
    closesocket(listeningSocket);
    WaitForSingleObject(thread, INFINITE);
    printf("SERVER TERMINATED.\n");
    fflush(stdout);
    return 0;
}

void *readEvents() {
    events = (struct EVENT *) calloc(DEFAULT_EVENTS_NUMBER, sizeof(struct EVENT));
    eventThreads = (HANDLE *) calloc(DEFAULT_EVENTS_NUMBER, sizeof(HANDLE));
    FILE *file = fopen("events.txt", "r");
    if (file == NULL) {
        printf("CANNOT GET EVENT LIST FILE.\n");
        fflush(stdout);
    } else {
        while (feof(file) == 0) {
            char *string = parseStreamLine(file);
            if (!strcmp("", string)) continue;
            string = strtok(string, ":");
            int period = atoi(string);
            string = strtok(NULL, "\n");
            newEvent(period, string);
        }
    }
    fclose(file);
}

void *readMessages() {
    char buf[100];
    for (;;) {
        memset(buf, 0, 100);
        fgets(buf, 100, stdin);
        buf[strlen(buf) - 1] = '\0';

        if (!strcmp("-help", buf)) {
            printf("HELP:\n");
            printf("USE \'-l\' TO SHOW ALL ONLINE USERS.\n");
            printf("USE \'-evl\' TO SHOW ALL EXISTING EVENTS.\n");
            printf("USE \'-kick N\' TO KICK CLIENT BY NUMBER.\n");
            printf("USE \'-new_event:PER:MES\' TO START NEW EVENT.\n");
            printf("USE \'-s N\' TO START EVENT BY NUMBER.\n");
            printf("USE \'-e N\' TO CLOSE EVENT BY NUMBER.\n");
            printf("USE \'-q\' TO SHUTDOWN SERVER.\n");
            fflush(stdout);
        } else if (!strcmp("-l", buf)) {
            printf("CLIENTS ONLINE:\n");
            WaitForSingleObject(&mutex, INFINITE);
            for (int i = 0; i < numberOfClients; i++) {
                if (clients[i].socket != INVALID_SOCKET)
                    printf(" %d          %s          %d\n", clients[i].number, clients[i].address, clients[i].port);
            }
            ReleaseMutex(&mutex);
            fflush(stdout);
        } else if (!strcmp("-evl", buf)) {
            printf("\t\t\tEXISTING EVENTS:\n");
            WaitForSingleObject(&mutex, INFINITE);
            printf(" ID\tPER\t%-40s\tSUBS\n", "EVENT");
            for (int i = 0; i < numberOfEvents; i++) {
                if (events[i].repeatPeriod >= 0) {
                    int subs = 0;
                    for (int j = 0; j < events[i].numberOfSubscribers; j++) {
                        if ((events[i].clientIDs[j] != -1) &&
                            (clients[events[i].clientIDs[j]].socket != INVALID_SOCKET))
                            subs++;
                    }
                    printf(" %d\t%d\t%-40s\t%d\n", events[i].id, events[i].repeatPeriod, events[i].event, subs);
                }
            }
            printf("\n");
            ReleaseMutex(&mutex);
            fflush(stdout);
        } else if (!strcmp("-q", buf)) {
            break;
        } else {
            char *separator = " :";
            char *string = strtok(buf, separator);
            if (string == NULL) {
                printf("UNDEFINED MESSAGE.\n");
                fflush(stdout);
                continue;
            }
            if (!strcmp("-kick", string)) {
                string = strtok(NULL, separator);
                int id = atoi(string);
                if ((id < numberOfClients) && (clients[id].socket != INVALID_SOCKET)) {
                    kickClient(id);
                } else {
                    printf("INVALID KICK REQUEST.\n");
                    fflush(stdout);
                }
            } else if (!strcmp("-e", string)) {
                string = strtok(NULL, separator);
                int id = atoi(string);
                if ((id < numberOfEvents) && (events[id].repeatPeriod >= 0)) {
                    closeEvent(id);
                } else {
                    printf("INVALID DELETE REQUEST.\n");
                    fflush(stdout);
                }
            } else if (!strcmp("-new_event", string)) {
                string = strtok(NULL, ":");
                int period = atoi(string);
                string = strtok(NULL, "\n");
                newEvent(period, string);
            } else if (!strcmp("-s", string)) {
                string = strtok(NULL, separator);
                int id = atoi(string);
                if (events[id].repeatPeriod == 0) {
                    events[id].repeatPeriod = 1;
                } else {
                    printf("CANNOT START THIS EVENT.\n");
                    fflush(stdout);
                }
            } else {
                printf("UNDEFINED MESSAGE.\n");
                fflush(stdout);
            }
        }
    }
}

DWORD WINAPI listeningThread(void *info) {
    int listener = *((int *) info);

    int new, clientIndex;
    struct sockaddr_in addr;
    int length = sizeof(addr);

    for (;;) {
        new = accept(listener, (struct sockaddr *) &addr, &length);
        if (new == INVALID_SOCKET) {
            break;
        }

        WaitForSingleObject(&mutex, INFINITE);
        clients = (struct CLIENT *) realloc(clients, sizeof(struct CLIENT) * (numberOfClients + 1));
        clients[numberOfClients].socket = new;
        clients[numberOfClients].address = inet_ntoa(addr.sin_addr);
        clients[numberOfClients].port = addr.sin_port;
        clients[numberOfClients].number = numberOfClients;
        clientIndex = numberOfClients;
        clients[numberOfClients].thread = CreateThread(NULL, 0, &clientHandler, &clientIndex, 0, NULL);
        if (clients[numberOfClients].thread == NULL) {
            printf("CANNOT CREATE THREAD FOR NEW CLIENT.\n");
            fflush(stdout);
            continue;
        } else {
            printf("NEW CLIENT CONNECTED!\n");
            fflush(stdout);
        }
        ReleaseMutex(&mutex);
        numberOfClients++;
    }

    printf("SERVER TERMINATION PROCESS STARTED...\n");
    fflush(stdout);

    saveEvents();

    WaitForSingleObject(&mutex, INFINITE);
    for (int i = 0; i < numberOfEvents; i++) {
        events[i].repeatPeriod = -1;
    }
    WaitForMultipleObjects(numberOfEvents, eventThreads, TRUE, INFINITE);

    for (int i = 0; i < numberOfEvents; i++) {
        free(events[i].clientIDs);
        free(events[i].event);
    }
    free(events);
    free(eventThreads);

    for (int i = 0; i < numberOfClients; i++) {
        shutdown(clients[i].socket, 2);
        closesocket(clients[i].socket);
        clients[i].socket = INVALID_SOCKET;
        WaitForSingleObject(clients[i].thread, INFINITE);
        free(clients[i].address);
    }
    free(clients);
    ReleaseMutex(&mutex);
    printf("LISTENING ENDED.\n");
    fflush(stdout);
}

DWORD WINAPI clientHandler(void *clientIndex) {
    WaitForSingleObject(&mutex, INFINITE);
    int index = *((int *) clientIndex);
    int socket = clients[index].socket;
    ReleaseMutex(&mutex);

    char message[MESSAGE_SIZE] = {0};
    for (;;) {
        if (readN(socket, message) <= 0) {
            break;
        } else {
            printf("RECEIVED MESSAGE: %s\n", message);
            fflush(stdout);
            send(socket, message, sizeof(message), 0);

            if (!strcmp("", message)) continue;
            if (!strcmp("-evl", message)) {
                sendEventList(socket);
                continue;
            }

            char *string = strtok(message, " :");
            if (!strcmp("-new_event", string)) {
                string = strtok(NULL, ":");
                int period = atoi(string);
                string = strtok(NULL, "\n");
                newEvent(period, string);
                char confirm[MESSAGE_SIZE] = {0};
                strcat(confirm, "EVENT CREATED SUCCESSFULLY.");
                send(socket, confirm, sizeof(confirm), 0);
            } else if (!strcmp("-e", string)) {
                string = strtok(NULL, " ");
                int id = atoi(string);
                if ((id < numberOfEvents) && (events[id].repeatPeriod >= 0)) {
                    char confirm[MESSAGE_SIZE] = {0};
                    strcat(confirm, "REQUEST IS PROCESSING.");
                    send(socket, confirm, sizeof(confirm), 0);
                    closeEvent(id);
                    memset(confirm, 0, sizeof(confirm));
                    strcat(confirm, "EVENT ENDED ACCORDING TO YOUR REQUEST.");
                    send(socket, confirm, sizeof(confirm), 0);
                } else {
                    char confirm[MESSAGE_SIZE] = {0};
                    strcat(confirm, "INVALID DELETE REQUEST.");
                    send(socket, confirm, sizeof(confirm), 0);
                }
            } else if (!strcmp("-sub", string)) {
                string = strtok(NULL, " ");
                int id = atoi(string);
                if ((id < numberOfEvents) && (events[id].repeatPeriod >= 0)) {
                    if (events[id].numberOfSubscribers >= DEFAULT_SUBSCRIBERS_NUMBER) {
                        events[id].clientIDs = (int *) realloc(events[id].clientIDs,
                                                               sizeof(int) * (events[id].numberOfSubscribers + 1));
                    }
                    events[id].clientIDs[events[id].numberOfSubscribers] = index;
                    events[id].numberOfSubscribers++;
                    printf("CLIENT %d SUBSCRIBED TO EVENT %d.\n", index, id);
                    char confirm[MESSAGE_SIZE] = {0};
                    strcat(confirm, "YOU SUBSCRIBED TO EVENT SUCCESSFULLY.");
                    send(socket, confirm, sizeof(confirm), 0);
                } else {
                    char confirm[MESSAGE_SIZE] = {0};
                    strcat(confirm, "INVALID SUBSCRIBE REQUEST.");
                    send(socket, confirm, sizeof(confirm), 0);
                }
            } else if (!strcmp("-unsub", string)) {
                string = strtok(NULL, " ");
                int id = atoi(string);
                if ((id < numberOfEvents) && (events[id].repeatPeriod >= 0)) {
                    events[id].clientIDs[index] = -1;
                    printf("CLIENT %d UNSUBSCRIBED FROM EVENT %d.\n", index, id);
                    char confirm[MESSAGE_SIZE] = {0};
                    strcat(confirm, "YOU UNSUBSCRIBED SUCCESSFULLY.");
                    send(socket, confirm, sizeof(confirm), 0);
                } else {
                    char confirm[MESSAGE_SIZE] = {0};
                    strcat(confirm, "INVALID UNSUBSCRIBE REQUEST.");
                    send(socket, confirm, sizeof(confirm), 0);
                }
            }
        }
        memset(message, 0, sizeof(message));
    }

    printf("CLIENT %d DISCONNECTED.\n", index);
    fflush(stdout);
    shutdown(socket, 2);
    closesocket(socket);
    clients[index].socket = INVALID_SOCKET;
    printf("CLIENT %d HANDLER THREAD TERMINATED.\n", index);
    fflush(stdout);
    ExitThread(0);
}

DWORD WINAPI eventHandler(void *eventInfo) {
    struct EVENT *event = (struct EVENT *) eventInfo;
    char message[MESSAGE_SIZE] = {0};
    char str[5];
    itoa(event->id, str, 10);
    strcat(message, "EVENT ");
    strcat(message, str);
    strcat(message, ": ");
    strcat(message, event->event);
    if (event->repeatPeriod != 0) {
        for (;;) {
            if (event->repeatPeriod == -1) break;
            int period = event->repeatPeriod;
            for (int i = 0; i < event->numberOfSubscribers; i++) {
                if (event->clientIDs[i] != -1) {
                    if (clients[event->clientIDs[i]].socket != INVALID_SOCKET) {
                        send(clients[event->clientIDs[i]].socket, message, sizeof(message), 0);
                    }
                }
            }
            printf("%s\n", message);
            fflush(stdout);
            while (period > 0) {
                sleep(1);
                period--;
                if (event->repeatPeriod == -1) break;
            }
        }
    } else {
        for (;;) {
            if (event->repeatPeriod == -1) break;
            if (event->repeatPeriod == 1) {
                for (int i = 0; i < event->numberOfSubscribers; i++) {
                    if (event->clientIDs[i] != -1) {
                        if (clients[event->clientIDs[i]].socket != INVALID_SOCKET) {
                            send(clients[event->clientIDs[i]].socket, message, sizeof(message), 0);
                        }
                    }
                }
                event->repeatPeriod = -1;
                printf("%s\n", message);
                fflush(stdout);
                break;
            }
        }
    }
    printf("EVENT %d ENDED.\n", event->id);
    fflush(stdout);
    ExitThread(0);
}

int readN(int socket, char *buf) {
    int result = 0;
    int readBytes = 0;
    int messageSize = MESSAGE_SIZE;
    while (messageSize > 0) {
        readBytes = recv(socket, buf + result, messageSize, 0);
        if (readBytes <= 0) {
            return -1;
        }
        result += readBytes;
        messageSize -= readBytes;
    }
    return result;
}

void *kickClient(int number) {
    WaitForSingleObject(&mutex, INFINITE);
    for (int i = 0; i < numberOfClients; i++) {
        if (clients[i].number == number) {
            shutdown(clients[i].socket, 2);
            closesocket(clients[i].socket);
            clients[i].socket = INVALID_SOCKET;
            printf("CLIENT %d TERMINATED.\n", number);
            break;
        }
    }
    ReleaseMutex(&mutex);
}

void *newEvent(int period, char *string) {
    WaitForSingleObject(&mutex, INFINITE);
    if (numberOfEvents >= DEFAULT_EVENTS_NUMBER) {
        events = (struct EVENT *) realloc(events, sizeof(struct EVENT) * (numberOfEvents + 1));
        eventThreads = (HANDLE *) realloc(eventThreads, sizeof(HANDLE *) * (numberOfEvents + 1));
    }
    events[numberOfEvents].repeatPeriod = period;
    events[numberOfEvents].event = (char *) calloc(MESSAGE_SIZE, sizeof(char));
    strcat(events[numberOfEvents].event, string);
    events[numberOfEvents].id = numberOfEvents;
    events[numberOfEvents].numberOfSubscribers = 0;
    events[numberOfEvents].clientIDs = (int *) calloc(DEFAULT_SUBSCRIBERS_NUMBER, sizeof(int));
    eventThreads[numberOfEvents] = CreateThread(NULL, 0, &eventHandler, &events[numberOfEvents], 0, NULL);
    if (eventThreads[numberOfEvents] == NULL) {
        printf("CANNOT CREATE THREAD FOR NEW EVENT.\n");
        fflush(stdout);
        events[numberOfEvents].repeatPeriod = 0;
    } else {
        printf("NEW EVENT CREATED!\n");
        fflush(stdout);
    }
    ReleaseMutex(&mutex);
    numberOfEvents++;
}

void *closeEvent(int id) {
    WaitForSingleObject(&mutex, INFINITE);
    for (int i = 0; i < numberOfEvents; i++) {
        if (events[i].id == id) {
            events[i].repeatPeriod = -1;
            WaitForSingleObject(eventThreads[i], INFINITE);
            char message[MESSAGE_SIZE] = {0};
            char str[5];
            itoa(id, str, 10);
            strcat(message, "EVENT ");
            strcat(message, str);
            strcat(message, " ENDED.");
            for (int j = 0; j < events[i].numberOfSubscribers; ++j) {
                if (events[i].clientIDs[j] != -1) {
                    send(clients[events[i].clientIDs[j]].socket, message, sizeof(message), 0);
                }
            }
            free(events[i].event);
            free(events[i].clientIDs);
            break;
        }
    }
    ReleaseMutex(&mutex);
}

void *sendEventList(int socket) {
    WaitForSingleObject(&mutex, INFINITE);
    char list[MESSAGE_SIZE] = {0};
    strcat(list, "EXISTING EVENTS:");
    send(socket, list, sizeof(list), 0);
    memset(list, 0, sizeof(list));
    strcat(list, "ID\tPER\tEVENT");
    send(socket, list, sizeof(list), 0);
    for (int i = 0; i < numberOfEvents; i++) {
        if (events[i].repeatPeriod >= 0) {
            int subs = 0;
            for (int j = 0; j < events[i].numberOfSubscribers; j++) {
                if (events[i].clientIDs[j] != -1) subs++;
            }
            memset(list, 0, sizeof(list));
            char str1[3];
            itoa(events[i].id, str1, 10);
            strcat(list, str1);

            strcat(list, "\t");

            char str2[3];
            itoa(events[i].repeatPeriod, str2, 10);
            strcat(list, str2);

            strcat(list, "\t");

            strcat(list, events[i].event);
            send(socket, list, sizeof(list), 0);
        }
    }
    ReleaseMutex(&mutex);
}

void *saveEvents() {
    FILE *file = fopen("events.txt", "w");
    for (int i = 0; i < numberOfEvents; i++) {
        if (events[i].repeatPeriod >= 0) {
            fprintf(file, "%d:%s\n", events[i].repeatPeriod, events[i].event);
        }
    }
    fclose(file);
}

char *parseStreamLine(FILE *file) {
    char *string;
    int newNumber = MESSAGE_SIZE;
    int total = MESSAGE_SIZE;
    string = (char *) calloc(MESSAGE_SIZE, sizeof(char));
    if (string == NULL) {
        printf("Memory allocation error\n");
        exit(1);
    }
    fgets(string, MESSAGE_SIZE, file);
    char *firstNullSymbol = strchr(string, '\0');
    while (((firstNullSymbol - string + 1) == total) && (string[strlen(string) - 1] != '\n')) {
        newNumber = newNumber * 2;
        char *string1;
        string1 = (char *) calloc(MESSAGE_SIZE, sizeof(char));
        if (string1 == NULL) {
            printf("MEMORY ALLOCATION ERROR.\n");
            exit(1);
        }
        string1 = (char *) realloc(string1, newNumber * sizeof(char));
        if (string1 == NULL) {
            printf("MEMORY ALLOCATION ERROR.\n");
            exit(1);
        }
        char *ret = fgets(string1, newNumber, file);
        if (ret != NULL) {
            string = strcat(string, string1);
        }
        firstNullSymbol = strchr(string, '\0');
        total += (newNumber - 1);
    }
    return string;
}