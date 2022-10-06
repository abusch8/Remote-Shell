#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <limits.h>
#include <signal.h>
#include <arpa/inet.h>

#define BACKLOG 10
#define SIG "\0"

extern int errno;

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
    char buf[1256], *token, *rest = buf, *args[10];
    int ret, i = 0, fd[2];
    if ((ret = read(connfd, buf, sizeof(buf))) < 0) {
        return 1;
    } else if (!ret) {
        return 0; // client disconnected
    }
    while ((token = strtok_r(rest, " ", &rest))) { // tokenizer
        args[i] = (char *)malloc(sizeof(char) * (strlen(token) + 1));
        strcpy(args[i++], token);
    }
    args[i] = NULL;
    memset(&buf, 0, sizeof(buf));
    if (pipe(fd)) {
        return 1;
    }
    if (!strcmp(args[0], "cd")) {
        if (args[2] != NULL) { // TODO
            // char error[] = "Too many arguments\n";
            // printf("%s\n", error);
            // write(new_fd, error, sizeof(error));
        } else if (args[1] != NULL && chdir(args[1]) < 0) {
            // char error[strlen(strerror(errno)) + 1];
            // snprintf(error, sizeof(error), "%s\n", strerror(errno));
            // snprintf(error, strlen(strerror(errno)) + 1, "%s\n", strerror(errno));
            // char *error = strcpy(error, strerror(errno));
            // error = strcat(error, "\n");
            // write(new_fd, error, sizeof(error));
        }
        if (write(connfd, SIG, sizeof(SIG)) < 0) {
            return 1;
        }
        return 0;
    }
    switch (fork()) { // command execution
        case -1 : return 1; // error
        case  0 : // child
            close(fd[0]);
            dup2(fd[1], 1); // pipe stdout
            dup2(fd[1], 2); // pipe stderr
            close(fd[1]);
            execvp(args[0], args);
            perror("Failed execute command");
            exit(1);
        default : // parent
            wait(NULL);
            close(fd[1]);
            while ((ret = read(fd[0], buf, sizeof(buf))) != 0) {
                if (ret < 0 || write(connfd, buf, sizeof(buf)) < 0) {
                    return 1;
                }
                memset(&buf, 0, sizeof(buf));
            }
            if (write(connfd, SIG, sizeof(SIG)) < 0) {
                return 1;
            }
            close(fd[0]);
    }
    return read_client(connfd);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Error: Invalid argument length\n"
                        "Usage: %s port hostname\n", argv[0]);
        exit(1);
    }
    char hostname[HOST_NAME_MAX];
    int sockfd, connfd, port = atoi(argv[1]), run = 1; // i = 0; [BACKLOG]
    struct sockaddr_in server, client;
    pid_t pid;
    if (start_server(&sockfd, port, server)) {
        perror("Failed to start server");
        exit(1);
    }
    // TODO Add server start success info message
    if (argc == 3) {
        strcpy(hostname, argv[2]);
    } else {
        gethostname(hostname, sizeof(hostname));
    }
    while (run) {
        char conn_ip[INET_ADDRSTRLEN];
        socklen_t len = sizeof(client);
        if ((connfd = accept(sockfd, (struct sockaddr *)&client, &len)) < 0
        || (pid = fork()) < 0) {
            perror("Failed to establish connection to client");
        } else if (!pid) { // child
            // int index = i++;
            inet_ntop(client.sin_family, &client.sin_addr.s_addr, conn_ip, sizeof(conn_ip));
            printf("Client connected from %s on pid:%d\n", conn_ip, getpid());
            write(connfd, hostname, sizeof(hostname));
            if (read_client(connfd)) {
                perror("Failed to read client");
                exit(1);
            } else {
                // for (int i = index; i < BACKLOG - 1; i++) {
                //     new_fd[i] = new_fd[i + 1];
                // }
                // new_fd[BACKLOG]
                printf("Client disconnected from %s\n", conn_ip);
            }
        } else { // parent
            // char buf[1256];
            // fgets(buf, sizeof(buf), stdin);
            // strtok(buf, "\n");
            // if (!strcmp(buf, "quit")) {
            //     for (int j = 0; j < i; j++) {
            //         // write(new_fd[j], );
            //     }
            //     run = 0;
            // }
        }
    }
    close(sockfd);
    close(connfd);
    return 0;
}
