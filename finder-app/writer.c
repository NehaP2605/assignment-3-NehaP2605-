#include <fcntl.h>      /* open flags */
#include <unistd.h>     /* write, close */
#include <sys/stat.h>   /* mode constants */
#include <string.h>     /* strerror */
#include <errno.h>      /* errno */
#include <syslog.h>

int main(int argc, char* argv[]) {
    /* open syslog connection */
    openlog("writer_log", LOG_PID, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Pass filepath and string to be written");
        closelog();
        return 1;
    }

    const char* writepath = argv[1];
    const char* writestring = argv[2];

    /* open file, create if not present */
    int fd = open(writepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        syslog(LOG_ERR, "Error, unable to open/create file %s : %s", writepath, strerror(errno));
        closelog();
        return 1;
    }

    /* write to file */
    ssize_t bytes_written = write(fd, writestring, strlen(writestring));
    if (bytes_written < 0) {
        syslog(LOG_ERR, "Error writing to file %s : %s", writepath, strerror(errno));
        close(fd);
        closelog();
        return 1;
    }

    close(fd);
    closelog();
    return 0;
}

