#pragma once

#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sqlcipher/sqlite3.h>
#include <time.h>
#include <signal.h>

#define USERNAME_SIZE 25
#define PASSWORD_SIZE 65

#define TYPE_BYTE 1
#define TYPE_MESSAGE 0
#define TYPE_FRIENDADD 1
#define TYPE_ERROR 2
#define TYPE_404 3

//PACKET STRUCTURE
#define TIMESTAMP_HEADER sizeof(int64_t)
#define HEADER (TYPE_BYTE + (USERNAME_SIZE*2) + TIMESTAMP_HEADER)                         
#define MSG_BODY_SIZE 2048
#define MESSAGE_MAX (HEADER + MSG_BODY_SIZE)

typedef struct { 
    char username[USERNAME_SIZE]; 
    char password[PASSWORD_SIZE];
    unsigned char pubkey[crypto_box_PUBLICKEYBYTES];
} auth_payload;

static ssize_t recv_all(int fd, void *buf, size_t len) {
    size_t recvd = 0;
    while (recvd < len) {
        ssize_t n = recv(fd, (char*)buf + recvd, len - recvd, 0);
        if (n <= 0) return n;
        recvd += n;
    }
    return recvd;
}

static ssize_t send_all(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, (char*)buf + sent, len - sent, 0);
        if (n <= 0) return n;
        sent += n;
    }
    return sent;
}