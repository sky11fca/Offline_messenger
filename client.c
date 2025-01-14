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


void *receive_messages(void *socket_fd) 
{
    int sockfd = *(int *)socket_fd;
    char buffer[BUFFER_SIZE];

    while (1) 
    {
        int bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) 
        {
            buffer[bytes_received] = '\0';
            wprintw(message_win, "%s\n", buffer);
            wrefresh(message_win);
        } 
        else if (bytes_received == 0) 
        {
            break;
        } 
        else 
        {
            perror("recv");
            break;
        }
    }
    return NULL;
}

void init_windows() 
{
    int row, col;
    getmaxyx(stdscr, row, col);

    message_win = newwin(row - 3, col, 0, 0);
    input_win = newwin(3, col, row - 3, 0);

    scrollok(message_win, TRUE);
    box(input_win, 0, 0);

    wrefresh(message_win);
    wrefresh(input_win);
}

int main() 
{
    int sockfd;
    struct sockaddr_in server_addr;
    pthread_t recv_thread;
    char username[1024];
    char respondent[1024];


    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) 
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf("\033[2J\033[H");
    printf("WELLCOME TO THUNDERSTRUCK!!!\n");
    printf("The Open Source Messaging client\n");

    printf("------------------------------------------\n");
    printf("INSTRUCTIONS:\n");

    printf("/r -> reply\n");
    printf("/exit -> exit client\n");



    char login_buff[BUFFER_SIZE];
    char return_buff[BUFFER_SIZE];
    char login_username[BUFFER_SIZE];
    char login_password[BUFFER_SIZE];
    while(1)
    {
        printf("Enter Username: ");
        fgets(login_username, sizeof(login_username), stdin);
        login_username[strcspn(login_username, "\n")]='\0';

        printf("Enter a password (WILL ECHO): ");

        fgets(login_password, sizeof(login_password), stdin);
        login_password[strcspn(login_password, "\n")]='\0';
    
        snprintf(login_buff, sizeof(login_buff), "LOGIN:%s:%s", login_username, login_password);


        send(sockfd, login_buff, strlen(login_buff), 0);
        recv(sockfd, return_buff, BUFFER_SIZE, 0);

        if(strncmp(return_buff, "LOGIN_OK", 9)==0)
        {
            strcpy(username, login_username);
            break;
        }
        else if(strncmp(return_buff, "LOGIN_SIGNUP", 13)==0)
        {
            char opt[BUFFER_SIZE];
            char signup_buffer[BUFFER_SIZE];
            char ret_buff3[BUFFER_SIZE];
            printf("Warning! Username, not in the database! Would you like to add the username? (y/n) ");
            
            fgets(opt, sizeof(opt), stdin);
            opt[strcspn(opt, "\n")]='\0';
            
            if(strcmp(opt, "y")==0)
            {
                
                snprintf(signup_buffer, BUFFER_SIZE, "SIGNUP:%s:%s", login_username, login_password);
                send(sockfd, signup_buffer, BUFFER_SIZE, 0);
                recv(sockfd, ret_buff3, BUFFER_SIZE, 0);

                if(strncmp(ret_buff3, "SIGNUP_OK", 9)==0)
                {
                    strcpy(username, login_username);
                    break;
                }
                else if(strncmp(ret_buff3, "SIGNUP_ERR", 10)==0)
                {
                    perror("SIGNUP");
                    exit(EXIT_FAILURE);
                }
                
            }
            else if(strcmp(opt, "n")==0)
            {
                printf("Sorry! Try Again\n");
            }
        }
        else if(strncmp(return_buff, "LOGIN_FAIL", 10)==0)
        {
            sleep(3);
            printf("Sorry! Try Again: ");
        }
        else if(strncmp(return_buff, "LOGIN_ERR", 9)==0)
        {
            perror("LOGIN");
            exit(EXIT_FAILURE);
        }
    }

    printf("\033[2J\033[H");
    
    char getfriend_buff[BUFFER_SIZE];
    printf("Select contacts:\n");
    snprintf(getfriend_buff, BUFFER_SIZE, "GET_FRIENDLIST:%s", username);
    send(sockfd, getfriend_buff, BUFFER_SIZE, 0);
    
    int buff_size;
    char names[BUFFER_SIZE];

    while(1)
    {
        recv(sockfd, names, BUFFER_SIZE, 0);
        if(strncmp(names, "FRIENDLIST_DONE", 15)==0)
        {
            break;
        }
        else
        {
            printf("* %s\n", names);
        }
    }

    printf("-> ");

    char receipient_buff[BUFFER_SIZE];
    char receipient_input[BUFFER_SIZE];
    char return_buff2[BUFFER_SIZE];
    
    while(1)
    {
        fgets(receipient_input, sizeof(receipient_input), stdin);
        receipient_input[strcspn(receipient_input, "\n")]='\0';
        if(strcmp(receipient_input, username))
        {
            snprintf(receipient_buff, sizeof(receipient_buff), "CONTACT:%s:%s", username, receipient_input);

            send(sockfd, receipient_buff, strlen(receipient_buff), 0);
            recv(sockfd, return_buff2, BUFFER_SIZE, 0);

            if(strncmp(return_buff2, "CONTACT_OK", 11)==0)
            {
                strcpy(respondent, receipient_input);
                break;
            }
            else if(strncmp(return_buff2, "CONTACT_SIGNUP", 14)==0)
            {
                char opt[BUFFER_SIZE];
                char append_buffer[BUFFER_SIZE];
                char ret_buff4[BUFFER_SIZE];
                printf("WARNING, Username not in friendlist, but the same user with same name exists in ot database \nDo you want to add him? (y/n) ");

                fgets(opt, sizeof(opt), stdin);
                opt[strcspn(opt, "\n")]='\0';

                if(strcmp(opt, "y")==0)
                {
                    snprintf(append_buffer, BUFFER_SIZE, "APPEND_FRIEND:%s:%s", username, receipient_input);
                    send(sockfd, append_buffer, BUFFER_SIZE, 0);
                    recv(sockfd, ret_buff4, BUFFER_SIZE, 0);

                    if(strncmp(ret_buff4, "APPEND_FRIEND_OK", 17)==0)
                    {
                        strcpy(respondent, receipient_input);
                        break;
                    }
                    else if(strncmp(ret_buff4, "APPEND_FRIEND_ERR", 18)==0)
                    {
                        perror("ADD TO FRIENDLIST");
                        exit(EXIT_FAILURE);
                    }
                
                }
                else if(strcmp(opt, "n")==0)
                {
                    printf("Sorry! Try Again\n-> ");
                }
            }
            else if(strncmp(return_buff2, "CONTACT_FAIL", 12)==0)
            {
                perror("CONTACT");
            }
        }
        else
        {
            printf("SORRY! Your name cannot be your friend!: ");
        }
        
    }
    
    printf("\033[2J\033[H");
    char gethist_buffer[BUFFER_SIZE];

    pthread_create(&recv_thread, NULL, receive_messages, (void *)&sockfd);
    
    
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    init_windows();


    snprintf(gethist_buffer, sizeof(gethist_buffer), "HISTORY:%s:%s", username, respondent);
    send(sockfd, gethist_buffer, strlen(gethist_buffer), 0);


    char input[BUFFER_SIZE];
    int ch, pos = 0;

    while (1) 
    {
        memset(input, 0, BUFFER_SIZE);
        pos = 0;

        while (1) 
        {
            werase(input_win);
            box(input_win, 0, 0);
            mvwprintw(input_win, 1, 1, "%s: %s", username, input);
            wrefresh(input_win);

            ch = wgetch(input_win);

            if (ch == '\n') 
            {
                break;
            } 
            else if (ch == 127 || ch == KEY_BACKSPACE) 
            { 
                if (pos > 0) 
                {
                    input[--pos] = '\0';
                }
            } 
            else if (pos < BUFFER_SIZE - 1) 
            {
                input[pos++] = ch;
                input[pos] = '\0';
            }
        }

        if (strcmp(input, "/exit") == 0) 
        {
            break;
        }

        char message[1024];
        snprintf(message, sizeof(message), "MESSAGE:%s:%s:%s", username, respondent, input);

        send(sockfd, message, strlen(message), 0);

        
    }

    pthread_cancel(recv_thread);
    pthread_join(recv_thread, NULL);
    close(sockfd);
    endwin();

    return 0;
}