#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <readline/readline.h>
#include <pthread.h>

#include "hermes_protocols.h"

void print_message(const char *msg) {
    // save cursor position, clear line
    printf("\r\033[K"); // clear current line
    printf("Message from [%.24s]: %s\n", msg, msg + HEADER);
    rl_on_new_line();       // tell readline cursor is on new line
    rl_redisplay();         // redraw the prompt + current input
}

void *recv_handler(void *sock_desc) {
    int sock = *(int*)sock_desc;
    free(sock_desc);
    char buf[MESSAGE_MAX] = {0};
    while (recv_all(sock, buf, MESSAGE_MAX) > 0) {
        print_message(buf);
        memset(buf, 0, MESSAGE_MAX);
    }
    printf("server disconnected\n");
    exit(0);
}

int main() {
    int sock;
    struct sockaddr_in addr;

    char self[USERNAME_SIZE] = {0};
    char recipient[USERNAME_SIZE] = {0};

    char server_ip[16] = "127.0.0.1";
    int port = 8080;

    printf("enter server ip (leave blank for localhost): ");
    char input_ip[16] = {0};
    fgets(input_ip, sizeof(input_ip), stdin);
    if (input_ip[0] != '\n') {
        input_ip[strcspn(input_ip, "\n")] = '\0';
        strncpy(server_ip, input_ip, sizeof(server_ip) - 1);
    }

    printf("enter server port (leave blank for 8080): ");
    char input_port[8] = {0};
    fgets(input_port, sizeof(input_port), stdin);
    if (input_port[0] != '\n')
    port = atoi(input_port);

    printf("\nHermes messenger initial version. \nUsername max length is 24 characters. \nMessage max length is 2048 characters.\n\n");

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, server_ip, &addr.sin_addr);

    printf("connecting to %s:%d\n", server_ip, ntohs(addr.sin_port));

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
    {
        perror("Error");
        return 1;
    }

    printf("Connected to server.\n");

    printf("enter your username: ");
    scanf("%24s", self);

    printf("enter recipient username: ");
    scanf("%24s", recipient);

    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));

    uint32_t len = htonl(strlen(self));
    send(sock, &len, sizeof(uint32_t), 0);
    send(sock, self, strlen(self), 0);

    pthread_t recv_thread;
    int *sock_ptr = (int*)malloc(sizeof(int));
    *sock_ptr = sock;
    pthread_create(&recv_thread, NULL, recv_handler, sock_ptr);

    printf("You may send messages now! (press enter to send)\n");

    while(true)
    {
        char *msg = readline("Enter message: ");
        if (!msg || strcmp(msg, "quit") == 0) { free(msg); break; }
        if (strlen(msg) == 0) { free(msg); continue; }
        if(strlen(msg) > MSG_BODY_SIZE) 
        {
            printf("Too long! Message must be 2048 characters or less\n");
            continue;
        }

        char msg_payload[MESSAGE_MAX] = {0};
        strncpy(msg_payload, self, USERNAME_SIZE);
        strncpy(msg_payload+USERNAME_SIZE, recipient, USERNAME_SIZE);
        strncpy(msg_payload + HEADER, msg, MSG_BODY_SIZE);
        send_all(sock, msg_payload, MESSAGE_MAX);
        free(msg);
    }

    close(sock);
    return 0;
}