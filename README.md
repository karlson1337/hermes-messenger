# hermes-messenger
A terminal based messenger for *nix written with TCP sockets in C/C++.

All code is in `src`

## Requirements
- Linux x86_64
- readline library (`sudo apt install libreadline-dev` or similar)

## Build and Run
- Run `make` from repo root.
- The client and server are in `bin`
- Run `hermes_server` and input port.
- You may now run clients (`hermes_client`). A client asks for following input: server ip, server port, your new username for the session, and the recipient's username.

## Roadmap
I plan to implement:

- Authentication using passwords.
- Offline message queues
- End to end encryption