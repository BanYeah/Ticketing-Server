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
#define USER 1024

typedef struct _query {
    int user;
    int action;
    int seat;
} query;

typedef struct _user {
    int user_state; // 0: not registered, 1: log-in, 2: log-out
    int passcode;
} user;

pthread_mutex_t mutex_user, mutex_seat;
user user_info[USER] = {0};
int reserv[SEAT];

void* thread_func(void *arg);
int main(int argc, char *argv[]) {
    for (int i = 0; i < SEAT; i++) reserv[i] = -1;
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

    pthread_mutex_init(&mutex_user, NULL);
    pthread_mutex_init(&mutex_seat, NULL);

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

    pthread_mutex_destroy(&mutex_user);
    pthread_mutex_destroy(&mutex_seat);
    pthread_exit(NULL);
}

void* thread_func(void *arg) {
    pthread_detach(pthread_self());

    int connfd = *((int*)arg);
    free(arg);

    while (1) {
        query q;
        recv(connfd, &q, sizeof(q), 0);
        printf("%d %d %d\n", q.user, q.action, q.seat);
    }

    close(connfd);
    pthread_exit(NULL);
}