# Inter-Process Chat Application Using IPC

## Overview
A two-way terminal chat prototype written in C for Linux. It uses two named pipes (FIFOs): one for Process A → Process B and one for Process B → Process A.

## Requirements
- Linux, Ubuntu, or WSL with a Linux distribution
- GCC compiler
- Two terminal windows in the same Linux environment

## Compile
From the project directory:
```bash
gcc -Wall -Wextra -std=c11 -o ipc_chat src/chat.c
```
If your compiler reports `usleep` as undeclared, compile with:
```bash
gcc -Wall -Wextra -D_DEFAULT_SOURCE -std=c11 -o ipc_chat src/chat.c
```

## Run
Open Terminal 1:
```bash
./ipc_chat A
```
Open Terminal 2 in the same directory:
```bash
./ipc_chat B
```
Wait for both terminals to show `Connected!`. Type a message and press Enter. Type `/quit` to exit.

## IPC design
- `/tmp/ipc_chat_A_to_B`: messages from A to B
- `/tmp/ipc_chat_B_to_A`: messages from B to A
- `select()` watches the keyboard and incoming FIFO, allowing either side to send or receive while the program runs.
- `SIGINT` and `SIGTERM` request a controlled exit.

## Cleanup
After both processes exit:
```bash
rm -f /tmp/ipc_chat_A_to_B /tmp/ipc_chat_B_to_A
```
Only remove these paths when your project processes are stopped.

## Testing checklist
1. Start A first, then B.
2. Start B first, then A.
3. Send messages from A to B.
4. Send messages from B to A.
5. Send several messages in each direction.
6. Type `/quit` and confirm the process exits.
7. Try an invalid role such as `./ipc_chat C`; it should print usage and exit.
8. Confirm the two FIFO entries exist while the application is running with `ls -l /tmp/ipc_chat_A_to_B /tmp/ipc_chat_B_to_A`.

## Limitations
- Both processes must run on the same host / Linux environment.
- This is an educational prototype, not a production messaging service.
- No authentication, encryption, message history, or multi-user room is provided.
- Test on the exact lab machine before evaluation; IPC behaviour and compiler feature macros can vary across environments.
