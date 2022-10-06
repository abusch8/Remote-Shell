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

#define BACKLOG 10
#define SIG "\0"

extern int errno;

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Error: Invalid argument length\n"
                        "Usage: %s port hostname\n", argv[0]);
        exit(1);
    }

    // time_t raw_time;
    // struct tm *time_info;

    int sockfd, new_fd, port = atoi(argv[1]), run = 1;
    struct sockaddr_in server, client;
    socklen_t len;
    printf("Starting server...\n");
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Failed to create socket");
        exit(1);
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Failed to bind socket");
        exit(1);
    }
    if (listen(sockfd, BACKLOG) < 0) {
        perror("Failed to mark socket as passive");
        exit(1);
    }
    char hostname[HOST_NAME_MAX];
    argc == 3 ? atoi(strcpy(hostname, argv[2])) : gethostname(hostname, sizeof(hostname));
    while (run) {
        len = sizeof(client);
        if ((new_fd = accept(sockfd, (struct sockaddr *)&client, &len)) < 0) {
            perror("Failed to accept client");
            continue;
        }
        pid_t pid;
        if ((pid = fork()) < 0) {
            perror("Failed to create fork");
        } else if (!pid) { /* child */
            printf("connected\n"); // asctime(time_info)
            write(new_fd, hostname, sizeof(hostname));
            char buf[1256];
            while (run) {
                char *token, *rest = buf, *args[10];
                int ret, i = 0, fd[2];
                // time(&raw_time);
                // time_info = localtime(&raw_time);
                if ((ret = read(new_fd, buf, sizeof(buf))) < 0) {
                    perror("Failed to read client");
                    exit(1);
                } else if (!ret) {
                    printf("disconnected\n");
                    exit(0);
                }
                while ((token = strtok_r(rest, " ", &rest))) { /* tokenizer */
                    args[i] = (char *)malloc(sizeof(char) * (strlen(token) + 1));
                    strcpy(args[i++], token);
                }
                args[i] = NULL;
                memset(&buf, 0, sizeof(buf));
                pipe(fd);
                if (!strcmp(args[0], "cd")) {
                    if (args[2] != NULL) {
                        char error[] = "Too many arguments\n";
                        // printf("%s\n", error);
                        write(new_fd, error, sizeof(error));
                    } else if (args[1] != NULL && chdir(args[1]) < 0) {
                        // char error[strlen(strerror(errno)) + 1];
                        // snprintf(error, sizeof(error), "%s\n", strerror(errno));
                        // snprintf(error, strlen(strerror(errno)) + 1, "%s\n", strerror(errno));
                        char *error = strcpy(error, strerror(errno));
                        error = strcat(error, "\n");
                        write(new_fd, error, sizeof(error));
                    }
                    write(new_fd, SIG, sizeof(SIG));
                    continue;
                }
                switch (pid = fork()) {
                    case -1 :
                        perror("Failed to create fork");
                        break;
                    case  0 : /* child */
                        close(fd[0]);
                        dup2(fd[1], 1); // pipe stdout
                        dup2(fd[1], 2); // pipe stderr
                        close(fd[1]);
                        execvp(args[0], args);
                        perror("Failed execute command");
                        exit(1);
                    default : /* parent */
                        wait(NULL);
                        close(fd[1]);
                        while ((ret = read(fd[0], buf, sizeof(buf))) != 0) {
                            if (ret < 0) {
                                perror("Failed to read pipe");
                                exit(1);
                            }
                            if (write(new_fd, buf, sizeof(buf)) < 0) {
                                perror("Failed to write to client");
                                exit(1);
                            }
                            memset(&buf, 0, sizeof(buf));
                        }
                        write(new_fd, SIG, sizeof(SIG));
                        close(fd[0]);
                }
            }
        }
    }
    close(sockfd);
    close(new_fd);
    return 0;
}
