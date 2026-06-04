//Hermes client function definitions.

#pragma once

#include "../hermes_protocols.h"
#include <stdbool.h>

//UI

void ui_init();
void ui_set_header(const char *name);
void ui_print_message(uint64_t timestamp, const char *sender, const unsigned char *msg);
void ui_draw_friends();
void ui_show_help();
void ui_print_system_message(const char *msg);
char *ui_get_input();
void handle_resize();
void redraw_chat();
void ui_clear_chat();
void ui_cleanup();

//CHAT HISTORY DATABASE

bool derive_db_key(const char *password, const uint8_t *salt, char *hex_key_out);
void get_chatdb_path(const char *username, char *out, size_t len);
bool read_key_salt(unsigned char *salt_out);
bool chatdb_open(const char *password);
bool chatdb_insert(const char *sender, const char *recipient, int64_t timestamp, const unsigned char *content);
bool chatdb_history(const char *user_a, const char *user_b, int limit, int offset,
                   void (*cb)(int64_t ts, const char *sender, const unsigned char *msg, void *ud), void *ud);
int chatdb_get_count(const char *user_a, const char *user_b);

//FRIEND DATABASE

void friend_db_init();
void friends_add(const char *username, const unsigned char *pubkey);
bool friend_get_pubkey(const char *username, unsigned char *pubkey_out);
bool friend_exists(const char *username);
void friends_increment_unread(const char *username);
void friends_clear_unread(const char *username);

//AUTH

bool load_server_pk(unsigned char *pk_out);
void store_private_key(const unsigned char *sk, const char *password);
bool load_private_key(unsigned char *sk_out, const char *password);
void get_credentials();
bool authenticate();




