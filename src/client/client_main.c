//Hermes messenger main client code.

#include "../hermes_protocols.h"                   
#include "function_defs.h"

#include <netinet/tcp.h>
#include <stdbool.h>
#include <termios.h>

#define COLOR_RECEIVED "\033[32m"
#define COLOR_NOTIFY "\033[33m"
#define COLOR_RESET    "\033[0m"

#define COMMAND_SIZE 16

#define MAX_HISTORY 200

sqlite3 *friend_list = NULL;
sqlite3 *chat_db = NULL;

char self[USERNAME_SIZE] = {0};
char password[PASSWORD_SIZE];

unsigned char self_pk[crypto_box_PUBLICKEYBYTES];
unsigned char self_sk[crypto_box_SECRETKEYBYTES];

unsigned char server_pk[crypto_box_PUBLICKEYBYTES];

char recipient[USERNAME_SIZE] = {0};
char prompt[64];

int sock;

int chat_scroll_offset = 0;

const char *commands[] = 
{
    "/help   - show available commands",
    "/add    - add a friend: /add <username>",
    "/open   - open a chat with a friend: /open <username>",
    "/verify - verify if encryption is working: /verify <username>",
    "/quit   - disconnect and exit",
};
const int NUM_COMMANDS = sizeof(commands) / sizeof(commands[0]);

//========MAIN FUNCTIONALITY CODE========

static void print_history(int64_t timestamp, const char *sender, const unsigned char *msg, void *ud) {
    ui_print_message(timestamp, sender, msg); }

void redraw_chat() {
    if (recipient[0] == '\0') return;
    ui_clear_chat();
    chatdb_history(self, recipient, MAX_HISTORY, chat_scroll_offset, print_history, NULL);
}

void open_chat(const char *new_recipient) {
    memcpy(recipient, new_recipient, USERNAME_SIZE);
    friends_clear_unread(new_recipient);
    ui_set_header(recipient);
    ui_draw_friends();
    chat_scroll_offset = 0;
    redraw_chat(); // clears and renders with chatdb_history
}

void security_number(const unsigned char *pk_a, const unsigned char *pk_b) 
{
    const unsigned char *lo = pk_a, *hi = pk_b;
    if (memcmp(pk_a, pk_b, crypto_box_PUBLICKEYBYTES) > 0) { lo = pk_b; hi = pk_a; }
 
    unsigned char combined[crypto_box_PUBLICKEYBYTES * 2];
    memcpy(combined, lo, crypto_box_PUBLICKEYBYTES);
    memcpy(combined + crypto_box_PUBLICKEYBYTES, hi, crypto_box_PUBLICKEYBYTES);

    unsigned char hash[32];
    char out[32 * 2 + 1];
    crypto_generichash(hash, sizeof(hash), combined, sizeof(combined), NULL, 0);

    // format as hex
    for (int i = 0; i < 32; i++)
        sprintf(out + i * 2, "%02x", hash[i]);
    out[64] = '\0';

    char grouped[64 + 64/4 + 1]; //print in groups of 4
    int pos = 0;
    for (int i = 0; i < 64; i++) {
        if (i > 0 && i % 4 == 0) grouped[pos++] = ' ';
        grouped[pos++] = out[i];
    }
    grouped[pos] = '\0';

    ui_print_system_message(grouped);
    ui_print_system_message("Compare this string with the one your friend gets. It should be same. If not, then encryption has been compromised!");
}

bool command(const char *msg)
{
    char sub[16] = {0};
    char arg[USERNAME_SIZE] = {0};
    sscanf(msg + 1, "%15s %24s", sub, arg);

    if(strcmp(sub, "help") == 0)
    {
        ui_show_help();
        return true;
    }
    if(strcmp(sub, "add") == 0)
    {
        if (arg[0] == '\0') {return true;}
        if(strcmp(arg, self) == 0) { return true; }
        if (friend_exists(arg)) { return true; }

        char temp[MESSAGE_MAX] = {0};
        temp[0] = 1;
        memcpy(temp+TYPE_BYTE, self, USERNAME_SIZE);
        memcpy(temp+TYPE_BYTE+USERNAME_SIZE, arg, USERNAME_SIZE);
        send_all(sock, temp, MESSAGE_MAX+crypto_box_SEALBYTES);
        return true;
    }
    if(strcmp(sub, "open") == 0)
    {
        if (arg[0] == '\0') { return true; }
        unsigned char pubkey[crypto_box_PUBLICKEYBYTES];

        if(strcmp(arg, self) == 0) { return true; }

        if (!friend_get_pubkey(arg, pubkey)){ return true; }
        open_chat(arg);
        return true;
    }
    if(strcmp(sub, "verify") == 0)
    {
        if (arg[0] == '\0') { return true; }
        unsigned char pubkey[crypto_box_PUBLICKEYBYTES];

        if(strcmp(arg, self) == 0) { return true; }

        if (!friend_get_pubkey(arg, pubkey)){ return true; }

        security_number(self_pk, pubkey);
    }
    if(strcmp(sub, "quit") == 0)
    {
        ui_cleanup();
        close(sock);
        sqlite3_close(chat_db);
        sqlite3_close(friend_list);
        exit(0);
    }
    
    return false;
}

void send_message(const char *msg)
{
    if (!msg) { exit(0); }
    if (strlen(msg) == 0) { return; }

    if(*msg == '/') 
    {
        command(msg); 
        return; 
    }

    if (recipient[0] == '\0') 
    {
        ui_print_system_message("No recipient set. Use /open <username> to start a chat");
        return;
    }

    if(strlen(msg) > MSG_BODY_SIZE) {
        ui_print_system_message("Too long! Message must be 2048 characters or less");
        return;
    }

    unsigned char recipient_pk[crypto_box_PUBLICKEYBYTES];

    if (!friend_get_pubkey(recipient, recipient_pk)) 
    {
        ui_print_system_message("Recipient not in friends list.");
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
    crypto_box_seal(packet + HEADER, msgbuf, MSG_BODY_SIZE, recipient_pk);

    send_all(sock, packet, MESSAGE_MAX + crypto_box_SEALBYTES);
    chatdb_insert(self, recipient, timestamp, (const unsigned char *)msg);

    chat_scroll_offset = 0;
    redraw_chat(); // re renders entire chat
}

void *recv_handler(void *sock_desc) 
{
    int sock = *(int*)sock_desc;
    free(sock_desc);

    unsigned char buf[MESSAGE_MAX+crypto_box_SEALBYTES] = {0};

    unsigned char plaintext[MSG_BODY_SIZE] = {0};

    while (recv_all(sock, buf, MESSAGE_MAX+crypto_box_SEALBYTES) > 0) {

        int64_t timestamp;
        memcpy(&timestamp, buf + TYPE_BYTE + (USERNAME_SIZE * 2), TIMESTAMP_HEADER);

        memset(plaintext, 0, MSG_BODY_SIZE);
        
        char sender[USERNAME_SIZE];
        memcpy(sender, buf+TYPE_BYTE, USERNAME_SIZE);

        if(strcmp(sender, "server") != 0)
        {
            if (crypto_box_seal_open(plaintext,
            (unsigned char*)buf + HEADER,
            MSG_BODY_SIZE + crypto_box_SEALBYTES,
            self_pk, self_sk) != 0) 
            {
                ui_print_system_message("Decryption failed");
                continue;
            }
        }
        else
        {
            memcpy(plaintext, buf+HEADER, MSG_BODY_SIZE);
        }

        if(buf[0] == TYPE_ERROR || buf[0] == TYPE_404) 
        {
            ui_print_system_message((const char*)plaintext);
        }
        else if(buf[0] == TYPE_FRIENDADD)
        {
            char new_friend[USERNAME_SIZE];
            memcpy(new_friend, buf+TYPE_BYTE+USERNAME_SIZE, USERNAME_SIZE);
            unsigned char decrypted_pk[crypto_box_PUBLICKEYBYTES];
            if (crypto_box_seal_open(decrypted_pk, buf + HEADER,
                crypto_box_PUBLICKEYBYTES + crypto_box_SEALBYTES,
                self_pk, self_sk) != 0) {
                ui_print_system_message("Failed to decrypt friend's public key.");
                continue;
            }
            friends_add(new_friend, decrypted_pk);
            ui_print_system_message("Added friend successfully.");
        }
        else if(buf[0] == TYPE_MESSAGE) 
        {
            chatdb_insert(sender, self, timestamp, (const unsigned char *)plaintext);
            if(strcmp(sender, recipient) == 0)
            {
                if (chat_scroll_offset == 0) { ui_print_message(timestamp, sender, plaintext); }
            }
            else 
            {
                friends_increment_unread(sender);
            }
        }

        memset(buf, 0, MESSAGE_MAX+crypto_box_SEALBYTES);
    }
    ui_cleanup();
    printf("Server disconnected\n");
    sqlite3_close(chat_db);
    sqlite3_close(friend_list);
    close(sock);
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

    printf("Connected to server.\n\n");

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

    if (!load_server_pk(server_pk)) 
    {
        fprintf(stderr, "server_pk.key not found in ~/.config/hermes/\n");
        fprintf(stderr, "copy it from the server dir before connecting.\n");
        close(sock);
        return 1;
    }

    get_credentials();

    if (!authenticate()) { return 1; }

    friend_db_init();

    ui_init();
    ui_set_header("(no chat open)");
    ui_draw_friends();

    ui_show_help();

    pthread_t recv_thread;
    int *sock_ptr = (int*)malloc(sizeof(int));
    *sock_ptr = sock;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 96 * 1024);

    if(pthread_create(&recv_thread, NULL, recv_handler, sock_ptr) < 0)
    {
        perror("error creating receiving handler");
        close(*sock_ptr);
        free(sock_ptr);
        pthread_attr_destroy(&attr);
        ui_cleanup();
        sqlite3_close(friend_list);
        sqlite3_close(chat_db);
        return 0;
    }
    pthread_attr_destroy(&attr);

    snprintf(prompt, sizeof(prompt), "(to %s): ", recipient);
    while(true) {
        char *msg = ui_get_input();  
        send_message(msg);
    }
    ui_cleanup();
    sqlite3_close(friend_list);
    sqlite3_close(chat_db);
    close(sock);
    return 0;
}