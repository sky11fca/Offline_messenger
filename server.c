#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int uid=0;
int clients_count=0;

void str_trim_lf(char* arr, int length) {
    for (int i = 0; i < length; i++) {
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

void print_ip_addr(struct sockaddr_in addr) {
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

void queue_add(client_t *cl) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i]) {
            clients[i] = cl;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void queue_remove(int uid) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]) {
            if (clients[i]->uid == uid) {
                clients[i] = NULL;
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_message(char *s, int uid) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]) {
            if (clients[i]->uid != uid) {
                if (write(clients[i]->sockfd, s, strlen(s)) < 0) {
                    perror("ERROR: write to descriptor failed");
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    char buff_out[BUFFER_SIZE];
    char name[32];
    int leave_flag = 0;

    client_t *cli = (client_t *)arg;

    if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1) {
        printf("Enter the name correctly\n");
        leave_flag = 1;
    } else {
        strcpy(cli->name, name);
        sprintf(buff_out, "%s has joined\n", cli->name);
        printf("%s", buff_out);
        send_message(buff_out, cli->uid);
    }

    bzero(buff_out, BUFFER_SIZE);

    while (1) {
        if (leave_flag) {
            break;
        }
        int receive = recv(cli->sockfd, buff_out, BUFFER_SIZE, 0);
        if (receive > 0) {
            if (strlen(buff_out) > 0) {
                send_message(buff_out, cli->uid);
                str_trim_lf(buff_out, strlen(buff_out));
                printf("%s -> %s\n", cli->name, buff_out);
            }
        } else if (receive == 0 || strcmp(buff_out, "exit") == 0) {
            sprintf(buff_out, "%s has left\n", cli->name);
            printf("%s", buff_out);
            send_message(buff_out, cli->uid);
            leave_flag = 1;
        } else {
            printf("ERROR: -1\n");
            leave_flag = 1;
        }

        bzero(buff_out, BUFFER_SIZE);
    }

    close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    pthread_detach(pthread_self());

    return NULL;
}

int main() {
    int sockfd, new_sockfd;
    struct sockaddr_in server_addr, client_addr;
    pthread_t tid;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    if (listen(sockfd, 10) < 0) {
        perror("ERROR: listen");
        return 1;
    }

    printf("=== WELCOME TO THE CHAT SERVER ===\n");

    while (1) {
        socklen_t clilen = sizeof(client_addr);
        new_sockfd = accept(sockfd, (struct sockaddr*)&client_addr, &clilen);

        if ((new_sockfd) < 0) {
            perror("ERROR: accept");
            return 1;
        }

        if ((clients_count + 1) == MAX_CLIENTS) {
            printf("Max clients reached. Rejected: ");
            print_ip_addr(client_addr);
            printf(":%d\n", client_addr.sin_port);
            close(new_sockfd);
            continue;
        }

        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = client_addr;
        cli->sockfd = new_sockfd;
        cli->uid = uid++;

        queue_add(cli);
        pthread_create(&tid, NULL, &handle_client, (void*)cli);

        sleep(1);
    }

    return 0;
}
