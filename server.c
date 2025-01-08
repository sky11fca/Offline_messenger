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
#include <sqlite3.h>
#include <openssl/sha.h>

#define PORT 8080
#define MAX_CLIENTS 1024
#define MAX_BUFFER 1024
#define HASH_LENGTH 64
#define SQLDATABASE "example_database.db"

int client_sockets[MAX_CLIENTS];
pthread_mutex_t client_sockets_mutex;
pthread_mutex_t file_mutex;

char* connected_users[MAX_CLIENTS];

int sha256_password(const char* input, char output[HASH_LENGTH+1])
{
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256_CTX sha256_ctx;
    SHA256_Init(&sha256_ctx);
    SHA256_Update(&sha256_ctx, input, strlen(input));
    SHA256_Final(hash, &sha256_ctx);

    for(int i=0; i<SHA256_DIGEST_LENGTH; i++)
    {
        sprintf(output+(i*2), "%02x", hash[i]);
    }

    output[HASH_LENGTH]='\0';
}

void save_to_log(const char* sender, const char* receiver, const char* message)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_open(SQLDATABASE, &db);
    if(rc != SQLITE_OK)
    {
        perror("OPEN");
        exit(EXIT_FAILURE);
    }

    const char* sql="INSERT INTO chatlogs (sender, receiver, message) VALUES (?, ?, ?)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if(rc != SQLITE_OK)
    {
        perror("OPEN");
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }

    sqlite3_bind_text(stmt, 1, sender, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, receiver, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, message, -1, SQLITE_STATIC);

    rc=sqlite3_step(stmt);

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void add_to_friendlist(int client_fd, const char* username, const char* receiver)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_open(SQLDATABASE, &db);

    if(rc != SQLITE_OK)
    {
        send(client_fd, "APPEND_FRIEND_ERR", 17, 0);
        perror("OPEN");
        exit(EXIT_FAILURE);
    }

    const char* sql="INSERT INTO friendlist (username, friend) VALUES (?, ?)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if(rc != SQLITE_OK)
    {
        send(client_fd, "APPEND_FRIEND_ERR", 17, 0);
        perror("OPEN");
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, receiver, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if(rc != SQLITE_DONE)
    {
        send(client_fd, "APPEND_FRIEND_ERR", 17, 0);
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db)); 
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }
    else
    {
        send(client_fd, "APPEND_FRIEND_OK", 16, 0);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

int in_friendlist(const char* username, const char* friend)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc, exist=0;

    rc = sqlite3_open(SQLDATABASE, &db);
    if(rc!=SQLITE_OK)
    {
        perror("OPEN");
        return 0;
    }

    const char *sql="SELECT 1 FROM friendlist WHERE username = ? AND friend = ?";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if(rc!=SQLITE_OK)
    {
        perror("OPEN");
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, friend, -1, SQLITE_STATIC);

    if(sqlite3_step(stmt)==SQLITE_ROW)
    {
        exist=1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return exist;
}

int in_database(const char* username)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc, exist=0;

    rc = sqlite3_open(SQLDATABASE, &db);
    if(rc!=SQLITE_OK)
    {
        perror("OPEN");
        return 0;
    }

    const char *sql="SELECT 1 FROM users WHERE username = ?";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if(rc!=SQLITE_OK)
    {
        perror("OPEN");
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    if(sqlite3_step(stmt)==SQLITE_ROW)
    {
        exist=1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return exist;
}

void add_user(int client_fd, const char* username, const char* password)
{
    sqlite3* db;
    sqlite3_stmt *stmt;
    int rc;

    rc=sqlite3_open(SQLDATABASE, &db);
    if(rc!=SQLITE_OK)
    {
        send(client_fd, "SIGNUP_ERR", 10, 0);
        perror("CANNOT OPEN DATABASE");
        exit(EXIT_FAILURE);
    }

    const char *insert_sql = "INSERT INTO users (username, password) VALUES (?, ?)";
    rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, 0);
    if(rc != SQLITE_OK)
    {
        send(client_fd, "SIGNUP_ERR", 10, 0);
        perror("FAIL TO PREPARE SQL STATEMENT");
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);

    if(sqlite3_step(stmt)==SQLITE_DONE)
    {
        send(client_fd, "SIGNUP_OK", 9, 0);
    }
    else
    {
        send(client_fd, "SIGNUP_ERR", 10, 0);
        perror("SIGNUP");
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void login(int client_fd, const char* username, const char* password)
{
    sqlite3* db;
    sqlite3_stmt *stmt;
    int rc;

    rc=sqlite3_open(SQLDATABASE, &db);
    if(rc!=SQLITE_OK)
    {
        send(client_fd, "LOGIN_FAIL", 10, 0);
        perror("CANNOT OPEN DATABASE");
        exit(EXIT_FAILURE);
    }

    const char* sql="SELECT username, password FROM users WHERE username = ? AND password = ?";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if(rc != SQLITE_OK)
    {
        send(client_fd, "LOGIN_ERR", 9, 0);
        perror("FAIL TO PREPARE SQL STATEMENT");
        sqlite3_close(db);
        exit(EXIT_FAILURE);
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);

    int match=0;
    if(sqlite3_step(stmt)==SQLITE_ROW)
    {
        match=1;
    }

    sqlite3_finalize(stmt);

    if(match)
    {
        send(client_fd, "LOGIN_OK", 9, 0);
    }
    else
    {
        const char* check_sql="SELECT * FROM users WHERE username = ?";
        rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

        int exists=0;
        
        if(sqlite3_step(stmt)==SQLITE_ROW)
        {
            exists=1;
        }

        sqlite3_finalize(stmt);

        if(exists)
        {
            send(client_fd, "LOGIN_FAIL", 10, 0);
        }

        else
        {
            send(client_fd, "LOGIN_SIGNUP", 13, 0);
        }
    }
    sqlite3_close(db);
}

void friendlist(int client_fd, const char* username, const char* receiver)
{

    if(in_friendlist(username, receiver))
    {
        send(client_fd, "CONTACT_OK", 11, 0);
    }
    else if(!in_friendlist(username, receiver) && in_database(receiver))
    {
        send(client_fd, "CONTACT_SIGNUP", 15, 0);
    }
    else
    {
        send(client_fd, "CONTACT_FAIL", 12, 0);
    }


}

void get_friendlist(int client_fd, const char* username)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_open(SQLDATABASE, &db);
    if(rc!=SQLITE_OK)
    {
        perror("OPEN");
        exit(EXIT_FAILURE);
    }

    const char *sql="SELECT friend FROM friendlist WHERE username = ?";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        perror("Failed to prepare SQL statement");
        sqlite3_close(db);
        exit(EXIT_FAILURE); 
    }
    
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    while((rc=sqlite3_step(stmt))==SQLITE_ROW)
    {
        const char *friend = (const char*)sqlite3_column_text(stmt, 0);
        send(client_fd, friend, MAX_BUFFER, 0);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    send(client_fd, "FRIENDLIST_DONE", 15, 0);
}

void get_chat_log(int client_fd, const char * username, const char* respondent)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_open(SQLDATABASE, &db);
    if(rc!=SQLITE_OK)
    {
        perror("OPEN");
        exit(EXIT_FAILURE);
    }

    const char *sql="SELECT sender, message FROM chatlogs WHERE (sender = ? AND receiver = ?) OR (sender = ? AND receiver = ?) ORDER BY timestamp ASC";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        perror("Failed to prepare SQL statement");
        sqlite3_close(db);
        exit(EXIT_FAILURE); 
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, respondent, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, respondent, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, username, -1, SQLITE_STATIC);

    char buff[MAX_BUFFER];

    while((rc=sqlite3_step(stmt))==SQLITE_ROW)
    {
        const char *sender = (const char*)sqlite3_column_text(stmt, 0);
        const char *message = (const char*)sqlite3_column_text(stmt, 1);

        snprintf(buff, MAX_BUFFER, "<%s> %s\n", sender, message);

        send(client_fd, buff, strlen(buff), 0);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void get_previous_message(const char* username, const char* respondent, char* previous_message)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_open(SQLDATABASE, &db);
    if(rc!=SQLITE_OK)
    {
        perror("OPEN");
        exit(EXIT_FAILURE);
    }

    const char *sql="SELECT sender, message FROM chatlogs WHERE (sender = ? AND receiver = ?) OR (sender = ? AND receiver = ?) ORDER BY timestamp ASC";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        perror("Failed to prepare SQL statement");
        sqlite3_close(db);
        exit(EXIT_FAILURE); 
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, respondent, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, respondent, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, username, -1, SQLITE_STATIC);

    while((rc=sqlite3_step(stmt))==SQLITE_ROW)
    {
        const char* message = (const char*)sqlite3_column_text(stmt, 1);
        
        if(strncmp(message, "REPLIED TO", 10)==0)
        {
            char real_message[MAX_BUFFER];
            sscanf(message, "REPLIED TO \"%*[^\"]\"\n -> %[^\n]", real_message);
            strcpy(previous_message, real_message);
        }
        else
        {
            strcpy(previous_message, message);
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void broadcast_message(const char *message, int sender_fd, const char* username, const char* respondent) 
{
    pthread_mutex_lock(&client_sockets_mutex);
    char finalbuff[1024];
    snprintf(finalbuff, sizeof(finalbuff), "<%s> %s", username, message);
    
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        if (client_sockets[i] != -1 && connected_users[i] != NULL && (strcmp(connected_users[i], username)==0 || strcmp(connected_users[i], respondent)==0)) 
        {
            send(client_sockets[i], finalbuff, strlen(finalbuff), 0);
        }
    }
    pthread_mutex_unlock(&client_sockets_mutex);
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char buffer[1024];
    char username[1024];
    char password[1024];
    char respondent[1024];
    char hash_password[HASH_LENGTH+1];
    char message[1024];
    char previous_message[1024];
    char reply_buff[MAX_BUFFER];

    int bytes_read;


    while ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) 
    {
        buffer[bytes_read] = '\0';

        if(strncmp(buffer, "LOGIN:", 6)==0)
        {
            sscanf(buffer, "LOGIN:%[^:]:%[^\n]", username, password);
            sha256_password(password, hash_password);
            printf("LOGIN CLIENT -> %s PASSWORD: %s\n", username, hash_password);
            login(client_fd, username, hash_password);

            pthread_mutex_lock(&client_sockets_mutex);
            for(int i=0; i<MAX_CLIENTS; i++)
            {
                if(client_sockets[i] == client_fd)
                {
                    connected_users[i]=strdup(username);
                    break;
                }
            }

            pthread_mutex_unlock(&client_sockets_mutex);

        }
        else if(strncmp(buffer, "CONTACT:", 8)==0)
        {
            sscanf(buffer, "CONTACT:%[^:]:%[^\n]", username, respondent);
            printf("CONTACT CLIENT -> %s, %s\n", username, respondent);
            friendlist(client_fd, username, respondent);

        }
        else if(strncmp(buffer, "MESSAGE:", 8)==0)
        {
            sscanf(buffer, "MESSAGE:%[^:]:%[^:]:%[^\n]", username, respondent, message);
            printf("MESSAGE CLIENT -> %s, %s : %s\n", username, respondent, message);
            if(strncmp(message, "/r ", 3)==0)
            {
                get_previous_message(username, respondent, previous_message);
                snprintf(reply_buff, MAX_BUFFER, "REPLIED TO \"%s\"\n -> %s", previous_message, message+3);
                broadcast_message(reply_buff, client_fd, username, respondent);
                save_to_log(username, respondent, reply_buff);
            }
            else
            {
                broadcast_message(message, client_fd, username, respondent);
                save_to_log(username, respondent, message);
            } 
        }
        else if(strncmp(buffer, "HISTORY:", 8)==0)
        {
            sscanf(buffer, "HISTORY:%[^:]:%[^\n]", username, respondent);
            get_chat_log(client_fd, username, respondent);
        }
        else if(strncmp(buffer, "SIGNUP:", 7)==0)
        {
            sscanf(buffer, "SIGNUP:%[^:]:%[^\n]", username, password);
            sha256_password(password, hash_password);
            add_user(client_fd, username, hash_password);
        }
        else if(strncmp(buffer, "APPEND_FRIEND:", 14)==0)
        {
            sscanf(buffer, "APPEND_FRIEND:%[^:]:%[^\n]", username, respondent);
            add_to_friendlist(client_fd, username, respondent);
        }
        else if(strncmp(buffer, "GET_FRIENDLIST:", 15)==0)
        {
            sscanf(buffer, "GET_FRIENDLIST:%[^\n]", username);
            get_friendlist(client_fd, username);
        }


    }

    if (bytes_read == 0) 
    {
        printf("Client %d disconnected.\n", client_fd);
    } 
    else 
    {
        perror("recv");
    }

    close(client_fd);

    pthread_mutex_lock(&client_sockets_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        if (client_sockets[i] == client_fd) 
        {
            client_sockets[i] = -1;
            if(connected_users[i])
            {
                free(connected_users[i]);
                connected_users[i]=NULL;
            }
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

    for (int i = 0; i < MAX_CLIENTS; i++) 
    {
        client_sockets[i] = -1;
        connected_users[i] = NULL;
    }

    pthread_mutex_init(&client_sockets_mutex, NULL);
    pthread_mutex_init(&file_mutex, NULL);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) 
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) 
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) 
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d\n", PORT);

    while (1) 
    {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        pthread_mutex_lock(&client_sockets_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) 
        {
            if (client_sockets[i] > 0) 
            {
                FD_SET(client_sockets[i], &readfds);
                if (client_sockets[i] > max_sd) 
                {
                    max_sd = client_sockets[i];
                }
            }
        }
        pthread_mutex_unlock(&client_sockets_mutex);

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) 
        {
            perror("select error");
        }

        if (FD_ISSET(server_fd, &readfds)) 
        {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) 
            {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            printf("New connection: socket fd %d, IP %s, port %d\n", new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            pthread_mutex_lock(&client_sockets_mutex);
            for (int i = 0; i < MAX_CLIENTS; i++) 
            {
                if (client_sockets[i] == -1) 
                {
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