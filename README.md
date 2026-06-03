# Hermes Messenger

A secure, end-to-end encrypted terminal messenger written in C/C++.

![C](https://img.shields.io/badge/C-A8B9CC?style=for-the-badge&logo=c&logoColor=white)
![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![macOS](https://img.shields.io/badge/macOS-000000?style=for-the-badge&logo=apple&logoColor=white)

---

## Features

- **End-to-end encryption** - messages are encrypted with the recipient's public key using X25519 + XSalsa20-Poly1305 (`crypto_box_seal`). The server forwards ciphertext it cannot read.
- **Secure authentication** - credentials are transmitted inside a sealed box encrypted to the server's public key. Passwords are never stored in plaintext; the server stores Argon2 hashes only.
- **Identity keys** - each user has an Ed25519 keypair. The private key is stored locally, encrypted with a key derived from the user's password via Argon2 (`crypto_pwhash`).
- **Encrypted chat history** - local message history is stored in a SQLCipher-encrypted SQLite database, keyed from the user's password.
- **Key binding** - the server binds each username to a public key at registration. Login with a mismatched key is rejected, preventing account takeover after key loss.
- **ncurses based TUI**
- **TCP keepalive** - dead connections are detected and cleaned up automatically.

---

## Dependencies

| Library | Purpose |
|---|---|
| [libsodium](https://libsodium.org) | All cryptography (encryption, hashing, key derivation) |
[SQLCipher](https://www.zetetic.net/sqlcipher/) | Friend list and encrypted chat history |
| .[ncurses](https://invisible-island.net/ncurses/) | Terminal user interface |
| pthreads | Concurrent connection handling |

### Install dependencies (Debian/Ubuntu)

```bash
sudo apt install libsodium-dev libsqlcipher-dev libncurses-dev
```

---

## Building

Install clang or gcc, then set `CXX` and `CC` in Makefile (clang++ and clang by default).

On Linux, use the provided Makefile from the repository root:

```bash
make
```

Or if on macOS and dependencies installed using brew:

```bash
make CFLAGS="-I$(brew --prefix)/include" LDFLAGS="-L$(brew --prefix)/lib"
```

---

## Setup

### Server

Run the server (in `bin/server`):

```bash
./hermes_server
```

The server will prompt for a port (default: `8080`). User identities and password hashes are stored in `hermes.db`, and message queue for offline users is stored in `hermes_messages.db`.

### Client

Run the client (in `bin/client`):

```bash
./hermes_client
```

On first run you will be prompted for a server address, port, username, and password. A new identity keypair will be generated and stored encrypted at `~/.config/hermes/<username>.key`. On subsequent runs the existing keypair is loaded and verified.

Friends db and encrypted chat db are also stored in the same directory.

> **Note:** If your `.key` file is lost, your account cannot be recovered. Keep a backup.

---

## Usage

```
/help               show available commands
/add  <username>    send a friend request
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
- **Authentication**: the auth payload (username, password, public key) is sealed to the server's public key - only the server can decrypt it.

---

### Packet structure

```
[ type (1B) | sender (25B) | recipient (25B) | timestamp (8B) | body (2048B) ]
```

Message bodies are `crypto_box_seal` ciphertext. Server packets (friend add responses, errors) are plaintext.

---

## Limitations & Known Issues

- The server generates a new keypair on each restart. Clients connecting after a restart will see a new server key (Trust On First Use model - first-connect key is trusted).
- No group chats.

---

## License

MIT
