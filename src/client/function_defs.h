#pragma once

#include "../hermes_protocols.h"
#include <stdbool.h>

//CHAT HISTORY DATABASE

bool derive_db_key(const char *password, const uint8_t *salt, char *hex_key_out);
void get_chatdb_path(const char *username, char *out, size_t len);
bool read_key_salt(unsigned char *salt_out);
bool chatdb_open(const char *password);
bool chatdb_insert(const char *sender, const char *recipient, int64_t timestamp, const char *content);
bool chatdb_history(const char *user_a, const char *user_b, int limit,
                   void (*cb)(int64_t ts, const char *sender, const char *msg, void *ud), void *ud);
void chatdb_close(void);

//FRIEND DATABASE

void friend_db_init();
void friends_add(const char *username, const unsigned char *pubkey);
bool friend_get_pubkey(const char *username, unsigned char *pubkey_out);
void list_friends();
bool friend_exists(const char *username);

//AUTH
void store_private_key(const unsigned char *sk, const char *password);
bool load_private_key(unsigned char *sk_out, const char *password);
void get_credentials();
bool authenticate();




