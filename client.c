#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ncurses.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define USERS "database"

WINDOW *message_win;
WINDOW *input_win;



int login(char* username)
{
    char line[BUFFER_SIZE];
    char in[BUFFER_SIZE];
    while(1){
        printf("Enter username: ");
        fgets(in, sizeof(in), stdin);
        in[strcspn(in, "\n")]=0;

        FILE* userdb=fopen(USERS, "r");
        if(!userdb)
        {
            perror("FOPEN");
            exit(EXIT_FAILURE);
        }

        int valid=0;

        while(fgets(line, sizeof(line), userdb))
        {
            line[strcspn(line, "\n")]=0;
            if(strcmp(line, in)==0)
            {
                valid=1;
                strcpy(username, in);
                break;
            }
        }

        fclose(userdb);
        
        if(valid)
        {
            return 1;
        }
        else
        {
            printf("ERROR! User not in db, Try again!\n");
        }
    }

}

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
    char username[1024];
    int try=0;
    int success=0;
    // printf("Enter a username: ");
    // fgets(username, sizeof(username), stdin);
    // username[strcspn(username, "\n")]=0;
    if(!login(username))
    {
        perror("LOGIN");
        exit(EXIT_FAILURE);
    }



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

    //send(sockfd, username, strlen(username), 0);

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
            mvwprintw(input_win, 1, 1, "%s: %s", username, input);
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

        if (strcmp(input, "/exit") == 0) {
            break;
        }

        char message[1024];
        snprintf(message, 1024, "<%s> %s", username, input);

        // Send message to server
        send(sockfd, message, strlen(message), 0);

        
    }

    // Cleanup
    pthread_cancel(recv_thread);
    pthread_join(recv_thread, NULL);
    close(sockfd);
    endwin();

    return 0;
}