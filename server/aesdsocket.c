#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUF_SIZE 512
#define SOCKET_DATA_FILE "/var/tmp/aesdsocketdata"

int is_shutdown = false;

void sig_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        is_shutdown = true;
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            exit(-1);
        } else if (pid > 0) {
            exit(0);
        }
    }

    int ret;
    int sfd;
    char buf[BUF_SIZE];

    openlog("aesdsocket", LOG_CONS, LOG_USER);
    
    struct sigaction sigact = { 0 };
    sigact.sa_handler = sig_handler;
    if (sigaction(SIGINT, &sigact, NULL) == -1) {
        perror("sigaction");
        exit(-1);
    }
    if (sigaction(SIGTERM, &sigact, NULL) == -1) {
        perror("sigaction");
        exit(-1);
    }

    struct addrinfo hints;
    struct addrinfo *ai_res, *rp;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    ret = getaddrinfo(NULL, "9000", &hints, &ai_res);
    if (ret) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        exit(-1);
    }

    for (rp = ai_res; rp != NULL; rp = rp->ai_next) {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;
        if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
            perror("setsockopt");
            continue;
        }
        if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        close(sfd);
    }
    if (rp == NULL) {
        fprintf(stderr, "Could not bind\n");
        exit(-1);
    }
    freeaddrinfo(ai_res);

    ret = listen(sfd, 10);
    if (ret == -1) {
        perror("listen");
        exit(-1);
    }


    while (!is_shutdown) {
        struct sockaddr_in addr;
        socklen_t addrlen;
        int connfd = accept(sfd, (struct sockaddr*)&addr, &addrlen);
        if (connfd == -1) {
            perror("accept");
            continue;
        }
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(addr.sin_addr));

        int fd = open(SOCKET_DATA_FILE, O_RDWR | O_APPEND | O_CREAT, 0644);
        if (fd == -1) {
            perror("open");
            exit(-1);
        }

        ssize_t n;
        while ((n = recv(connfd, buf, sizeof(buf)-1, 0)) > 0) {
            // buf[n] = '\0';
            // printf("receive: %s\n", buf);
            if (write(fd, buf, n) < n)
                exit(-1);
            if (buf[n-1] == '\n')
                break;
        }
        if (n == -1) {
            perror("recv");
            continue;
        }

        if (lseek(fd, SEEK_SET, 0) == -1) {
            perror("lseek");
            exit(-1);
        }
        while ((n = read(fd, buf, sizeof(buf)-1)) > 0) {
            if (send(connfd, buf, n, 0) < n) {
                fprintf(stderr, "send return < n\n");
                exit(-1);
            }
        }
        if (n == -1) {
            perror("read");
            exit(-1);
        }

        close(connfd);
        close(fd);
    }

    syslog(LOG_INFO, "Caught signal, exiting");

    if (access(SOCKET_DATA_FILE, F_OK) == 0)
        if (remove(SOCKET_DATA_FILE) == -1) {
            perror("remove");
            exit(-1);
        }
    close(sfd);
    closelog();

    return 0;
}
