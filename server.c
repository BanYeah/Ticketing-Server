#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
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
    int data;
} query;

typedef struct _user {
    int state; // 0: not registered, 1: log-in, 2: log-out
    int passcode;
} user;

int listenfd;
pthread_mutex_t mutex_user, mutex_seat;

user user_info[USER] = {0};
int reserv[SEAT];

void sigint_handler(int signo);
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

    signal(SIGINT, sigint_handler);
    while (1) {
        int caddrlen = sizeof(caddr);
        int* connfd = (int*)malloc(sizeof(int));
        if ((*connfd = accept(listenfd, (struct sockaddr*)&caddr, (socklen_t*)&caddrlen)) < 0) {
            printf("accept() failed\n");
            free(connfd);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, thread_func, connfd);
    }
}

void sigint_handler(int signo) {
    pthread_mutex_destroy(&mutex_user);
    pthread_mutex_destroy(&mutex_seat);
    close(listenfd);
    exit(0);
}

void* thread_func(void *arg) {
    pthread_detach(pthread_self());

    int connfd = *((int*)arg), user_id = -1;
    free(arg);

    int flag = 1, ret;
    while (flag) {
        query q;
        recv(connfd, &q, sizeof(q), 0);
        // printf("%d %d %d\n", q.user, q.action, q.data);

        switch (q.action) {
        case 1: // log-in
            if (user_id != -1 ||               // client already log-in
                q.user < 0 || q.user > USER) { // out-of-range
                ret = -1;
                send(connfd, &ret, sizeof(int), 0);
                break; 
            }

            pthread_mutex_lock(&mutex_user);
            switch (user_info[q.user].state) {
            case 0: // not registered
                user_id = q.user;
                user_info[q.user].state = 1;
                user_info[q.user].passcode = q.data;
                ret = 1;
                send(connfd, &ret, sizeof(int), 0);
                break;

            case 1: // log-in
                ret = -1;
                send(connfd, &ret, sizeof(int), 0);
                break;

            case 2: // log-out
                if (user_info[q.user].passcode == q.data) { // passcode correct!
                    user_id = q.user;
                    user_info[q.user].state = 1;
                    ret = 1;
                    send(connfd, &ret, sizeof(int), 0);
                } else {
                    ret = -1;
                    send(connfd, &ret, sizeof(int), 0);
                }
                break;
            }
            pthread_mutex_unlock(&mutex_user);
            break;

        case 2: // reserve
            if (user_id == -1 ||               // client before log-in
                q.user != user_id ||           // user mismatch
                q.data < 0 || q.data > SEAT) { // out-of-range 
                ret = -1;
                send(connfd, &ret, sizeof(int), 0);
                break; 
            }

            pthread_mutex_lock(&mutex_seat);
            if (reserv[q.data] == -1) { // not reserved
                reserv[q.data] = user_id;
                send(connfd, &q.data, sizeof(q.data), 0);
            } else {
                ret = -1;
                send(connfd, &ret, sizeof(int), 0);
            }
            pthread_mutex_unlock(&mutex_seat);
            break;

        case 3: // check reservation
            if (user_id == -1 ||     // before log-in
                q.user != user_id) { // user mismatch
                ret = -1;
                send(connfd, &ret, sizeof(int), 0);
                break; 
            }

            pthread_mutex_lock(&mutex_seat);
            int reserv_seat = -1;
            for (int i = 0; i < SEAT; i++) {
                if (reserv[i] == user_id) {
                    reserv_seat = i;
                    break;
                }
            }
            send(connfd, &reserv_seat, sizeof(int), 0);
            pthread_mutex_unlock(&mutex_seat);
            break;

        case 4: // cancel reservation
            if (user_id == -1 ||               // before log-in
                q.user != user_id ||           // user mismatch
                q.data < 0 || q.data > SEAT) { // out-of-range 
                ret = -1;
                send(connfd, &ret, sizeof(int), 0);
                break; 
            }

            pthread_mutex_lock(&mutex_seat);
            int cancel_seat = -1;
            for (int i = 0; i < SEAT; i++) {
                if (reserv[i] == user_id) {
                   reserv[i] = -1; 
                   cancel_seat = i;
                   break;
                }
            }
            send(connfd, &cancel_seat, sizeof(int), 0);
            pthread_mutex_unlock(&mutex_seat);
            break;

        case 5: // log-out
            if (user_id == -1 ||     // before log-in
                q.user != user_id) { // user mismatch
                ret = -1;
                send(connfd, &ret, sizeof(int), 0);
                break; 
            }

            pthread_mutex_lock(&mutex_user);
            user_info[user_id].state = 2;
            user_id = -1;
            ret = 1;
            send(connfd, &ret, sizeof(int), 0);
            pthread_mutex_unlock(&mutex_user);
            break;
        
        case 0:
            if (!(q.user || q.data)) { // After recv (0, 0, 0)
                pthread_mutex_lock(&mutex_seat);
                send(connfd, reserv, sizeof(reserv), 0);
                pthread_mutex_unlock(&mutex_seat);
                flag = 0;
                break;
            }
        default:
            ret = -1;
            send(connfd, &ret, sizeof(int), 0);
        }
    }

    close(connfd);
    pthread_exit(NULL);
}