#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define NUM_PROCESSES 6

typedef struct _query {
    int user;
    int action;
    int seat;
} query;

void wait_until_six(int process_id, struct sockaddr_in server_addr) {
    int connfd = socket(PF_INET, SOCK_STREAM, 0);
    if (connect(connfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1) {
		printf("Connection failed\n");
		exit(1);
    }

    while (1) {
        // 현재 시간 가져오기
        time_t now = time(NULL);
        struct tm *local_time = localtime(&now);

        // __시 45분이 지났는지 확인
        if (local_time->tm_min >= 45) {
            printf("매크로 %d: 티켓팅 돌입!\n", process_id);

            int response;
            query q = {process_id, 1, 1234};
            send(connfd, &q, sizeof(q), 0);
            recv(connfd, &response, sizeof(int), 0);
            printf("매크로 %d: 로그인 성공!\n", process_id);

            // 0번 좌석부터 순차적으로 예약 시도
            for (int i = 0; i < NUM_PROCESSES; i++) {
                q.action = 2;
                q.seat = i;
                send(connfd, &q, sizeof(q), 0);
                recv(connfd, &response, sizeof(int), 0);
                if (response != -1) {
                    printf("\033[33m매크로 %d: %d번 좌석 예약 완료!\033[0m\n", process_id, i);
                    break;
                } else {
                    printf("매크로 %d: %d번 좌석 예약 실패!\n", process_id, i);
                }
            }
            break;
        }

        // 대기 (0.1초)
        sleep(0.1);
    }

    query t = {0, 0, 0};
    send(connfd, &t, sizeof(t), 0);

    close(connfd);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 3) { // help message
		printf("argv[1]: server address, argv[2]: port number\n");
		exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    // NUM_PROCESSES 개수만큼 프로세스 생성
    pid_t pids[NUM_PROCESSES];
    for (int i = 0; i < NUM_PROCESSES; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork 실패");
            exit(1);
        } else if (pids[i] == 0) { // 자식 프로세스에서 실행
            wait_until_six(i + 1, server_addr);
            exit(0); // 자식 프로세스 종료
        }
    }

    // 부모 프로세스에서 모든 자식 프로세스 종료 대기
    for (int i = 0; i < NUM_PROCESSES; i++) {
        waitpid(pids[i], NULL, 0);
    }

    printf("모든 매크로가 좌석 예약을 완료했습니다.\n");
    printf("\n");
    printf("좌석 예약 현황: \n");

    int connfd = socket(PF_INET, SOCK_STREAM, 0);
    if (connect(connfd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1) {
		printf("Connection failed\n");
		exit(1);
    }
    query t = {0, 0, 0};
    send(connfd, &t, sizeof(t), 0);
    int arr[256];
    recv(connfd, arr, sizeof(arr), 0);
    close(connfd);

    for(int i = 0; i < NUM_PROCESSES; i++) printf("%d ", arr[i]);
    printf("\n");

    return 0;
}
