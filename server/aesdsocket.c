#define _GNU_SOURCE
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
#include <pthread.h>
#include <time.h>
#include "queue.h"

#define BUF_SIZE 512
#define SOCKET_DATA_FILE "/var/tmp/aesdsocketdata"

struct thread_arg_t {
    int connfd;
};

struct thread_node_t {
    pthread_t thread;
    SLIST_ENTRY(thread_node_t) entry;
};

SLIST_HEAD(thread_head_t, thread_node_t);

int is_shutdown = false;
pthread_mutex_t mutex;

void sig_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        is_shutdown = true;
    }
}

void timer_hander(union sigval v) {
    char buf[128];
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    if (lt == NULL) {
        perror("localtime");
        exit(-1);
    }
    if (strftime(buf, sizeof(buf), "%a, %d %b %Y %T %z", lt) == 0) {
        fprintf(stderr, "strftime return 0");
        exit(-1);
    }

    int fd = open(SOCKET_DATA_FILE, O_RDWR | O_APPEND | O_CREAT, 0644);
    if (fd == -1) {
        perror("open");
        exit(-1);
    }
    if (pthread_mutex_lock(&mutex)) {
        fprintf(stderr, "pthread_mutex_lock error\n");
        exit(-1);
    }
    char out[256];
    sprintf(out, "timestamp: %s\n", buf);
    size_t len = strlen(out);
    if (write(fd, out, len) < len) {
        fprintf(stderr, "write return less");
        exit(-1);
    }
    if (pthread_mutex_unlock(&mutex)) {
        fprintf(stderr, "pthread_mutex_unlock error\n");
        exit(-1);
    }
}

void *conn_thread(void *arg) {
    struct thread_arg_t *thread_arg = (struct thread_arg_t*)arg;
    int connfd = thread_arg->connfd;
    char buf[BUF_SIZE];

    int fd = open(SOCKET_DATA_FILE, O_RDWR | O_APPEND | O_CREAT, 0644);
    if (fd == -1) {
        perror("open");
        exit(-1);
    }

    ssize_t n;
    if (pthread_mutex_lock(&mutex)) {
        fprintf(stderr, "pthread_mutex_lock error\n");
        exit(-1);
    }
    while ((n = recv(connfd, buf, sizeof(buf)-1, 0)) > 0) {
        // buf[n] = '\0';
        // printf("receive: %s\n", buf);
        if (write(fd, buf, n) < n)
            exit(-1);
        if (buf[n-1] == '\n')
            break;
    }
    if (pthread_mutex_unlock(&mutex)) {
        fprintf(stderr, "pthread_mutex_unlock error\n");
        exit(-1);
    }
    if (n == -1) {
        perror("recv");
        pthread_exit(NULL);
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
    return NULL;
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

    // Timer
    timer_t timerid;
    struct sigevent sev;
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_value.sival_ptr = &timerid;
    sev.sigev_notify_function = timer_hander;
    sev.sigev_notify_attributes = NULL;
    if (timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
        perror("timer_create");
        exit(-1);
    }
    struct itimerspec its;
    its.it_value.tv_sec = 10;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;
    if (timer_settime(timerid, 0, &its, NULL) == -1) {
        perror("timer_settime");
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

    struct thread_head_t *thread_head = malloc(sizeof(struct thread_head_t));
    SLIST_INIT(thread_head);
    pthread_mutex_init(&mutex, 0);

    while (!is_shutdown) {
        struct sockaddr_in addr;
        socklen_t addrlen;
        int connfd = accept(sfd, (struct sockaddr*)&addr, &addrlen);
        if (connfd == -1) {
            perror("accept");
            continue;
        }
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(addr.sin_addr));

        struct thread_node_t *node = malloc(sizeof(struct thread_node_t));

        struct thread_arg_t *arg = malloc(sizeof(struct thread_arg_t));
        arg->connfd = connfd;
        if (pthread_create(&node->thread, NULL, conn_thread, arg)) {
            fprintf(stderr, "pthread_create error\n");
            exit(-1);
        }
        SLIST_INSERT_HEAD(thread_head, node, entry);

        struct thread_node_t *tmp;
        SLIST_FOREACH_SAFE (node, thread_head, entry, tmp) {
            if (pthread_tryjoin_np(node->thread, NULL)) {
                SLIST_REMOVE(thread_head, node, thread_node_t, entry);
                free(node);
            }
        }
    }

    syslog(LOG_INFO, "Caught signal, exiting");


    struct thread_node_t *node, *tmp;
    SLIST_FOREACH_SAFE (node, thread_head, entry, tmp) {
        pthread_join(node->thread, NULL);
        SLIST_REMOVE(thread_head, node, thread_node_t, entry);
        free(node);
    }
    free(thread_head);

    pthread_mutex_destroy(&mutex);

    timer_delete(timerid);

    if (access(SOCKET_DATA_FILE, F_OK) == 0)
        if (remove(SOCKET_DATA_FILE) == -1) {
            perror("remove");
            exit(-1);
        }
    close(sfd);
    closelog();

    return 0;
}
