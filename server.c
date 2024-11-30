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

#define IP_ADDR "127.0.0.1"

#define PORT 9090

int main()
{
    int sockfd;

    sockfd=socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server, from;
    
    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(IP_ADDR);
    server.sin_port = htons(PORT);

    bind(sockfd, (struct sockaddr *) &server, sizeof(server));

    listen(sockfd, 10);

    while(1)
    {
        char msg[256];
        int client;
        int lenght = sizeof(from);

        client = accept(sockfd, (struct sockaddr*) &from, &lenght);

        if(fork()==0)
        {
            bzero(msg, 256);

            read(client, msg, 256);

            printf("Successfully received the message: %s", msg);

            write(client, msg, 256);

            close(client);

        }
        else
        {
            close(client);
        }


    }
}

    //socket
    //bind
    //listen
    ///////////////while//////////////////
    //fork
        //accept
        //read
        //write
        //exit
    ///////////////
        //close
    ///////////////end while//////////////

    //close