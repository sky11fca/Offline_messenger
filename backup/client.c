#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void *receive_handler(void *sock) {
    int sockfd = *((int *)sock);
    char buffer[BUFFER_SIZE];
    while (1) {
        int receive = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (receive > 0) {
            printf("%s\n", buffer);
        } else if (receive == 0) {
            break;
        } else {
            perror("recv");
        }
        memset(buffer, 0, BUFFER_SIZE);
    }
    return NULL;
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    pthread_t recv_thread;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    printf("Connected to server!\n");

    pthread_create(&recv_thread, NULL, receive_handler, (void*)&sockfd);

    while (1) {
        fgets(buffer, BUFFER_SIZE, stdin);
        send(sockfd, buffer, strlen(buffer), 0);
        memset(buffer, 0, BUFFER_SIZE);
    }

    close(sockfd);

    return 0;
}
