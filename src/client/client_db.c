//Hermes client databases code.

#include "globals.h"
#include "function_defs.h"
#include <readline/readline.h>

//========CHAT HISTORY DATABASE CODE========

bool derive_db_key(const char *password, const uint8_t *salt, char *hex_key_out) {
    uint8_t key[32];
    if (crypto_pwhash(key, sizeof(key), password, strlen(password), salt,
                      crypto_pwhash_OPSLIMIT_INTERACTIVE,
                      crypto_pwhash_MEMLIMIT_INTERACTIVE,
                      crypto_pwhash_ALG_DEFAULT) != 0)
        return false;

    sodium_bin2hex(hex_key_out, 65, key, sizeof(key));
    sodium_memzero(key, sizeof(key));
    return true;
}

void get_chatdb_path(const char *username, char *out, size_t len) {
    const char *home = getenv("HOME");
    char dir[256];
    snprintf(dir, sizeof(dir), "%s/.config/hermes", home);
    mkdir(dir, 0700);
    snprintf(out, len, "%s/%s.db", dir, username);
}

bool read_key_salt(unsigned char *salt_out) {
    char path[256];
    snprintf(path, sizeof(path), "%s/.config/hermes/%s.key", getenv("HOME"), self);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fread(salt_out, 1, crypto_pwhash_SALTBYTES, f);
    fclose(f);
    return true;
}

bool chatdb_open(const char *password) {
    uint8_t salt[crypto_pwhash_SALTBYTES];
    if (!read_key_salt(salt)) return false;

    char hex_key[65];
    if (!derive_db_key(password, salt, hex_key)) return false;

    char db_path[256];
    get_chatdb_path(self, db_path, sizeof(db_path));
    if (sqlite3_open(db_path, &chat_db) != SQLITE_OK) return false;
    char pragma[128];

    snprintf(pragma, sizeof(pragma), "PRAGMA key = \"x'%s'\";", hex_key);
    sodium_memzero(hex_key, sizeof(hex_key));

    if (sqlite3_exec(chat_db, pragma, NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_close(chat_db);
        chat_db = NULL;
        return false;
    }
    sodium_memzero(pragma, sizeof(pragma));

    return sqlite3_exec(chat_db,
        "CREATE TABLE IF NOT EXISTS messages ("
        "  timestamp INTEGER NOT NULL,"
        "  sender    TEXT    NOT NULL,"
        "  recipient TEXT    NOT NULL,"
        "  content   TEXT    NOT NULL"
        ");",
        NULL, NULL, NULL) == SQLITE_OK ? true : false;
}

bool chatdb_insert(const char *sender, const char *recipient, int64_t timestamp, const char *content) {
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO messages(timestamp,sender,recipient,content) VALUES(?,?,?,?);";

    if (sqlite3_prepare_v2(chat_db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, timestamp);
    sqlite3_bind_text(stmt,  2, sender,    -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt,  3, recipient, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt,  4, content,   -1, SQLITE_STATIC);

    bool rc = sqlite3_step(stmt) == SQLITE_DONE ? true : false;
    sqlite3_finalize(stmt);
    return rc;
}

bool chatdb_history(const char *user_a, const char *user_b, int limit,
                   void (*cb)(int64_t ts, const char *sender, const char *msg, void *ud), void *ud) {
    sqlite3_stmt *stmt;
    const char *sql =
        "SELECT timestamp, sender, content FROM messages "
        "WHERE (sender=? AND recipient=?) OR (sender=? AND recipient=?) "
        "ORDER BY timestamp ASC LIMIT ?;";

    if (sqlite3_prepare_v2(chat_db, sql, -1, &stmt, NULL) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, user_a, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user_b, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, user_b, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, user_a, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt,  5, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW)
        cb(sqlite3_column_int64(stmt, 0),
           (const char *)sqlite3_column_text(stmt, 1),
           (const char *)sqlite3_column_text(stmt, 2), ud);

    sqlite3_finalize(stmt);
    return true;
}

void chatdb_close(void) {
    sqlite3_close(chat_db);
    chat_db = NULL;
}

//========FRIEND DATABASE CODE========

void friend_db_init()
{
    char path[256];
    snprintf(path, sizeof(path), "%s/.config/hermes/%s_friends.db", getenv("HOME"), self);
    sqlite3_open(path, &friend_list);
    sqlite3_exec(friend_list,
        "CREATE TABLE IF NOT EXISTS friends "
        "(username TEXT PRIMARY KEY, pubkey BLOB NOT NULL);",
        NULL, NULL, NULL);
}

void friends_add(const char *username, const unsigned char *pubkey) 
{
    sqlite3_stmt *s;
    sqlite3_prepare_v2(friend_list,
        "INSERT OR REPLACE INTO friends VALUES (?,?)", -1, &s, NULL);
    sqlite3_bind_text(s, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_blob(s, 2, pubkey, crypto_box_PUBLICKEYBYTES, SQLITE_STATIC);
    sqlite3_step(s);
    sqlite3_finalize(s);
}

bool friend_get_pubkey(const char *username, unsigned char *pubkey_out) 
{
    sqlite3_stmt *s;
    sqlite3_prepare_v2(friend_list,
        "SELECT pubkey FROM friends WHERE username=?", -1, &s, NULL);
    sqlite3_bind_text(s, 1, username, -1, SQLITE_STATIC);
    if (sqlite3_step(s) != SQLITE_ROW) { sqlite3_finalize(s); return false; }
    memcpy(pubkey_out, sqlite3_column_blob(s, 0), crypto_box_PUBLICKEYBYTES);
    sqlite3_finalize(s);
    return true;
}

void list_friends() 
{
    sqlite3_stmt *s;
    sqlite3_prepare_v2(friend_list, "SELECT username FROM friends", -1, &s, NULL);
    printf("friends:\n");
    while (sqlite3_step(s) == SQLITE_ROW)
        printf("  %s\n", sqlite3_column_text(s, 0));
    sqlite3_finalize(s);
}

bool friend_exists(const char *username) 
{
    sqlite3_stmt *s;
    sqlite3_prepare_v2(friend_list,
        "SELECT 1 FROM friends WHERE username=?", -1, &s, NULL);
    sqlite3_bind_text(s, 1, username, -1, SQLITE_STATIC);
    bool exists = sqlite3_step(s) == SQLITE_ROW;
    sqlite3_finalize(s);
    return exists;
}
