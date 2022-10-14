#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
// #include <netinet/in.h>
#include <arpa/inet.h>
// #include <netdb.h>

#define SIG_END  "\\1" // End of read signal
#define SIG_TERM "\\2" // Server termination signal

extern int errno;

int start_client(int *sockfd, in_addr_t ip, int port, struct sockaddr_in server, char *hostname) {
    printf("Starting client...\n");
    if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return 1;
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = ip;
    server.sin_port = htons(port);
    if (connect(*sockfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        return 1;
    }
    if (read(*sockfd, hostname, sizeof(hostname)) < 0) {
        return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Error: Invalid argument length\n"
                        "Usage: %s ip port\n", argv[0]);
        exit(1);
    }
    in_addr_t ip = strcmp(argv[1], "localhost") ? inet_addr(argv[1]) : inet_addr("127.0.0.1");
    int sockfd, port = atoi(argv[2]), run = 1, ret;
    struct sockaddr_in server;
    char buf[4096], *hostname = (char *)malloc(sizeof(char) * (HOST_NAME_MAX + 1));
    if (start_client(&sockfd, ip, port, server, hostname)) {
        perror("Failed to start client");
        exit(1);
    }
    while (run) {
        memset(&buf, 0, sizeof(buf));
        printf("%s> ", hostname);
        fgets(buf, sizeof(buf), stdin);
        strtok(buf, "\n");
        if (!strcmp(buf, "quit") || !strcmp(buf, "exit")) {
            run = 0;
            break;
        }
        if (write(sockfd, buf, sizeof(buf)) < 0) {
            close(sockfd);
            perror("Failed to write to server");
            exit(1);
        }
        while ((ret = read(sockfd, buf, sizeof(buf))) != 0 && strcmp(buf, SIG_END)) {
            if (ret < 0) {
                perror("Failed to read server");
                close(sockfd);
                exit(1);
            }
            if (!strcmp(buf, SIG_TERM)) {
                printf("Server shutdown\n");
                run = 0;
                break;
            }
            printf("%s", buf);
            memset(&buf, 0, sizeof(buf));
        }
    }
    close(sockfd);
    return 0;
}
