#pragma once

#define USERNAME_SIZE 25
#define HEADER (USERNAME_SIZE*2)
#define MESSAGE_MAX 2098
#define MSG_BODY_SIZE (MESSAGE_MAX - HEADER)

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