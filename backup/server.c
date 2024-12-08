#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <sqlite3.h>

#define IP_ADDR "127.0.0.1"

#define PORT 9090
#define MAX_CLIENTS 10
#define BUFF_SIZE 1024

typedef struct{
    int client_socket;
    char username[50];
} Client;

Client clients[MAX_CLIENTS]={0};
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
sqlite3 *db;


void broadcast_msg(int sender_sock, const char* msg, int msg_len, const char* recipient)
{
    pthread_mutex_lock(&client_mutex);
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        if(clients[i].client_socket !=0 && strcmp(clients[i].username, recipient)==0)
        {
            send(clients[i].client_socket, msg, msg_len, 0);
            pthread_mutex_unlock(&client_mutex);
            return;
        }
    }

    char *errmsg=0;
    char sql[BUFF_SIZE];
    snprintf(sql, sizeof(sql), "INSERT INTO messages(recipient, message) VALUES('%s', '%s');", recipient, msg);
    if(sqlite3_exec(db, sql, 0, 0, &errmsg)!=SQLITE_OK)
    {
        fprintf(stderr, "[SERVER] -> failed to insert message: %s\n", errmsg);
        sqlite3_free(errmsg);
    }
    pthread_mutex_unlock(&client_mutex);
}

void send_offline_messages(const char* username, int client_socket)
{
    char* errmsg=0;
    char sql[BUFF_SIZE];
    snprintf(sql, sizeof(sql), "SELECT message FROM messages WHERE recipient = '%s';", username);

    sqlite3_stmt *stmt;
    if(sqlite3_prepare_v2(db, sql, -1, &stmt, 0)==SQLITE_OK)
    {
        while(sqlite3_step(stmt)==SQLITE_ROW)
        {
            const char * message = (const char*)sqlite3_column_text(stmt, 0);
            send(client_socket, message, strlen(message), 0);
        }
        sqlite3_finalize(stmt);

        snprintf(sql, sizeof(sql), "DELETE FROM messages WHERE recipient = '%s';", username);
        if(sqlite3_exec(db, sql, 0, 0, &errmsg)!=SQLITE_OK)
        {
            fprintf(stderr, "[SERVER] -> Failed to Delete Message: %s\n", errmsg);
            sqlite3_free(errmsg);
        }
    }
    else
    {
        fprintf(stderr, "[SERVER] -> Failed to Fetch message: %s\n", sqlite3_errmsg(db));
    }
}

void* handle_client(void *arg)
{
    int client_sock = *(int*)arg;
    char buff[BUFF_SIZE];
    int valread;

    read(client_sock, buff, BUFF_SIZE);

    char username[50];
    sscanf(buff, "%s", username);
    printf("[SERVER] -> User %s, Authentificated\n", username);

    pthread_mutex_lock(&client_mutex);
    for(int i=0; i<MAX_CLIENTS; i++)
    {
        if(clients[i].client_socket==0)
        {
            clients[i].client_socket = client_sock;
            strncpy(clients[i].username, username, sizeof(clients[i].username)-1);
            send_offline_messages(username, client_sock);
            break;
        }
    }

    pthread_mutex_unlock(&client_mutex);

    while((valread=read(client_sock, buff, BUFF_SIZE))>0)
    {
        buff[valread]='\0';
        char recipient[50];
        sscanf(buff, "%s", recipient);
        broadcast_msg(client_sock, buff, valread, recipient);
    }

    pthread_mutex_lock(&client_mutex);

    for(int i=0; i<MAX_CLIENTS; i++)
    {
        if(clients[i].client_socket==client_sock)
        {
            clients[i].client_socket=0;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);

    close(client_sock);
    printf("[SERVER] -> Client disconnected\n");
    return NULL;
}

int main()
{
    int server_socket, new_socket;
    struct sockaddr_in address;
    pthread_t thread_id;
    int addrlen = sizeof(address);
    int op=1;

    if(sqlite3_open("messages.db", &db))
    {
        fprintf(stderr, "SQL -> Can't open database: %s\n", sqlite3_errmsg(db));
        return(1);
    }

    char* errmsg=0;
    const char* sql = "CREATE TABLE IF NOT EXISTS messages (recipient TEXT, message TEXT)";
    if(sqlite3_exec(db, sql, 0, 0, &errmsg)!=SQLITE_OK)
    {
        fprintf(stderr, "SQL ERROR: %s\n", errmsg);
        sqlite3_free(errmsg);
        return(1);
    }

    server_socket=socket(AF_INET, SOCK_STREAM, 0);
    
    if(server_socket == 0)
    {
        perror("Socket");
        exit(EXIT_FAILURE);
    }

    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op));
    setsockopt(new_socket, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op));

    bzero(&address, sizeof(address));
    address.sin_family=AF_INET;
    address.sin_addr.s_addr=INADDR_ANY;
    address.sin_port=htons(PORT);

    if(bind(server_socket, (struct sockaddr*)&address, sizeof(address))<0)
    {
        perror("Bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if(listen(server_socket, 5)<0)
    {
        perror("Listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server up and running: %i\n", ntohs(address.sin_port));

    while(1)
    {
        if((new_socket=accept(server_socket, (struct sockaddr*)&address, (socklen_t*)&addrlen))<0)
        {
            perror("Accept");
            EXIT_FAILURE;
        }

        printf("[SERVER] -> New connection, sockfd: %d, ip: %s, port: %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        pthread_mutex_lock(&client_mutex);

        for(int i=0; i<MAX_CLIENTS; i++)
        {
            if(clients[i].client_socket==0)
            {
                clients[i].client_socket=new_socket;
                pthread_create(&thread_id, NULL, handle_client, (void* )&new_socket);
                pthread_detach(thread_id);
                break;
            }
        }
        pthread_mutex_unlock(&client_mutex);
    }
    sqlite3_close(db);
    return 0;

}