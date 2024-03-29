#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define BACKLOG 10

#define SIG_END  "\\1" // End of read signal
#define SIG_TERM "\\2" // Server termination signal

extern int errno;

char *home;

int start_server(int *sockfd, int port, struct sockaddr_in server) {
    printf("Starting server...\n");
    if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return 1;
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
    if (bind(*sockfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        return 1;
    }
    if (listen(*sockfd, BACKLOG) < 0) {
        return 1;
    }
    return 0;
}

int read_client(int connfd) {
    char buf[4096], *token, *rest = buf, *args[10];
    int ret, i = 0, p[2];
    if ((ret = read(connfd, buf, sizeof(buf))) < 0) {
        return 1;
    } else if (!ret) {
        return 0; // connection lost
    }
    if (buf[0] == '\n') { // check if client input is empty
        if (write(connfd, SIG_END, sizeof(SIG_END)) < 0) {
            return 1;
        }
        return read_client(connfd);
    }
    while ((token = strtok_r(rest, " ", &rest))) { // tokenizer
        args[i] = (char *)malloc(sizeof(char) * (strlen(token) + 1));
        strcpy(args[i++], token);
    }
    args[i] = NULL;
    memset(&buf, 0, sizeof(buf));
    if (pipe(p)) {
        return 1;
    }
    if (!strcmp(args[0], "cd")) {
        if (args[1] == NULL || !strcmp(args[1], "~")) {
            chdir(home);
        } else if (args[2] != NULL) {
            strcpy(buf, "Too many arguments\n");
            write(connfd, buf, sizeof(buf));
        } else if (args[1][0] == '~') {
            strcpy(buf, home);
            strcat(buf, strtok(args[1], "~"));
            if (chdir(buf) < 0) {
                memset(&buf, 0, sizeof(buf));
                strcpy(buf, strerror(errno));
                strcat(buf, "\n");
                write(connfd, buf, sizeof(buf));
            }
        } else if (chdir(args[1]) < 0) {
            strcpy(buf, strerror(errno));
            strcat(buf, "\n");
            write(connfd, buf, sizeof(buf));
        }
        if (write(connfd, SIG_END, sizeof(SIG_END)) < 0) {
            return 1;
        }
        return read_client(connfd);
    }
    switch (fork()) { // command execution
    case -1: // error
        return 1;
    case 0: // child
        close(connfd);
        close(p[0]);
        dup2(p[1], 1); // pipe stdout
        dup2(p[1], 2); // pipe stderr
        close(p[1]);
        execvp(args[0], args);
        perror("Failed execute command");
        exit(1);
    default: // parent
        wait(NULL);
        close(p[1]);
        while ((ret = read(p[0], buf, sizeof(buf))) != 0) {
            if (ret < 0 || write(connfd, buf, sizeof(buf)) < 0) {
                return 1;
            }
            memset(&buf, 0, sizeof(buf));
        }
        if (write(connfd, SIG_END, sizeof(SIG_END)) < 0) {
            return 1;
        }
        close(p[0]);
    }
    return read_client(connfd);
}

int run = 1;

void termination_handler(int signum) {
    run = 0;
    return;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Error: Invalid argument length\n"
                        "Usage: %s port hostname\n", argv[0]);
        exit(1);
    }
    char hostname[HOST_NAME_MAX], buf[4096];
    int sockfd, *connfd = (int *)malloc(sizeof(int)), curr = 0, port = atoi(argv[1]);
    struct sockaddr_in server, client;
    struct sigaction new_action, old_action;
    new_action.sa_handler = termination_handler;
    sigemptyset(&new_action.sa_mask);
    sigaddset(&new_action.sa_mask, SIGTERM);
    new_action.sa_flags = 0;
    sigaction(SIGINT, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN) {
        sigaction(SIGINT, &new_action, NULL);
    }
    if (start_server(&sockfd, port, server)) {
        perror("Failed to start server");
        exit(1);
    }
    if (argc == 3) {
        strcpy(hostname, argv[2]);
    } else {
        gethostname(hostname, sizeof(hostname));
    }
    FILE *fp = popen("echo ~", "r");
    fgets(buf, sizeof(buf), fp);
    pclose(fp);
    strtok(buf, "\n");
    home = (char *)malloc(sizeof(char) * (strlen(buf) + 1));
    strcpy(home, buf);
    chdir(home);
    while (run) {
        char conn_ip[INET_ADDRSTRLEN];
        int ret;
        pid_t pid;
        socklen_t len = sizeof(client);
        if ((ret = accept(sockfd, (struct sockaddr *)&client, &len)) < 0
        || (pid = fork()) < 0) {
            if (run) {
                perror("Failed to establish connection to client");
            }
        } else if (!pid) { // child
            close(sockfd);
            connfd[curr] = ret;
            inet_ntop(client.sin_family, &client.sin_addr.s_addr, conn_ip, sizeof(conn_ip));
            printf("Client connected from %s on pid:%d\n", conn_ip, getpid());
            write(connfd[curr], hostname, sizeof(hostname));
            if (read_client(connfd[curr])) {
                if (run) {
                    perror("Failed to read client");
                }
                exit(1);
            } else {
                printf("Client disconnected from %s\n", conn_ip);
                exit(0);
            }
        } else {
            connfd = (int *)realloc(connfd, (curr + 1) * sizeof(int));
            connfd[curr++] = ret;
        }
    }
    printf("\nShutting down server...\n");
    for (int i = 0; i < curr; i++) {
        write(connfd[i], SIG_TERM, sizeof(SIG_TERM));
        close(connfd[i]);
    }
    close(sockfd);
    return 0;
}
