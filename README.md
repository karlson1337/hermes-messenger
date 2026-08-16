# Hermes Messenger

A secure, end-to-end encrypted terminal messenger written in C/C++.

![C](https://img.shields.io/badge/C-A8B9CC?style=for-the-badge&logo=c&logoColor=white)
![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)
![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![macOS](https://img.shields.io/badge/macOS-000000?style=for-the-badge&logo=apple&logoColor=white)

---

## Features

- **End-to-end encryption** - messages are encrypted with the recipient's public key using X25519 + XSalsa20-Poly1305 (`crypto_box_seal`). The server forwards ciphertext it cannot read. Can be verified with security numbers.
- **Secure authentication** - credentials are transmitted inside a sealed box encrypted to the server's public key. Passwords are never stored in plaintext; the server stores Argon2 hashes only.
- **Identity keys** - each user has an X25519 keypair. The private key is stored locally, encrypted with a key derived from the user's password via Argon2 (`crypto_pwhash`).
- **Encrypted chat history** - local message history is stored in a SQLCipher-encrypted SQLite database, keyed from the user's password.
- **Key binding** - the server binds each username to a public key at registration. Login with a mismatched key is rejected, preventing account takeover after key loss.
- **ncurses based TUI**
- **TCP keepalive** - dead connections are detected and cleaned up automatically.

---

## Try Hermes (Supports Debian/Ubuntu, Fedora/RHEL, Arch Linux)
### Hosted on AWS EC2

```bash
curl -sL https://raw.githubusercontent.com/karlson1337/hermes-messenger/aws_host_scripts_deprecated/hermes_client_installer.sh | bash
```

### Privacy Notice for above:
No data is logged. Only some data is temporarily stored (offline message queues). Server can see metadata (sender and receiver usernames, timestamp) but this info is not logged except for above mentioned offline message queues (temporarily).

## Dependencies

| Library | Purpose |
|---|---|
| [libsodium](https://libsodium.org) | All cryptography (encryption, hashing, key derivation) |
| [SQLCipher](https://www.zetetic.net/sqlcipher/) | Friend list and encrypted chat history |
| [ncurses](https://invisible-island.net/ncurses/) | Terminal user interface |

### Install dependencies

(Debian/Ubuntu)

```bash
sudo apt install libsodium-dev libsqlcipher-dev libncurses-dev libreadline-dev
```

(Fedora/RHEL)

```bash
sudo dnf install libsodium-devel sqlcipher-devel ncurses-devel readline-devel
```

(Arch Linux)

```bash
sudo pacman -S libsodium sqlcipher ncurses readline
```

(macOS)

```bash
brew install libsodium sqlcipher ncurses readline
```

(Visit https://brew.sh/ for information on how to install homebrew)

---

## Building

Install clang or gcc, then set `CXX` and `CC` in Makefile (g++ and gcc by default).

Run from repository root

Linux:

```bash
make linux
```

macOS:

```bash
make macos
```

---

## Setup

### Server

Run the server (in `bin/server`):

```bash
./hermes_server
```

The server will prompt for a port (default: `8080`). User identities and password hashes are stored in `hermes.db`, and message queue for offline users is stored in `hermes_messages.db`.

On first setup, copy the generated `server_pk.key` to `~/.config/hermes/` on the clients' computers before running the clients. This key is used to encrypt credentials during authentication. If it doesn't match the server's actual public key, authentication will fail.

> **Note:** Never copy server_sk.key to any client computer. If it leaks, an attacker can impersonate the server and decrypt user credentials.

### Client

Run the client (in `bin/client`):

```bash
./hermes_client
```

On first run you will be prompted for a server address, port, username, and password. A new identity keypair will be generated and stored encrypted at `~/.config/hermes/<username>.key`. On subsequent runs the existing keypair is loaded and verified, and existing host `~/.config/hermes/host` is used.

Friends db and encrypted chat db are also stored in the same directory.

> **Note:** If your `.key` file is lost, your account cannot be recovered. Keep a backup.

---

## Usage

```
/help                 show available commands
/add    <username>    add someone as a friend
/open   <username>    open a chat with a friend
/verify <username>    verify if encryption is working
/quit                 disconnect and exit
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
- **Encryption verification** — `/verify <username>` derives a fingerprint from both users' public keys. If both parties see the same string out-of-band (e.g. over a phone call), it confirms no man-in-the-middle has substituted either key. Equivalent to safety numbers on Signal or WhatsApp.

---

### Packet structure

```
[ type (1B) | sender (25B) | recipient (25B) | timestamp (8B) | body (2048B) ]
```

Message bodies are `crypto_box_seal` ciphertext. Server packets (friend add responses, errors) are plaintext.

---

## Limitations & Known Issues

- No group chats.
- No forward secrecy: no ephemeral keys per session (static X25519)

---

## License

MIT
