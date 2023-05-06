#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>


int main(int argc, char* argv[]) {
    openlog("writer", LOG_CONS, LOG_USER);
    
    if (argc < 3) {
        syslog(LOG_ERR, "Usage: %s file string\n", argv[0]);
        exit(1);
    }

    FILE *fp = fopen(argv[1], "w+");
    if (!fp) {
        perror("fopen()");
        syslog(LOG_ERR, "fopen() error: %s", strerror(errno));
        exit(1);
    }
    size_t len = strlen(argv[2]);
    size_t num = fwrite(argv[2], sizeof(char), len, fp);
    if (num < len) {
        perror("fwrite()");
        syslog(LOG_ERR, "fwrite() error: %s", strerror(errno));
        exit(1);
    }

    syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);

    return 0;
}
