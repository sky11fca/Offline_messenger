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
#define FRIENDS "friendlist"

WINDOW *message_win;
WINDOW *input_win;

//TODO
//FIX CLIENT BROADCASTS TO ALL CLIENT MESSAGES BUG
//USE SQL DATABASE TO 

void get_friends_list()
{
    char line[BUFFER_SIZE];
    FILE* contacts=fopen(FRIENDS, "r");

    if(!contacts)
    {
        perror("FOPEN");
        exit(EXIT_FAILURE);
    }

    while(fgets(line, sizeof(line), contacts))
    {
        printf("%s", line);
    }

    fclose(contacts);
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
    char respondent[1024];
    int try=0;
    int success=0;


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



    printf("Enter Username: ");


    char login_buff[BUFFER_SIZE];
    char return_buff[BUFFER_SIZE];
    char login_in[BUFFER_SIZE];
    
    fgets(login_in, sizeof(login_in), stdin);
    login_in[strcspn(login_in, "\n")]='\0';
    snprintf(login_buff, sizeof(login_buff), "LOGIN:%s", login_in);


    send(sockfd, login_buff, strlen(login_buff), 0);
    recv(sockfd, return_buff, BUFFER_SIZE, 0);

    if(strncmp(return_buff, "LOGIN_OK", 9)==0)
    {
        strcpy(username, login_in);
    }
    else if(strncmp(return_buff, "LOGIN_SIGNUP", 13)==0)
    {
        strcpy(username, login_in);
    }

    printf("Select contacts:\n");
    get_friends_list();
    printf("-> ");

    char receipient_buff[BUFFER_SIZE];
    char receipient_input[BUFFER_SIZE];
    char return_buff2[BUFFER_SIZE];

    fgets(receipient_input, sizeof(receipient_input), stdin);
    receipient_input[strcspn(receipient_input, "\n")]='\0';

    snprintf(receipient_buff, sizeof(receipient_buff), "CONTACT:%s:%s", username, receipient_input);

    send(sockfd, receipient_buff, strlen(receipient_buff), 0);
    recv(sockfd, return_buff2, BUFFER_SIZE, 0);

    if(strncmp(return_buff2, "CONTACT_OK", 11)==0 || strncmp(return_buff2, "CONTACT_SIGNUP", 15)==0)
    {
        strcpy(respondent, receipient_input);
    }
    else
    {
        perror("USERNAME NOT IN DATABASE!");
        exit(EXIT_FAILURE);
    }



    // Start thread to receive messages
    pthread_create(&recv_thread, NULL, receive_messages, (void *)&sockfd);
    
    
    // Initialize ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    // Initialize windows
    init_windows();
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
        snprintf(message, sizeof(message), "MESSAGE:%s:%s:%s", username, respondent, input);

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