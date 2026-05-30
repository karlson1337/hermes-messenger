#include "../hermes_protocols.h"                   
#include "function_defs.h"

#include <readline/readline.h>
#include <netinet/tcp.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <termios.h>

#define COMMAND_SIZE 16

sqlite3 *friend_list = NULL;
sqlite3 *chat_db = NULL;

char self[USERNAME_SIZE] = {0};
char password[PASSWORD_SIZE];

unsigned char self_pk[crypto_sign_PUBLICKEYBYTES];
unsigned char self_sk[crypto_sign_SECRETKEYBYTES];

unsigned char server_pk[crypto_box_PUBLICKEYBYTES];

char recipient[USERNAME_SIZE] = {0};

int sock;

static const char *commands[] = 
{
    "/help   - show available commands",
    "/add    - add a friend: /add <username>",
    "/list   - list friends",
    "/open   - open a chat: /open <username>  [must be added to friends list beforehand]",
    "/quit   - disconnect and exit",
};
static const int NUM_COMMANDS = sizeof(commands) / sizeof(commands[0]);

//========MAIN FUNCTIONALITY CODE========

void print_message(const char *sender, int64_t timestamp, const unsigned char *msg) 
{
    char buf[32];
    struct tm *tm = localtime(&timestamp);
    strftime(buf, sizeof(buf), "%b %d %H:%M", tm);
    
    printf("\r\033[K");
    printf("\n[%s] (%.24s): %s\n", buf, sender, msg);
    rl_on_new_line();
    rl_redisplay();
}

static void print_history(int64_t ts, const char *sender, const char *msg, void *ud) {
    char timebuf[32];
    struct tm *tm = localtime(&ts);
    strftime(timebuf, sizeof(timebuf), "%H:%M %b %d", tm);
    printf("[%s] %s: %s\n", timebuf, sender, msg);
}

void open_chat(const char *new_recipient) {
    strncpy(recipient, new_recipient, USERNAME_SIZE-1);

    chatdb_history(self, recipient, 50, print_history, NULL);
}

bool command(const char *msg)
{
    char sub[16] = {0};
    char arg[USERNAME_SIZE] = {0};
    sscanf(msg + 1, "%15s %24s", sub, arg);

    if(strcmp(sub, "help") == 0)
    {
        for(int i = 0; i < NUM_COMMANDS; i++) printf("%s\n", commands[i]);
        return true;
    }
    if(strcmp(sub, "add") == 0)
    {
        if(strcmp(arg, self) == 0) 
        {
            printf("you can't add yourself!\n");
            return true;
        }
        if (friend_exists(arg)) 
        {
            printf("[%s] is already in your friends list\n", arg);
            return true;
        }
        char temp[MESSAGE_MAX] = {0};
        temp[0] = 1;
        strncpy(temp+1, self, USERNAME_SIZE);
        strncpy(temp+1+USERNAME_SIZE, arg, USERNAME_SIZE);
        send_all(sock, temp, MESSAGE_MAX+crypto_box_SEALBYTES);
        printf("added user\n");
        return true;
    }
    if(strcmp(sub, "list") == 0)
    {
        list_friends();
        return true;
    }
    if(strcmp(sub, "open") == 0)
    {
        unsigned char pubkey[crypto_sign_PUBLICKEYBYTES];

        if(strcmp(arg, self) == 0) 
        {
            printf("you can't chat with yourself.");
            return true;
        }

        if (!friend_get_pubkey(arg, pubkey))
        {
            printf("user not in friends list, use /add <username> to add them.\n"); 
            return true;
        }

        open_chat(arg);
        printf("now chatting with [%s]\n", recipient);

        return true;
    }
    if(strcmp(sub, "quit") == 0)
    {
        close(sock);
        chatdb_close();
        exit(0);
    }
    
    return false;
}

void send_message(const char *msg)
{
    if (!msg) { exit(0); }
    if (strlen(msg) == 0) { return; }

    if(*msg == '/') { command(msg);  return; }

    if (recipient[0] == '\0') 
    {
        printf("no recipient set. use /open <username> to start a chat\n");
        return;
    }

    if(strlen(msg) > MSG_BODY_SIZE) {
        printf("Too long! Message must be 2048 characters or less\n");
        return;
    }

    unsigned char recipient_pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char recipient_x25519[crypto_box_PUBLICKEYBYTES];

    if (!friend_get_pubkey(recipient, recipient_pk)) 
    {
        printf("recipient not in friends list\n");
        return;
    }

    if(crypto_sign_ed25519_pk_to_curve25519(recipient_x25519, recipient_pk) != 0)
    { 
        fprintf(stderr, "invalid public key for %s\n", recipient);
        return;
    }
    
    unsigned char packet[MESSAGE_MAX + crypto_box_SEALBYTES] = {0};
    packet[0] = TYPE_MESSAGE;
    memcpy(packet + TYPE_BYTE, self, USERNAME_SIZE);
    memcpy(packet + TYPE_BYTE + USERNAME_SIZE, recipient, USERNAME_SIZE);
    int64_t timestamp = (int64_t)time(NULL);
    memcpy(packet + TYPE_BYTE + USERNAME_SIZE*2, &timestamp, sizeof(int64_t));

    unsigned char msgbuf[MSG_BODY_SIZE] = {0};
    memcpy(msgbuf, msg, strlen(msg));
    crypto_box_seal(packet + HEADER, msgbuf, MSG_BODY_SIZE, recipient_x25519);

    send_all(sock, packet, MESSAGE_MAX + crypto_box_SEALBYTES);
    chatdb_insert(self, recipient, timestamp, (const char *)msg);
}

void *recv_handler(void *sock_desc) 
{
    int sock = *(int*)sock_desc;
    free(sock_desc);

    char buf[MESSAGE_MAX+crypto_box_SEALBYTES] = {0};

    unsigned char self_x25519_sk[crypto_box_SECRETKEYBYTES];
    unsigned char self_x25519_pk[crypto_box_PUBLICKEYBYTES];

    if (crypto_sign_ed25519_pk_to_curve25519(self_x25519_pk, self_pk) != 0)
    {
        fprintf(stderr, "invalid secret key for %s.\n", self);
        return NULL;
    }
    
    if(crypto_sign_ed25519_pk_to_curve25519(self_x25519_pk, self_pk) != 0)
    {
        fprintf(stderr, "invalid public key for %s.\n", self);
        return NULL;
    }

    unsigned char plaintext[MSG_BODY_SIZE] = {0};

    while (recv_all(sock, buf, MESSAGE_MAX+crypto_box_SEALBYTES) > 0) {

        int64_t timestamp;
        memcpy(&timestamp, (int64_t*)buf + TYPE_BYTE + (USERNAME_SIZE * 2), TIMESTAMP_HEADER);

        memset(plaintext, 0, MSG_BODY_SIZE);
        
        char sender[USERNAME_SIZE];
        memcpy(sender, buf+TYPE_BYTE, USERNAME_SIZE);

        if(strcmp(sender, "server") != 0)
        {
            if (crypto_box_seal_open(plaintext,
            (unsigned char*)buf + HEADER,
            MSG_BODY_SIZE + crypto_box_SEALBYTES,
            self_x25519_pk, self_x25519_sk) != 0) 
            {
                printf("decryption failed");
                continue;
            }
        }
        else
        {
            memcpy(plaintext, buf+HEADER, MSG_BODY_SIZE);
        }

        if(buf[0] == TYPE_ERROR || buf[0] == TYPE_404) 
        {
            print_message(sender, timestamp, plaintext);
        }
        else if(buf[0] == TYPE_FRIENDADD)
        {
            char new_friend[USERNAME_SIZE];
            memcpy(new_friend, buf+TYPE_BYTE+USERNAME_SIZE, USERNAME_SIZE);
            friends_add(new_friend, (unsigned char*)(buf + HEADER));
        }
        else print_message(sender, timestamp, plaintext);
        if(buf[0] == TYPE_MESSAGE) { chatdb_insert(sender, self, timestamp, (const char *)plaintext); }

        memset(buf, 0, MESSAGE_MAX+crypto_box_SEALBYTES);
    }
    printf("server disconnected\n");
    chatdb_close();
    exit(0);
}

int main() 
{
    if (sodium_init() < 0) { fprintf(stderr, "libsodium init failed\n"); return 1; }

    struct sockaddr_in addr;

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

    printf("\nHermes messenger early version. \nMessage max length is 2048 characters.\n\n");

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
        perror("error");
        return 1;
    }

    printf("connected to server.\n\n");

    int idle = 60;
    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
    #ifdef __linux__
        int interval = 10;
        int count = 3;
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,     sizeof(idle));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT,   &count,    sizeof(count));
    #elif defined(__APPLE__) || defined(__unix__)
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle));
    #endif

    if (recv_all(sock, server_pk, crypto_box_PUBLICKEYBYTES) <= 0) 
    {
        fprintf(stderr, "Failed to receive server public key\n");
        close(sock);
        return 1;
    }

    get_credentials();

    if (!authenticate()) { return 1; }

    friend_db_init();

    pthread_t recv_thread;
    int *sock_ptr = (int*)malloc(sizeof(int));
    *sock_ptr = sock;
    pthread_create(&recv_thread, NULL, recv_handler, sock_ptr);

    printf("\n===============================================\n\n");
    printf("You may send messages now! (press enter to send).\nFor a list of available commands, type /help\n");

    while(true) {
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "(to %s): ", recipient);
        char *msg = readline(prompt);
        send_message(msg);
        free(msg);
    }
    chatdb_close();
    close(sock);
    return 0;
}