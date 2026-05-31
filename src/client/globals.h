//Hermes client global variables.

#pragma once

#include "../hermes_protocols.h"
#include <stdbool.h>
#include <termios.h>
#include <sys/stat.h>

extern sqlite3 *friend_list;
extern sqlite3 *chat_db;

extern char self[USERNAME_SIZE];
extern char password[PASSWORD_SIZE];

extern unsigned char self_pk[crypto_box_PUBLICKEYBYTES];
extern unsigned char self_sk[crypto_box_SECRETKEYBYTES];

extern unsigned char server_pk[crypto_box_PUBLICKEYBYTES];

extern char recipient[USERNAME_SIZE];

extern int sock;