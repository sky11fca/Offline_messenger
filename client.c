#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

// Function to handle receiving messages from the server
void *receive_messages(void *arg) {
    int sockfd = *(int *)arg;
    char buffer[1024];
    int bytes_received;

    while ((bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
    }

    if (bytes_received == 0) {
        printf("Server disconnected.\n");
    } else {
        perror("recv");
    }

    close(sockfd);
    exit(EXIT_FAILURE);
    return NULL;
}

int main() {
    int sockfd;
    struct sockaddr_in server_address;
    char message[1024];

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to the server.\n");

    // Create a thread to handle receiving messages
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_messages, &sockfd);

    // Main loop to send messages to the server
    while (1) {
        fgets(message, sizeof(message), stdin);

        if (send(sockfd, message, strlen(message), 0) < 0) {
            perror("send");
            break;
        }
    }

    close(sockfd);
    return 0;
}
