#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ncurses.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024

WINDOW *message_win;
WINDOW *input_win;

void *receive_messages(void *socket_fd) {
    int sockfd = *(int *)socket_fd;
    char buffer[BUFFER_SIZE];

    while (1) {
        int bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';

            // Display received messages in the upper window
            wprintw(message_win, "%s\n", buffer);
            wrefresh(message_win);
        } else if (bytes_received == 0) {
            break;
        } else {
            perror("recv");
            break;
        }
    }
    return NULL;
}

void init_windows() {
    int row, col;
    getmaxyx(stdscr, row, col);

    message_win = newwin(row - 3, col, 0, 0);
    input_win = newwin(3, col, row - 3, 0);

    scrollok(message_win, TRUE);
    box(input_win, 0, 0);

    wrefresh(message_win);
    wrefresh(input_win);
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    pthread_t recv_thread;

    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    // Initialize windows
    init_windows();

    // Setup connection
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    // Start thread to receive messages
    pthread_create(&recv_thread, NULL, receive_messages, (void *)&sockfd);

    // Input area in ncurses
    char input[BUFFER_SIZE];
    int ch, pos = 0;

    while (1) {
        memset(input, 0, BUFFER_SIZE);
        pos = 0;

        while (1) {
            werase(input_win);
            box(input_win, 0, 0);
            mvwprintw(input_win, 1, 1, "You: %s", input);
            wrefresh(input_win);

            ch = wgetch(input_win);

            if (ch == '\n') {
                break;
            } else if (ch == 127 || ch == KEY_BACKSPACE) { // Handle backspace
                if (pos > 0) {
                    input[--pos] = '\0';
                }
            } else if (pos < BUFFER_SIZE - 1) {
                input[pos++] = ch;
                input[pos] = '\0';
            }
        }

        // Send message to server
        send(sockfd, input, strlen(input), 0);

        // Exit on '/exit'
        if (strcmp(input, "/exit") == 0) {
            break;
        }
    }

    // Cleanup
    pthread_cancel(recv_thread);
    pthread_join(recv_thread, NULL);
    close(sockfd);
    endwin();

    return 0;
}