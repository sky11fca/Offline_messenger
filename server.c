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
#define MAX_BUFFER 1024
#define USERS "database"
#define FRIENDS "friendlist"

int client_sockets[MAX_CLIENTS];
pthread_mutex_t client_sockets_mutex;
pthread_mutex_t file_mutex;

int in_friendlist(const char* name)
{
    char line[MAX_BUFFER];
    FILE* friend_list=fopen(FRIENDS, "r");
    if(!friend_list)
    {
        perror("FOPEN");
        exit(EXIT_FAILURE);
    }

    while(fgets(line, sizeof(line), friend_list))
    {
        line[strcspn(line, "\n")]='\0';

        if(strcmp(line, name)==0)
        {
            fclose(friend_list);
            return 1;
        }
    }
    fclose(friend_list);
    return 0;
}

int in_database(const char* name)
{
    char line[MAX_BUFFER];
    FILE* userdb=fopen(USERS, "r");
    if(!userdb)
    {
        perror("FOPEN");
        exit(EXIT_FAILURE);
    }

    while(fgets(line, sizeof(line), userdb))
    {
        line[strcspn(line, "\n")]='\0';

        if(strcmp(line, name)==0)
        {
            fclose(userdb);
            return 1;
        }
    }
    fclose(userdb);
    return 0;
}


void login(int client_fd, const char* username)
{
    int ok=0;
    FILE* userdb=fopen(USERS, "r");
    if(!userdb)
    {
        perror("FOPEN");
        exit(EXIT_FAILURE);
    }

    char line[MAX_BUFFER];

    while(fgets(line, sizeof(line), userdb))
    {
        line[strcspn(line, "\n")]='\0';
        if(strcmp(line, username))
        {
            ok=1;
            break;
        }
    }
    fclose(userdb);

    if(ok)
    {
        send(client_fd, "LOGIN_OK", 9, 0);
    }
    else
    {
        FILE* userdb2=fopen(USERS, "a");
        fprintf(userdb2, "%s\n", username);
        fclose(userdb2);

        send(client_fd, "LOGIN_SIGNUP", 13, 0);
    }
}

void friendlist(int client_fd, const char* username, const char* receiver)
{
    //receiver in friendlist -> CONTACT_OK
    //receiver not in friend list but in database -> CONTACT_SIGNUP
    //receiver not in both friend list and database -> CONTACT_ERR

    if(in_friendlist(receiver))
    {
        send(client_fd, "CONTACT_OK", 11, 0);
    }
    else if(!in_friendlist(receiver) && in_database(receiver))
    {
        FILE* friend_list=fopen(FRIENDS, "a");
        fprintf(friend_list, "%s\n", receiver);
        fclose(friend_list);
        send(client_fd, "CONTACT_SIGNUP", 15, 0);
    }
    else
    {
        send(client_fd, "CONTACT_ERR", 12, 0);
    }


}

// Function to broadcast a message to all clients
void broadcast_message(const char *message, int sender_fd, const char* username, const char* respondent) {
    pthread_mutex_lock(&client_sockets_mutex);
    char finalbuff[1024];

    snprintf(finalbuff, sizeof(finalbuff), "<%s> %s", username, message);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != -1) {
            send(client_sockets[i], finalbuff, strlen(finalbuff), 0);
        }
    }
    pthread_mutex_unlock(&client_sockets_mutex);
}

// Function to handle client communication
void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char buffer[1024];
    char username[1024];
    char respondent[1024];
    char message[1024];

    int bytes_read;


    while ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';

        if(strncmp(buffer, "LOGIN:", 6)==0)
        {
            sscanf(buffer, "LOGIN:%s", username);
            printf("LOGIN CLIENT -> %s\n", username);
            login(client_fd, username);

        }
        else if(strncmp(buffer, "CONTACT:", 8)==0)
        {
            sscanf(buffer, "CONTACT:%[^:]:%s", username, respondent);
            printf("CONTACT CLIENT -> %s, %s\n", username, respondent);
            friendlist(client_fd, username, respondent);

        }
        else if(strncmp(buffer, "MESSAGE:", 8)==0)
        {
            sscanf(buffer, "MESSAGE:%[^:]:%[^:]:%[^\n]", username, respondent, message);
            printf("MESSAGE CLIENT -> %s, %s : %s\n", username, respondent, message);
            broadcast_message(message, client_fd, username, respondent);
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