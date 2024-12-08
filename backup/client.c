#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

#define PORT 9090
#define BUFF_SIZE 1024

void* received_msg(void *client_socket)
{
    int sock=*(int*)client_socket;
    char buff[BUFF_SIZE];
    int valread;

    while((valread=read(sock, buff, BUFF_SIZE))>0)
    {
        buff[valread]='\0';
        printf("%s\n", buff);
    }

    return NULL;
}

int main(int argc, char* argv[])
{
    // if(argc>2)
    // {
    //     fprintf(stderr, "USAGE: %s <port>");
    //     exit(EXIT_FAILURE);
    // }

    // int port = atoi(argv[0]);

    int client_sock;
    struct sockaddr_in server_addr;
    char msg[BUFF_SIZE];
    pthread_t thread_id;

    client_sock=socket(AF_INET, SOCK_STREAM, 0);
    if(client_sock<0)
    {
        perror("Socket");
        exit(EXIT_FAILURE);
    }

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(PORT);
    if(inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr)<=0)
    {
        perror("[CLIENT] -> inet_pron");
        exit(EXIT_FAILURE);
    }

    if(connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr))<0)
    {
        perror("Connect");
        exit(EXIT_FAILURE);
    }

    printf("Enter Username: ");
    fgets(msg, BUFF_SIZE, stdin);
    send(client_sock, msg, strlen(msg), 0);

    pthread_create(&thread_id, NULL, received_msg, (void *)&client_sock);
    pthread_detach(thread_id);

    while(1)
    {
        fgets(msg, BUFF_SIZE, stdin);
        send(client_sock, msg, strlen(msg), 0);
    }
    close(client_sock);
    return 0;
}