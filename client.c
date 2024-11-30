#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

#define PORT 9090
#define IP_ADDR "127.0.0.1"



int main()
{
    int sockfd;
    char msg[256];
    struct sockaddr_in server;

    sockfd=socket(AF_INET, SOCK_STREAM, 0);

    server.sin_family=AF_INET;
    server.sin_addr.s_addr = inet_addr(IP_ADDR);
    server.sin_port = htons(PORT);

    connect(sockfd, (struct sockaddr*) &server, sizeof(server));

    bzero(msg, 256);

    read(0, msg, 256);

    write(sockfd, msg, 256);

    read(sockfd, msg, 256);

    printf("Returned Message: %s", msg);

    close(sockfd);

}