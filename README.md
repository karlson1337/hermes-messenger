# Hermes Messenger

A secure, end-to-end encrypted terminal messenger written in C/C++.

![C](https://img.shields.io/badge/C-A8B9CC?style=for-the-badge&logo=c&logoColor=white)
![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)

---

## Features

- **End-to-end encryption** — messages are encrypted with the recipient's public key using X25519 + XSalsa20-Poly1305 (`crypto_box_seal`). The server forwards ciphertext it cannot read.
- **Secure authentication** — credentials are transmitted inside a sealed box encrypted to the server's public key. Passwords are never stored in plaintext; the server stores Argon2 hashes only.
- **Identity keys** — each user has an Ed25519 keypair. The private key is stored locally, encrypted with a key derived from the user's password via Argon2 (`crypto_pwhash`).
- **Encrypted chat history** — local message history is stored in a SQLCipher-encrypted SQLite database, keyed from the user's password.
- **Key binding** — the server binds each username to a public key at registration. Login with a mismatched key is rejected, preventing account takeover after key loss.
- **TCP keepalive** — dead connections are detected and cleaned up automatically.

---

## Dependencies

| Library | Purpose |
|---|---|
| [libsodium](https://libsodium.org) | All cryptography (encryption, hashing, key derivation) |
[SQLCipher](https://www.zetetic.net/sqlcipher/) | Friend list and encrypted chat history |
| [readline](https://tiswww.case.edu/php/chet/readline/rltop.html) | Interactive terminal input |
| pthreads | Concurrent connection handling |

### Install dependencies (Debian/Ubuntu)

```bash
sudo apt install libsodium-dev libreadline-dev libsqlcipher-dev
```

---

## Building

Use the provided Makefile from the repository root:

```bash
make
```

---

## Setup

### Server

Run the server (in `bib/server`):

```bash
./hermes_server
```

The server will prompt for a port (default: `8080`). User identities and password hashes are stored in `hermes.db`.

### Client

```bash
./hermes_client
```

On first run you will be prompted for a server address, port, username, and password. A new identity keypair will be generated and stored encrypted at `~/.config/hermes/<username>.key`. On subsequent runs the existing keypair is loaded and verified.

> **Note:** If your `.key` file is lost, your account cannot be recovered without server-side intervention. Keep a backup.

---

## Usage

```
/help               show available commands
/add  <username>    send a friend request
/list               list your friends
/open <username>    open a chat with a friend
/quit               disconnect and exit
```

Type a message and press **Enter** to send. You must `/open` a chat before sending messages.

---

## Security Model

```
Client A > Server > Client B
            │
    sees: sender, recipient, timestamp
    cannot see: message content
```

- **End-to-end**: message bodies are encrypted with `crypto_box_seal` (X25519 ECDH + XSalsa20-Poly1305). The server cannot decrypt messages.
- **Authentication**: the auth payload (username, password, public key) is sealed to the server's public key — only the server can decrypt it.

---

### Packet structure

```
[ type (1B) | sender (25B) | recipient (25B) | timestamp (8B) | body (2048B) ]
```

Message bodies are `crypto_box_seal` ciphertext. Server packets (friend add responses, errors) are plaintext.

---

## Limitations & Known Issues

- Offline messaging is not yet implemented — messages sent to offline users are dropped. (Coming soon though)
- The server generates a new keypair on each restart. Clients connecting after a restart will see a new server key (TOFU model — first-connect key is trusted).
- No group chats.

---

## License

MIT
