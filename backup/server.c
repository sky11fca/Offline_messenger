#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8080
#define MAX_CLIENTS 1024
#define FILE_NAME ".logs.log"

int client_sockets[MAX_CLIENTS];
pthread_mutex_t client_sockets_mutex;
pthread_mutex_t file_mutex;

//reply function. last message
void get_last_line(char* buffer)
{
    FILE* file=fopen(FILE_NAME, "r");
    if(!file){
        strcpy(buffer, "No messages avalable!");
        return;
    }

    char line[1024];
    while(fgets(line, sizeof(line), file))
    {
        strcpy(buffer, line);
    }
    fclose(file);

    size_t len=strlen(buffer);
    if(len>0 && buffer[len-1]=='\n')
    {
        buffer[len-1]='\0';
    }
}

// Function to save messages to the file
void save_message_to_file(const char *message) {
    pthread_mutex_lock(&file_mutex);
    FILE *file = fopen(FILE_NAME, "a");
    if (file) {
        fprintf(file, "%s\n", message);
        fclose(file);
    } else {
        perror("Failed to open file");
    }
    pthread_mutex_unlock(&file_mutex);
}

// Function to get the contents of the log file
void get_log_file(char *log_contents, size_t size) {
    pthread_mutex_lock(&file_mutex);
    FILE *file = fopen(FILE_NAME, "r");
    if (file) {
        fread(log_contents, 1, size, file);
        fclose(file);
    } else {
        perror("Failed to open file");
        strcpy(log_contents, "Log file is empty or unavailable.\n");
    }
    pthread_mutex_unlock(&file_mutex);
}

// Function to broadcast a message to all clients
void broadcast_message(const char *message, int sender_fd) {
    pthread_mutex_lock(&client_sockets_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != -1) {
            send(client_sockets[i], message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&client_sockets_mutex);
}

// Function to handle client communication
void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char buffer[1024];
    int bytes_read;

    // Send log file contents to the client upon connection
    char log_contents[4096] = {0};
    get_log_file(log_contents, sizeof(log_contents));
    send(client_fd, log_contents, strlen(log_contents), 0);

    while ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        printf("Client %d: %s", client_fd, buffer);
        if(strcmp(buffer, "/exit")){
            if (strncmp(buffer, "/r ", 3) == 0) {
                char last_message[1024];
                //get_log_file(last_message, sizeof(last_message));
                get_last_line(last_message);
                char response[2048];
                snprintf(response, sizeof(response), "Replied to %s -> %s", last_message, buffer + 3);
                broadcast_message(response, client_fd);
                save_message_to_file(response);
            } else {
                broadcast_message(buffer, client_fd);
                save_message_to_file(buffer);
            }
        }
    }

    if (bytes_read == 0) {
        printf("Client %d disconnected.\n", client_fd);
    } else {
        perror("recv");
    }

    close(client_fd);

    // Remove the client from the list
    pthread_mutex_lock(&client_sockets_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == client_fd) {
            client_sockets[i] = -1;
            break;
        }
    }
    pthread_mutex_unlock(&client_sockets_mutex);

    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    fd_set readfds;

    // Initialize client sockets array
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_sockets[i] = -1;
    }

    pthread_mutex_init(&client_sockets_mutex, NULL);
    pthread_mutex_init(&file_mutex, NULL);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        pthread_mutex_lock(&client_sockets_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] > 0) {
                FD_SET(client_sockets[i], &readfds);
                if (client_sockets[i] > max_sd) {
                    max_sd = client_sockets[i];
                }
            }
        }
        pthread_mutex_unlock(&client_sockets_mutex);

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("select error");
        }

        // Check for new connections
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            printf("New connection: socket fd %d, IP %s, port %d\n",
                   new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            pthread_mutex_lock(&client_sockets_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == -1) {
                    client_sockets[i] = new_socket;
                    break;
                }
            }
            pthread_mutex_unlock(&client_sockets_mutex);

            int *pclient = malloc(sizeof(int));
            *pclient = new_socket;
            pthread_t tid;
            pthread_create(&tid, NULL, handle_client, pclient);
            pthread_detach(tid);
        }
    }

    close(server_fd);
    pthread_mutex_destroy(&client_sockets_mutex);
    pthread_mutex_destroy(&file_mutex);
    return 0;
}
