#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <resolv.h>
#include <netdb.h>

// run 2 programs using fork + exec
// command: make clean && make all && ./partb

#define ICMP_HDRLEN 8
#define WATCHDOG_IP "127.0.0.1"
#define WATCHDOG_PORT 3000
#define true 1

unsigned short calculate_checksum(unsigned short *paddress, int len) {
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    for (; len > 1; len -= 2) {
        sum += *w++;
    }
    if (len == 1) {
        *((unsigned char *) &answer) = *((unsigned char *) w);
        sum += answer;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = ~sum;

    return answer;
}

int main(int argc, char *strings[]) {
    if (argc != 2) {
        printf("usage: %s <addr>\n", strings[0]);
        exit(0);
    }

    int tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSocket == -1) {
        printf("Socket not created: %d\n", errno);
    }

    struct sockaddr_in watchdogAddr;
    memset(&watchdogAddr, 0, sizeof(watchdogAddr));
    watchdogAddr.sin_family = AF_INET;
    watchdogAddr.sin_port = htons(WATCHDOG_PORT);
    int ip_addr = inet_pton(AF_INET, (const char *) WATCHDOG_IP, &watchdogAddr.sin_addr);
    if (ip_addr < 1)
        printf("inet_pton() failed %d: ", errno);

    struct hostent *hname;
    hname = gethostbyname(strings[1]);

    struct sockaddr_in destAddr;
    bzero(&destAddr, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = 0;
    destAddr.sin_addr.s_addr = *(long *) hname->h_addr;

    int rawSocket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (rawSocket < 0) {
        perror("socket");
        return -1;
    }

    int ttl = 255;
    int sockopt = setsockopt(rawSocket, SOL_IP, IP_TTL, &ttl, sizeof(ttl));
    if (sockopt != 0) {
        perror("setsockopt");
        return -1;
    }

    char *args[2];
    args[0] = "./watchdog";
    args[1] = NULL;
    int pid = fork();

    if (pid == 0) {
        //printf("In child\n");
        execvp(args[0], args);
    } else {
        sleep(2);
        int connectionStatus = connect(tcpSocket, (struct sockaddr *) &watchdogAddr, sizeof(watchdogAddr));
        if (connectionStatus == -1) {
            printf("Socket not connected: %d", errno);
        }

        int flag = 0;
        while (!flag) {
            recv(tcpSocket, &flag, 1, 0);
        }

        send(tcpSocket, strings[1], sizeof(strings[1]), 0);
        char data[IP_MAXPACKET] = "Ping.\n";
        int dataLength = (int) strlen(data) + 1;
        char packet[IP_MAXPACKET];
        int seq = 0;
        struct timeval start, end;
        char pingStatus[5];
        // to check watchdog
        int counter = 0;

        while (true) {
            counter++;
            struct icmp icmphdr;
            icmphdr.icmp_type = ICMP_ECHO;
            icmphdr.icmp_code = 0;
            icmphdr.icmp_id = 18;
            icmphdr.icmp_seq = seq++;
            icmphdr.icmp_cksum = 0;

            bzero(packet, IP_MAXPACKET);
            memcpy((packet), &icmphdr, ICMP_HDRLEN);
            memcpy(packet + ICMP_HDRLEN, data, dataLength);
            icmphdr.icmp_cksum = calculate_checksum((unsigned short *) (packet), ICMP_HDRLEN + dataLength);
            memcpy((packet), &icmphdr, ICMP_HDRLEN);

            gettimeofday(&start, 0);
            sendto(rawSocket, packet, ICMP_HDRLEN + dataLength, 0, (struct sockaddr *) &destAddr, sizeof(destAddr));

            strcpy(pingStatus, "ping");
            send(tcpSocket, pingStatus, sizeof(pingStatus), 0);

            // to check watchdog
            if (counter > 5)
                sleep(10);
//
            bzero(packet, IP_MAXPACKET);
            int replyPID = fork();

            if (replyPID == 0) {
                recv(tcpSocket, &packet, sizeof(packet), 0);
                if (strcmp("Timeout", packet) == 0) {
                    printf("Received timeout.\n");
                    int myPid = getppid();
                    kill(myPid, SIGTERM);
                    exit(0);
                }
            } else {
                socklen_t length = sizeof(destAddr);
                int bytesReceived = (int) recvfrom(rawSocket, packet, sizeof(packet), 0, (struct sockaddr *) &destAddr,
                                                   &length);
                if (bytesReceived > 0) {
                    gettimeofday(&end, 0);

                    strcpy(pingStatus, "pong");
                    send(tcpSocket, pingStatus, sizeof(pingStatus), 0);

                    float time = (float) (end.tv_sec - start.tv_sec) * 1000.0f +
                                 (float) (end.tv_usec - start.tv_usec) / 1000.0f;
                    printf("Ping returned: %d bytes from IP = %s, Seq = %d, time = %.3f ms\n", bytesReceived,
                           strings[1], seq, time);
                    sleep(1);
                }
            }
        }
        close(tcpSocket);
        close(rawSocket);
    }
    return 0;
}