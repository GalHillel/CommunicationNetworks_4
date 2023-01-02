#include <stdio.h>
#include <sys/time.h>
#include <signal.h>

#define true 1

int main() {
    printf("hello partb");
    struct timeval start, end;
    long time;
    gettimeofday(&start, NULL);

    while (true) {
        gettimeofday(&end, NULL);
        time = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
        if (time >= 10000)
        {
            break;
        }
    }
    printf("server can't be reached\n");
    kill(0, SIGINT);
    return 0;
}