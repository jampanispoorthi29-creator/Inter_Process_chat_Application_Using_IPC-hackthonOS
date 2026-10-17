#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <time.h>

#define A2B "/tmp/ipc_chat_A_to_B"
#define B2A "/tmp/ipc_chat_B_to_A"
#define SIZE 512

static volatile sig_atomic_t running = 1;

void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

int make_fifo(const char *path) {
    if (mkfifo(path, 0666) == 0 || errno == EEXIST) {
        struct stat st;
        if (lstat(path, &st) == 0 && S_ISFIFO(st.st_mode))
            return 0;
        fprintf(stderr, "%s is not a FIFO\n", path);
        return -1;
    }
    perror("mkfifo");
    return -1;
}

int main(int argc, char *argv[]) {
    const char *role;

    if (argc == 2) {
        role = argv[1];
    } else if (argc == 3 && strcmp(argv[1], "--chat") == 0) {
        role = argv[2];
    } else {
        printf("Usage: %s A|B\n", argv[0]);
        return 1;
    }

    if (strcmp(role, "A") != 0 && strcmp(role, "B") != 0) {
        printf("Choose A or B\n");
        return 1;
    }

    int isA = strcmp(role, "A") == 0;
    const char *outPath = isA ? A2B : B2A;
    const char *inPath  = isA ? B2A : A2B;
    const char *peer = isA ? "Process B" : "Process A";

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGPIPE, SIG_IGN);

    if (make_fifo(A2B) == -1 || make_fifo(B2A) == -1)
        return 1;

    printf("=== IPC Chat: Process %s ===\n", role);
    printf("Waiting for the other process...\n");
    fflush(stdout);

    int in = open(inPath, O_RDONLY | O_NONBLOCK);
    if (in == -1) {
        perror("open input FIFO");
        return 1;
    }

    int out = -1;
    while (running) {
        out = open(outPath, O_WRONLY | O_NONBLOCK);

        if (out >= 0)
            break;

        if (errno != ENXIO && errno != EINTR) {
            perror("open output FIFO");
            close(in);
            return 1;
        }

        struct timespec delay = {0, 100000000};
        nanosleep(&delay, NULL);
    }

    if (!running) {
        close(in);
        return 0;
    }

    int flags = fcntl(in, F_GETFL);
    if (flags != -1)
        fcntl(in, F_SETFL, flags & ~O_NONBLOCK);

    printf("Connected! Type messages; enter /quit to exit.\n");
    printf("You are Process %s.\n", role);
    printf("You: ");
    fflush(stdout);

    char buf[SIZE];

    while (running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(in, &readfds);

        int maxfd = in > STDIN_FILENO ? in : STDIN_FILENO;

        int result = select(maxfd + 1, &readfds, NULL, NULL, NULL);

        if (result == -1) {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        /* Receive messages from the other process */
        if (FD_ISSET(in, &readfds)) {
            ssize_t n = read(in, buf, SIZE - 1);

            if (n > 0) {
                buf[n] = '\0';

                if (strcmp(buf, "/quit\n") == 0 ||
                    strcmp(buf, "/quit") == 0) {
                    printf("\n[%s disconnected]\n", peer);
                    running = 0;
                    break;
                }

                printf("\n[%s received]: %s", peer, buf);

                if (buf[n - 1] != '\n')
                    putchar('\n');

                printf("You: ");
                fflush(stdout);
            } else if (n == 0) {
                printf("\n[%s disconnected]\n", peer);
                running = 0;
                break;
            } else if (errno != EINTR && errno != EAGAIN) {
                perror("read");
                break;
            }
        }

        /* Send messages typed in this terminal */
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            ssize_t n = read(STDIN_FILENO, buf, SIZE - 1);

            if (n <= 0) {
                running = 0;
                break;
            }

            buf[n] = '\0';

            if (strcmp(buf, "/quit\n") == 0 ||
                strcmp(buf, "/quit") == 0) {
                const char *quit = "/quit\n";
                (void)write(out, quit, strlen(quit));
                running = 0;
                break;
            }

            ssize_t sent = 0;
            while (sent < n) {
                ssize_t w = write(out, buf + sent, (size_t)(n - sent));
                if (w == -1) {
                    if (errno == EINTR)
                        continue;
                    perror("write");
                    running = 0;
                    break;
                }
                sent += w;
            }

            if (!running)
                break;

            printf("You: ");
            fflush(stdout);
        }
    }

    close(in);
    close(out);
    printf("\nProcess %s disconnected.\n", role);

    return 0;
}
