#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include <string>
#include <unordered_map>
#include <queue>

#include "hermes_protocols.h"

typedef struct User
{
    std::string username;
    int sock;
    std::queue<std::string> offline_queue;

    pthread_mutex_t send_mutex;

    User() {
        pthread_mutex_init(&send_mutex, nullptr);
    }

    ~User() {
        pthread_mutex_destroy(&send_mutex);
    }
}User;

std::unordered_map<std::string, User*> Users;
pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;

void *connection_handler(void *socket_desc) {
    User *u = new User();

    int sock = *(int*)socket_desc;
    free(socket_desc);

    char buffer[USERNAME_SIZE] = {0};
    int read_size;
    
    uint32_t len;
    recv_all(sock, &len, 4);
    len = ntohl(len);

    read_size = recv_all(sock, buffer, len);
    buffer[read_size] = '\0';
    
    u->username = std::string(buffer);
    u->sock = sock;

    pthread_mutex_lock(&table_mutex);
    Users[u->username] = u;
    pthread_mutex_unlock(&table_mutex);

    printf("client:%s logged in\n", (u->username).c_str());

    while (1) {
        char buf[MESSAGE_MAX] = {0};
        if (recv_all(sock, buf, MESSAGE_MAX) <= 0) break;

        char recipient[USERNAME_SIZE];
        memcpy(recipient, buf+USERNAME_SIZE, USERNAME_SIZE);

        int temp_fd;

        pthread_mutex_lock(&table_mutex);
        if(Users.find(std::string(recipient)) == Users.end()) 
        {
            pthread_mutex_unlock(&table_mutex);
            printf("recipient offline\n");
            continue;
        }
        temp_fd = (Users[std::string(recipient)])->sock;
        pthread_mutex_unlock(&table_mutex);
        send_all(temp_fd, buf, MESSAGE_MAX);
    }

    printf("client disconnected: %s\n", (u->username).c_str());

    pthread_mutex_lock(&table_mutex);
    Users.erase(u->username);
    pthread_mutex_unlock(&table_mutex);

    close(sock);
    delete u;
    return NULL;
}

void cleanup(int sig) {
    pthread_mutex_lock(&table_mutex);
    for (auto &pair : Users) {
        close(pair.second->sock);
    }
    pthread_mutex_unlock(&table_mutex);
    exit(0);
}

int main() {

    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    int server_fd, client_fd, *new_fd;
    
    struct sockaddr_in addr;
    int opt = 1;
    socklen_t addrlen = sizeof(addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        { perror("setsockopt"); return 1; }

    int port = 8080;
    printf("enter server port (leave blank for 8080): ");
    char input_port[8] = {0};
    fgets(input_port, sizeof(input_port), stdin);
    if (input_port[0] != '\n')
    port = atoi(input_port);

    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        { perror("bind"); return 1; }

    if (listen(server_fd, 16) < 0)
        { perror("listen"); return 1; }

    printf("Listening on port %d...\n", port);

    while ((client_fd = accept(server_fd, (struct sockaddr *)&addr, &addrlen)) >= 0) {
        printf("Client connected\n");

        pthread_t sniffer_thread;
        new_fd = (int*)malloc(sizeof(int));
        *new_fd = client_fd;

        if (pthread_create(&sniffer_thread, NULL, connection_handler, (void*)new_fd) < 0) {
            perror("Could not create thread");
            return 1;
        }
        
        pthread_detach(sniffer_thread);
        printf("Handler assigned\n");
    }
    if (client_fd < 0) { perror("accept"); return 1; }

    return 0;
}