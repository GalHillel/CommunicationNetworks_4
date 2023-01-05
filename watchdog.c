#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

#define WATCHDOG_PORT 3000
#define MAX_CLIENT_ADDR_LEN 16
#define PING_STATUS_LEN 5
#define true 1

int main() {
    printf("hello partb\n");

    int watchdogSock = socket(AF_INET, SOCK_STREAM, 0);
    if (watchdogSock == -1) {
        printf("Socket not created: %d", errno);
        exit(0);
    }

    int enable = 1, status;
    status = setsockopt(watchdogSock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (status == -1) {
        printf("setsockopt() failed with error code : %d", errno);
        close(watchdogSock);
        exit(0);
    }

    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(WATCHDOG_PORT);

    int bindStatus = bind(watchdogSock, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
    if (bindStatus == -1) {
        printf("Bind failed with error code : %d", errno);
        close(watchdogSock);
        exit(0);
    }

    int listenStat = (int) listen(watchdogSock, 5);
    if (listenStat == -1) {
        printf("Listen failed with error code : %d", errno);
        close(watchdogSock);
        exit(0);
    }

    struct sockaddr_in clientAddress;
    socklen_t clientAddressLen = sizeof(clientAddress);
    memset(&clientAddress, 0, clientAddressLen);

    int clientSocket = accept(watchdogSock, (struct sockaddr *) &clientAddress, &clientAddressLen);
    if (clientSocket == -1) {
        printf("listenStat failed with error code : %d", errno);
        close(clientSocket);
        exit(0);
    }

    int flag1 = 1;
    send(clientSocket, &flag1, 1, 0);

    char destIP[MAX_CLIENT_ADDR_LEN];
    bzero(destIP, MAX_CLIENT_ADDR_LEN);
    int receive = (int) recv(clientSocket, destIP, MAX_CLIENT_ADDR_LEN, 0);
    if (receive < 1) {
        printf("Destination IP wasn't received correctly");
        close(clientSocket);
        return -1;
    }

    char pingStatus[PING_STATUS_LEN];
    int timer = 0;

    while (true) {
        recv(clientSocket, pingStatus, PING_STATUS_LEN, 0);
        if (strcmp("ping", pingStatus) == 0) {
            //printf("Started timer\n");
            fcntl(clientSocket, F_SETFL, O_NONBLOCK);
            while (timer < 10) {
                timer++;
                recv(clientSocket, pingStatus, PING_STATUS_LEN, 0);
                if (strcmp("pong", pingStatus) == 0) {
                    timer = 0;
                    fcntl(clientSocket, F_SETFL, 0);
                    break;
                }
                sleep(1);
            }
            if (timer == 10) {
                printf("Disconnected\n");
                close(clientSocket);
                break;
            }
        }
    }

    return 0;
}

