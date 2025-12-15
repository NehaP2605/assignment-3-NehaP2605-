#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>

#define BUFF_SIZE 1024

volatile sig_atomic_t stop_requested = 0;
static int sock_fd = -1;
static int file_fd = -1;
const char* file_path = "/var/tmp/aesdsocketdata";
static char* recv_buff = NULL;
static int recv_len = 0;

void cleanup() {
    if(sock_fd != -1) {
        close(sock_fd);
        sock_fd = -1;
    }
    if(file_fd != -1) {
        close(file_fd);
        file_fd = -1;
    }
    //delete the file
    remove("/var/tmp/aesdsocketdata");
    //close syslog
    closelog();
}

void openAndBindSocket(int* sock_fd) {
    struct addrinfo hints, *res, *resptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE; // for bind
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if(getaddrinfo(NULL, "9000", &hints, &res) != 0) {
        syslog(LOG_ERR,"getaddrinfo error : %s", strerror(errno));
        return;
    }
    for(resptr = res; resptr != NULL; resptr = resptr->ai_next) {
        *sock_fd = socket(resptr->ai_family,
                resptr->ai_socktype,
                resptr->ai_protocol);
        if(*sock_fd == -1) continue;
        if(bind(*sock_fd, resptr->ai_addr, resptr->ai_addrlen) == 0) {
            //bind successfull
            break;
        }
        close(*sock_fd);
    }
    if (resptr == NULL) {
        //bind failed
        syslog(LOG_ERR,"bind error : %s\n", strerror(errno));
        return;
    }
    freeaddrinfo(res); // free the linked list
}

void handle_signal(int signo) {
    if(signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_ERR, "Caught signal,exiting");
        stop_requested = 1;
        shutdown(sock_fd, SHUT_RDWR);
    }
}

int sendDataToClient(int* client_fd) {
    file_fd = open(file_path,O_RDONLY);
    if (file_fd < 0 ) {
        syslog(LOG_ERR, "Error opening file /var/tmp/aesdsocketdata : %s", strerror(errno));
        return -1;
    }
    char buff[BUFF_SIZE];
    ssize_t bytes_read = 0;
    while( (bytes_read = read(file_fd, buff, BUFF_SIZE)) > 0 ) {
        int offset = 0;
        while(offset < bytes_read) {
            int bytes_sent = send(*client_fd, buff+offset, bytes_read-offset, 0);
            if(bytes_sent < 0) {
                syslog(LOG_ERR, "Error while sending the data to client %s", strerror(errno));
                return -1;
            } else {
                syslog(LOG_DEBUG, " bytes_sent : %d",bytes_sent);
                /*printf("bytes sent : %d\n", bytes_sent);
                write(STDOUT_FILENO, buff + offset, bytes_sent);
                write(STDOUT_FILENO, "\n", 1);*/
            }
            offset += bytes_sent;
        }
    }
    close(file_fd);
    return (bytes_read < 0) ? -1 : 0; 
}

int receiveData(char** buf, int* client_fd) {
    char temp_buff[BUFF_SIZE];
    ssize_t bytes = recv(*client_fd, temp_buff, sizeof(temp_buff), 0);
    if(bytes < 0) {
        syslog(LOG_ERR, " recv failed : %s", strerror(errno));
        return -1;
    }

    if(bytes == 0) {
        free(recv_buff);
        recv_buff = NULL;
        recv_len = 0;
        return 0; //client disconnected
    }

    // append the new buffer to the persistent buffer
    char* new_buff = realloc(recv_buff, recv_len+bytes);
    if (!new_buff) {
        syslog(LOG_ERR, "realloc error %s", strerror(errno));
        return -1;
    }
    recv_buff = new_buff;
    memcpy(recv_buff+recv_len, temp_buff, bytes);
    recv_len += bytes;

    // find newline in this buffer
    char *newline = memchr(recv_buff, '\n', recv_len);
    if(!newline) {
        syslog(LOG_INFO, "packet not complete yet\n");
        return 1;
    }

    size_t packet_len = newline - recv_buff + 1; // include '\n'
                                                 //allocate memory for output buffer
    *buf = malloc(packet_len);
    if(!*buf) return -1;
    memcpy(*buf, recv_buff, packet_len);
    // remove consumed data
    size_t remaining = recv_len - packet_len;
    memmove(recv_buff, recv_buff+packet_len, remaining);
    recv_len = remaining;
    //append packet data to file
    file_fd = open(file_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file_fd < 0 ) {
        syslog(LOG_ERR, "Error opening file /var/tmp/aesdsocketdata : %s", strerror(errno));
        return -1;
    }
    size_t written = 0;
    while(written < packet_len) {
        ssize_t bytes_written = write(file_fd,*buf+written, packet_len-written);
        syslog(LOG_DEBUG, " bytes_written : %ld packet_len : %ld written : %ld", bytes_written, packet_len, written);
        if (bytes_written < 0) {
            syslog(LOG_ERR, "Error writing to file /var/tmp/aesdsocketdata : %s", strerror(errno));
            return -1;
        }
        written += bytes_written;
    }
    close(file_fd);
    // send data back to the client
   if(sendDataToClient(client_fd) < 0) {
        syslog(LOG_ERR, "error sending data to client\n");
        return -1;
    }

    return packet_len;
}

int main(int args, char* argv[]) {
    int daemon_mode = 0;
    if(args == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    }
    /* open syslog connection */
    openlog("aesdsocket_log", LOG_PID, LOG_USER);

    // setup signal handlers
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    //block no additional signals during handler
    sigemptyset(&sa.sa_mask);
    //restart interrupted syscalls if possible
   // sa.sa_flags = SA_RESTART;
    if(sigaction(SIGINT, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Error registering signal SIGINT %s", strerror(errno));
        return -1;
    }

    if(sigaction(SIGTERM, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Error registering signal SIGTERM %s", strerror(errno));
        return -1;
    }


    //create socket
    openAndBindSocket(&sock_fd);
    if (sock_fd < 0) {
        syslog(LOG_ERR, "socket creation failed : %s\n", strerror(errno));
        return -1;
    }
    // run as daemon
    if (daemon_mode) {
        pid_t pid = fork();
        if(pid < 0) {
            syslog(LOG_ERR,"fork error : %s", strerror(errno));
            close(sock_fd);
            exit(EXIT_FAILURE);
        }
        if (pid > 0) {
            // parent exits , child continues as daemon
            exit(EXIT_SUCCESS);
        }
        // child continues as daemon, detach from terminal
        if(setsid() < 0) {
            syslog(LOG_ERR,"setsid error : %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        // change working dir to root
        if (chdir("/") < 0) {
            syslog(LOG_ERR,"chdir error : %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

    }
    // start listening on sock_fd and accept any incoming connection
    if(listen(sock_fd, 5) < 0) {
        syslog(LOG_ERR, " Error while trying to listen : %s\n", strerror(errno));
        close(sock_fd);
        sock_fd = -1;
        exit(EXIT_FAILURE);
    }
    int client_fd = -1;
    // start accepting connections
    while(!stop_requested) {
        struct sockaddr sock_addr;
        socklen_t sock_len = sizeof(sock_addr);
        client_fd = accept(sock_fd, &sock_addr, &sock_len);
        if(client_fd < 0) {
            if (stop_requested) {
                break;
            }
            syslog(LOG_ERR,"accept error : %s", strerror(errno));
            cleanup();
            exit(EXIT_FAILURE);
        }
        syslog(LOG_INFO,"listenAndAccept successfull\n");
        char host[NI_MAXHOST], serv[NI_MAXSERV];

        getnameinfo(&sock_addr, sock_len,
                host, sizeof(host),
                serv, sizeof(serv),
                NI_NUMERICHOST | NI_NUMERICSERV);

        syslog(LOG_INFO, "Accepted connection from %s:%s\n", host, serv);
        //receive data
        char* packet = NULL;
        int len;
        while(1) {
            len = receiveData(&packet, &client_fd);
            if(len < 0) {
                //error occured
                cleanup();
                free(packet);
                close(client_fd);
                exit(EXIT_FAILURE);
            }else if(len == 0) {
                //client disconnected
                syslog(LOG_INFO, "client disconnected\n");
                break;
            }
        }
        close(client_fd);
        free(packet);
        syslog(LOG_INFO, "End---->Closed connection from %s:%s\n", host, serv);

    }
    if (stop_requested) {
        shutdown(client_fd, SHUT_RDWR);
    }
    //clean up before returning
    close(client_fd);
    cleanup();
    return 0;
}
