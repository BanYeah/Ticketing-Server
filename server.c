#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>

#define SEAT 256
#define CLIENT 1024

void* thread_func(void *arg);
int main(int argc, char *argv[]) {
    if (argc < 3) { // help message
		printf("argv[1]: server address, argv[2]: port number\n");
		exit(1);
    }

    struct sockaddr_in saddr, caddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(atoi(argv[2]));
    saddr.sin_addr.s_addr = inet_addr(argv[1]);

    int listenfd;
    if ((listenfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        printf("socket() failed\n");
		exit(1);
    } else if (bind(listenfd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        printf("bind() failed\n");
		exit(1);
    } else if (listen(listenfd, SOMAXCONN) < 0) {
        printf("listen() failed\n");
		exit(1);
    }

    while (1) {
        int caddrlen = sizeof(caddr);
        int* connfd = (int*)malloc(sizeof(int));
        if ((*connfd = accept(listenfd, (struct sockaddr*)&caddr, (socklen_t*)&caddrlen)) < 0) {
            perror("accept() failed\n");
            free(connfd);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, thread_func, connfd);
    }
}

void* thread_func(void *arg) {
    pthread_detach(pthread_self());

    int connfd = *((int*)arg);
    printf("connect\n");
    free(arg);

    close(connfd);
    pthread_exit(NULL);
}