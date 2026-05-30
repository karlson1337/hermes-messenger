#include "globals.h"
#include "function_defs.h"

void store_private_key(const unsigned char *sk, const char *password) 
{
    unsigned char salt[crypto_pwhash_SALTBYTES];
    unsigned char wrapping_key[crypto_secretbox_KEYBYTES];
    randombytes_buf(salt, sizeof(salt));
    if(crypto_pwhash(wrapping_key, sizeof(wrapping_key),
        password, strlen(password), salt,
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE,
        crypto_pwhash_ALG_DEFAULT) != 0) 
        { /* OOM */ sodium_memzero(wrapping_key, crypto_secretbox_KEYBYTES); return; }

    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    unsigned char ciphertext[crypto_secretbox_MACBYTES + crypto_sign_SECRETKEYBYTES];
    randombytes_buf(nonce, sizeof(nonce));
    
    if(crypto_secretbox_easy(ciphertext, sk, crypto_sign_SECRETKEYBYTES, nonce, wrapping_key) != 0)
    { sodium_memzero(wrapping_key, crypto_secretbox_KEYBYTES); return; }

    char path[256];
    snprintf(path, sizeof(path), "%s/.config/hermes/%s.key", getenv("HOME"), self);
    FILE *f = fopen(path, "wb");
    fwrite(salt,       1, sizeof(salt),       f);
    fwrite(nonce,      1, sizeof(nonce),       f);
    fwrite(ciphertext, 1, sizeof(ciphertext),  f);
    fclose(f);
    chmod(path, 0600);

    sodium_memzero(wrapping_key, sizeof(wrapping_key));
}

bool load_private_key(unsigned char *sk_out, const char *password) 
{
    char path[256];
    snprintf(path, sizeof(path), "%s/.config/hermes/%s.key", getenv("HOME"), self);
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    unsigned char salt[crypto_pwhash_SALTBYTES];
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    unsigned char ciphertext[crypto_secretbox_MACBYTES + crypto_sign_SECRETKEYBYTES];
    fread(salt,       1, sizeof(salt),       f);
    fread(nonce,      1, sizeof(nonce),       f);
    fread(ciphertext, 1, sizeof(ciphertext),  f);
    fclose(f);

    unsigned char wrapping_key[crypto_secretbox_KEYBYTES];
    if(crypto_pwhash(wrapping_key, sizeof(wrapping_key),
        password, strlen(password), salt,
        crypto_pwhash_OPSLIMIT_INTERACTIVE,
        crypto_pwhash_MEMLIMIT_INTERACTIVE,
        crypto_pwhash_ALG_DEFAULT) != 0)
        { sodium_memzero(wrapping_key, crypto_secretbox_KEYBYTES); return false; }

    bool ok = crypto_secretbox_open_easy(sk_out, ciphertext, sizeof(ciphertext),
                                          nonce, wrapping_key) == 0;
    sodium_memzero(wrapping_key, sizeof(wrapping_key));
    return ok;
}

void get_credentials()
{
    printf("enter username (max 24 characters): ");
    scanf("%24s", self);
    { int c; while ((c = getchar()) != '\n' && c != EOF); }

    struct termios old, new;
    tcgetattr(STDIN_FILENO, &old);
    new = old;
    new.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new);

    size_t pwlen;
    do {
        printf("enter password (8-64 characters): ");
        fgets(password, PASSWORD_SIZE, stdin);
        password[strcspn(password, "\n")] = '\0';
        pwlen = strlen(password);
        if (pwlen < 8)  printf("\npassword too short.\n");
        if (pwlen > 64) printf("\npassword too long.\n");
    } while (pwlen < 8 || pwlen > 64);
    tcsetattr(STDIN_FILENO, TCSANOW, &old);

    printf("\n");
}

bool authenticate()
{
    char dir[256];
    snprintf(dir, sizeof(dir), "%s/.config/hermes", getenv("HOME"));
    mkdir(dir, 0700);

    char key_path[256];
    snprintf(key_path, sizeof(key_path), "%s/%s.key", dir, self);
    bool key_exists = access(key_path, F_OK) == 0;

    if (!key_exists) {
        crypto_sign_keypair(self_pk, self_sk);
        store_private_key(self_sk, password);
    } else if (!load_private_key(self_sk, password)) {
        fprintf(stderr, "wrong password\n");
        return false;
    } else {
        crypto_sign_ed25519_sk_to_pk(self_pk, self_sk);
    }

    auth_payload payload = {0};
    strncpy(payload.username, self, USERNAME_SIZE-1);
    strncpy(payload.password, password, PASSWORD_SIZE-1);
    memcpy(payload.pubkey, self_pk, crypto_sign_PUBLICKEYBYTES);

    size_t cipher_len = sizeof(auth_payload) + crypto_box_SEALBYTES;
    unsigned char* ciphertext = (unsigned char*)malloc(cipher_len);
    
    crypto_box_seal(ciphertext, (const unsigned char*)&payload, sizeof(auth_payload), server_pk);
    
    if (send_all(sock, ciphertext, cipher_len) <= 0) {
        fprintf(stderr, "Failed to send auth payload\n");
        free(ciphertext);
        close(sock);
        return false;
    }
    free(ciphertext);

    sodium_memzero(&payload, sizeof(payload));

    uint8_t resp;
    if (recv_all(sock, &resp, 1) <= 0 || resp == 0) 
    {
        fprintf(stderr, "Authentication failed: Incorrect password or key mismatch.\nIf you are sure your password is correct, your key may have been deleted.\n");
        close(sock);
        return false;
    }
    else if (resp == 1) { chatdb_open(password); sodium_memzero(password, strlen(password));
        printf("logged in successfully!\n"); return true; } 
    else if (resp == 2) {chatdb_open(password); sodium_memzero(password, strlen(password));
        printf("registered successfully as a new user!\n"); return true; }
    else {printf("unknown error.\n"); sodium_memzero(password, strlen(password));
        return false; }

}
