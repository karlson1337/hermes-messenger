//Hermes server database code.

#include "../hermes_protocols.h"

extern sqlite3 *db;
extern unsigned char server_pk[crypto_box_PUBLICKEYBYTES];
extern unsigned char server_sk[crypto_box_SECRETKEYBYTES];

void db_init() {
    if (sqlite3_open("hermes.db", &db) != SQLITE_OK) {
        fprintf(stderr, "sqlite open: %s\n", sqlite3_errmsg(db));
        exit(1);
    }
    sqlite3_exec(db,
        "CREATE TABLE IF NOT EXISTS identities "
        "(username TEXT PRIMARY KEY, pwhash TEXT NOT NULL, pubkey BLOB NOT NULL);",
        NULL, NULL, NULL);
}

bool db_register(const char *username, const char *pwhash, const unsigned char *pubkey) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
        "INSERT INTO identities VALUES (?,?,?)", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, pwhash,   -1, SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 3, pubkey, crypto_sign_PUBLICKEYBYTES, SQLITE_STATIC);
    sqlite3_step(stmt);
    int changes = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    return changes > 0;
}
 
bool db_get_pwhash(const char *username, char *out_hash) {
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db,
        "SELECT pwhash FROM identities WHERE username=?", -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_ROW) { sqlite3_finalize(stmt); return false; }
    strcpy(out_hash, (const char*)sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    return true;
}

bool get_pubkey(const char *username, unsigned char *pubkey_out) {
    sqlite3_stmt *s;
    sqlite3_prepare_v2(db,
        "SELECT pubkey FROM identities WHERE username=?", -1, &s, NULL);
    sqlite3_bind_text(s, 1, username, -1, SQLITE_STATIC);
    int rc = sqlite3_step(s);
    if (rc != SQLITE_ROW) { sqlite3_finalize(s); return false; }
    memcpy(pubkey_out, sqlite3_column_blob(s, 0), crypto_sign_PUBLICKEYBYTES);
    sqlite3_finalize(s);
    return true;
}
