#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

// command: make clean && make all && ./partb

#define IP4_HDRLEN 20
#define ICMP_HDRLEN 8
#define true 1


unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *) buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int createPacket(char *packet, int seq) {
    struct icmp icmphdr;

    icmphdr.icmp_type = ICMP_ECHO;
    icmphdr.icmp_code = 0;
    icmphdr.icmp_cksum = 0;
    icmphdr.icmp_id = 24;
    icmphdr.icmp_seq = seq;

    memcpy((packet), &icmphdr, ICMP_HDRLEN);

    char data[IP_MAXPACKET] = "ping\n";
    int datalen = (int) strlen(data) + 1;
    memcpy(packet + ICMP_HDRLEN, data, datalen);

    icmphdr.icmp_cksum = checksum((unsigned short *) (packet), ICMP_HDRLEN + datalen);
    memcpy((packet), &icmphdr, ICMP_HDRLEN);

    return ICMP_HDRLEN + datalen;
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage:sudo ./partb <IP>\n");
        return 0;
    }

    char IP[INET_ADDRSTRLEN];
    strcpy(IP, argv[1]);
    struct in_addr addr;
    if (inet_pton(AF_INET, IP, &addr) != 1) {
        printf("Invalid IP-address\n");
        return 0;
    }

    int count = 0;
    char packet[IP_MAXPACKET];
    int packetlen = createPacket(packet, count);

    struct sockaddr_in dest_in;
    memset(&dest_in, 0, sizeof(struct sockaddr_in));
    dest_in.sin_family = AF_INET;

    dest_in.sin_addr.s_addr = inet_addr(IP);

    int sock;
    if ((sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1) { return -1; }

    char *args[2];

    // compiled watchdog.c by makefile
    args[0] = "./watchdog";
    args[1] = NULL;
    //int c = 0;
    while (true) {
        //c++;
        // fork a child process for watchdog
        int pid = fork();
        if (pid == 0) {
            printf("in child \n");
            // execute watchdog
            execvp(args[0], args);
        } else {
            struct timeval start, end;
            gettimeofday(&start, 0);

            int bytes_sent = (int) sendto(sock, packet, packetlen, 0, (struct sockaddr *) &dest_in, sizeof(dest_in));
            if (bytes_sent == -1) { return -1; }

            bzero(packet, IP_MAXPACKET);
            socklen_t len = sizeof(dest_in);
            ssize_t bytesRec;
            while ((bytesRec = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr *) &dest_in, &len))) {
                if (bytesRec > 0) {
                    break;
                }
            }
            //if (c > 5)
            //    sleep(10);
            gettimeofday(&end, 0);
            kill(pid, SIGKILL);

            char reply[IP_MAXPACKET];
            memcpy(reply, packet + ICMP_HDRLEN + IP4_HDRLEN, packetlen - ICMP_HDRLEN); // get reply data from packet
            float time = (float) ((float) (end.tv_sec - start.tv_sec) * 1000.0 +
                                  (float) (end.tv_usec - start.tv_usec) / 1000.0);
            printf("%zd bytes from: %s seq: %d time: %0.3f ms\n", bytesRec, IP, count, time);
            count++;
            bzero(packet, IP_MAXPACKET);

            sleep(1);
        }
    }

    close(sock);
    return 0;
}