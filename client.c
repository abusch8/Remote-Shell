#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <limits.h>

#define SIG_END  "\\1" // End of read signal
#define SIG_TERM "\\2" // Server termination signal

extern int errno;

// TODO
// int start_client() {

// }

// int write_server() {

// }

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Error: Invalid argument length\n"
                        "Usage: %s ip port\n", argv[0]);
        exit(1);
    }
    in_addr_t ip = strcmp(argv[1], "localhost") ? inet_addr(argv[1]) : inet_addr("127.0.0.1");
    int sockfd, port = atoi(argv[2]), run = 1, ret;
    struct sockaddr_in server;
    char buf[1256], hostname[HOST_NAME_MAX];
    printf("Starting client...\n");
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Failed to create socket");
        exit(1);
    }
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = ip;
    server.sin_port = htons(port);
    if (connect(sockfd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Failed to connect to server");
        exit(1);
    }
    if (read(sockfd, hostname, sizeof(hostname)) < 0) {
        perror("Failed to read server hostname");
        exit(1);
    }
    while (run) {
        memset(&buf, 0, sizeof(buf));
        printf("%s> ", hostname);
        fflush(stdout);
        fgets(buf, sizeof(buf), stdin);
        strtok(buf, "\n");
        if (strcmp(buf, "quit") == 0) {
            run = 0;
            break;
        }
        if (write(sockfd, buf, sizeof(buf)) < 0) {
            close(sockfd);
            perror("Failed to write to server");
            exit(1);
        }
        while ((ret = read(sockfd, buf, sizeof(buf))) != 0 && (strcmp(buf, SIG_END)) != 0) {
            if (ret < 0) {
                close(sockfd);
                perror("Failed to read server");
                exit(1);
            }
            if (!strcmp(buf, SIG_TERM)) {
                printf("Server shutdown\n");
                run = 0;
                break;
            }
            fflush(stdout);
            printf("%s", buf);
            memset(&buf, 0, sizeof(buf));
        }
    }
    close(sockfd);
    return 0;
}
