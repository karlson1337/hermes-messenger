//Hermes messenger main server code.

#include "../hermes_protocols.h"

#include <signal.h>
#include <netinet/tcp.h>
#include <string>
#include <unordered_map>
#include <queue>

sqlite3 *db;
unsigned char server_pk[crypto_box_PUBLICKEYBYTES];
unsigned char server_sk[crypto_box_SECRETKEYBYTES];

void db_init();
bool db_register(const char *username, const char *pwhash, const unsigned char *pubkey);
bool db_get_pwhash(const char *username, char *out_hash);
bool get_pubkey(const char *username, unsigned char *pubkey_out);

typedef struct User
{
    std::string username;
    int sock;
    pthread_mutex_t send_mutex;
    std::queue<std::string> offline_queue;

    User() { pthread_mutex_init(&send_mutex, nullptr); }
    ~User() { pthread_mutex_destroy(&send_mutex); }
}User;

std::unordered_map<std::string, User*> Users;
pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;

bool authenticate(int sock, char username_out[USERNAME_SIZE]) {

    if (send_all(sock, server_pk, crypto_box_PUBLICKEYBYTES) <= 0) return false;

    size_t cipher_len = sizeof(auth_payload) + crypto_box_SEALBYTES;
    unsigned char* ciphertext = (unsigned char*)malloc(cipher_len);
    if (recv_all(sock, ciphertext, cipher_len) <= 0) {
        free(ciphertext);
        return false;
    }

    auth_payload payload;
    if (crypto_box_seal_open((unsigned char*)&payload, ciphertext, cipher_len, server_pk, server_sk) != 0) {
        printf("Auth payload decryption failed\n");
        free(ciphertext);
        return false;
    }
    free(ciphertext);

    payload.username[USERNAME_SIZE-1] = '\0';
    payload.password[PASSWORD_SIZE-1] = '\0';

    unsigned char temp_pk[crypto_box_PUBLICKEYBYTES];

    char stored_hash[crypto_pwhash_STRBYTES];
    uint8_t resp = 0;

    if(Users.find(payload.username) != Users.end())
    {
        resp = 3;
        send_all(sock, &resp, 1);
        return false;
    }

    if (!db_get_pwhash(payload.username, stored_hash)) {

        if (crypto_pwhash_str(stored_hash, payload.password, strlen(payload.password),
                              crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
            send_all(sock, &resp, 1);
            return false;
        }
        if (db_register(payload.username, stored_hash, payload.pubkey)) {
            resp = 2;
            send_all(sock, &resp, 1);
            memcpy(username_out, payload.username, USERNAME_SIZE);
            printf("Registered new user: %s\n", username_out);
            return true;
        }
    } 
    else 
    {
        if (crypto_pwhash_str_verify(stored_hash, payload.password, strlen(payload.password)) == 0) 
        {
            unsigned char stored_pk[crypto_box_PUBLICKEYBYTES];
            if (!get_pubkey(payload.username, stored_pk) ||
                memcmp(stored_pk, payload.pubkey, crypto_box_PUBLICKEYBYTES) != 0)
            {
                printf("Pubkey mismatch for: %s\n", payload.username);
                send_all(sock, &resp, 1);
                return false;
            }

            resp = 1;
            send_all(sock, &resp, 1);
            memcpy(username_out, payload.username, USERNAME_SIZE);
            printf("Authenticated user: %s\n", username_out);
            return true;
        } 
        else { printf("Auth failed (wrong password) for: %s\n", payload.username); }
    }

    send_all(sock, &resp, 1);
    return false;
}

void *connection_handler(void *socket_desc) {
    User *u = new User();

    int sock = *(int*)socket_desc;
    free(socket_desc);

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

    char username[USERNAME_SIZE] = {0};
    if (!authenticate(sock, username)) 
    {
        close(sock);
        return NULL;
    }

    u->username = std::string(username);
    u->sock = sock;

    pthread_mutex_lock(&table_mutex);
    Users[u->username] = u;
    pthread_mutex_unlock(&table_mutex);

    while (1) {
        char buf[MESSAGE_MAX+crypto_box_SEALBYTES] = {0};
        if (recv_all(sock, buf, MESSAGE_MAX+crypto_box_SEALBYTES) <= 0) break;

        char recipient[USERNAME_SIZE];
        memcpy(recipient, buf+TYPE_BYTE+USERNAME_SIZE, USERNAME_SIZE);

        if(buf[0] == TYPE_FRIENDADD)
        {
            char temp[MESSAGE_MAX + crypto_box_SEALBYTES] = {0};
            unsigned char temp_pk[crypto_box_PUBLICKEYBYTES];
            strncpy(temp+1, "server", 6);
            if(!get_pubkey(recipient, temp_pk)) 
            {
                temp[0] = TYPE_404;
                memcpy(temp+HEADER, "user does not exist!", 20);
                send_all(u->sock, temp, MESSAGE_MAX+crypto_box_SEALBYTES);
                continue;
            }
            temp[0] = TYPE_FRIENDADD;

            strncpy(temp+TYPE_BYTE+USERNAME_SIZE, recipient, USERNAME_SIZE);
            memcpy(temp + HEADER, temp_pk, crypto_box_PUBLICKEYBYTES);
            send_all(u->sock, temp, MESSAGE_MAX + crypto_box_SEALBYTES);
            continue;
        }

        int temp_fd;

        pthread_mutex_lock(&table_mutex);
        if(Users.find(std::string(recipient)) == Users.end()) 
        {
            pthread_mutex_unlock(&table_mutex);
            continue;
        }
        temp_fd = (Users[std::string(recipient)])->sock;
        pthread_mutex_unlock(&table_mutex);
        send_all(temp_fd, buf, MESSAGE_MAX+crypto_box_SEALBYTES);
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
    sqlite3_close(db);
    exit(0);
}

int main() {
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    if (sodium_init() < 0) { fprintf(stderr, "sodium init failed\n"); return 1; }
    
    crypto_box_keypair(server_pk, server_sk);
    
    db_init();

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

    printf("listening on port %d\n", port);

    while (1) 
    {
        if((client_fd = accept(server_fd, (struct sockaddr *)&addr, &addrlen)) < 0)
        {
            if (errno == EINTR || errno == ECONNABORTED) continue;
            perror("accept");
            break;
        }
        printf("client connected\n");

        pthread_t listener_thread;
        new_fd = (int*)malloc(sizeof(int));
        *new_fd = client_fd;

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 96 * 1024); //set stack size to 96kb (default 8mb is way too much)

        if (pthread_create(&listener_thread, &attr, connection_handler, (void*)new_fd) < 0) 
        {
            perror("error creating new thread");
            close(*new_fd);
            free(new_fd);
            if (errno == EAGAIN) { fprintf(stderr, "server might be under high load.\n"); }
            pthread_attr_destroy(&attr);
            continue;
        }
        pthread_detach(listener_thread);
        pthread_attr_destroy(&attr);
    }
    return 0;
}