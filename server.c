#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define LISTEN_BACKLOG  50
#define LISTEN_PORT     10001
#define BUF_SIZE        128
#define BUF_MAX         (1024 * 1024)
#define REAP_INTERVAL   10

typedef struct message_t {
    struct message_t *prev, *next;
    char *str;
    size_t len;
} message_t;

static pthread_mutex_t message_mutex = PTHREAD_MUTEX_INITIALIZER;

message_t *first_message = NULL;
message_t *last_message = NULL;

typedef struct client_t {
    struct client_t *prev, *next;
    int sockfd;
    struct sockaddr *addr;
    socklen_t *socklen;
    pthread_t tid;
    message_t *last_message;
    int is_sender;
} client_t;

static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

client_t *first_client = NULL;
client_t *last_client = NULL;

void create_message(char *str, size_t len) {
    message_t *message = malloc(sizeof(message_t));

    if(message == NULL) {
        perror("calloc(message_t)");
        return;
    }

    message->len = len;
    message->str = calloc(1, len + 1);

    memcpy(message->str, str, len);

    if(pthread_mutex_lock(&message_mutex) != 0) {
        perror("pthread_mutex_lock(client_mutex)");
        exit(EXIT_FAILURE);
    }

    message->prev = last_message;
    message->next = NULL;

    if(first_message == NULL)
        first_message = message;

    if(last_message)
        last_message->next = message;

    last_message = message;

    if(pthread_mutex_unlock(&message_mutex) != 0) {
        perror("pthread_mutex_unlock(client_mutex)");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "created message \"%s\"\n", message->str);
}

void message_list_del(message_t *message) {
    if(pthread_mutex_lock(&message_mutex) != 0) {
        perror("pthread_mutex_lock(client_mutex)");
        exit(EXIT_FAILURE);
    }

    if(message->prev)
        message->prev->next = message->next;

    if(message->next)
        message->next->prev = message->prev;

    if(message == last_message) {
        if(message->next) {
            last_message = message->next;
        } else {
            last_message = message->prev;
        }
    }

    if(message == first_message) {
        if(message->prev) {
            first_message = message->prev;
        } else {
            first_message = message->next;
        }
    }

    if(pthread_mutex_unlock(&message_mutex) != 0) {
        perror("pthread_mutex_unlock(client_mutex)");
        exit(EXIT_FAILURE);
    }
}

void kill_message(message_t *message) {
    if(message == NULL)
        return;

    fprintf(stderr, "kill message @%p\n", (void *)message);

    message_list_del(message);

    if(message->str)
        free(message->str);

    free(message);
}

void reap_messages(void) {
    message_t *message = first_message, *next;

    while(message) {
        client_t *client = first_client;

        while(client) {
            if(message == client->last_message)
                return;

            client = client->next;
        }

        next = message->next;

        kill_message(message);

        message = next;
    }
}

void client_list_add(client_t *client) {
    if(pthread_mutex_lock(&client_mutex) != 0) {
        perror("pthread_mutex_lock(client_mutex)");
        exit(EXIT_FAILURE);
    }

    if(first_client == NULL)
        first_client = client;

    if(last_client) {
        last_client->next = client;
        client->prev = last_client;
    }

    last_client = client;

    if(pthread_mutex_unlock(&client_mutex) != 0) {
        perror("pthread_mutex_unlock(client_mutex)");
        exit(EXIT_FAILURE);
    }
}

void client_list_del(client_t *client) {
    if(pthread_mutex_lock(&client_mutex) != 0) {
        perror("pthread_mutex_lock(client_mutex)");
        exit(EXIT_FAILURE);
    }

    if(client->prev)
        client->prev->next = client->next;

    if(client->next)
        client->next->prev = client->prev;

    if(client == last_client) {
        if(client->next) {
            last_client = client->next;
        } else {
            last_client = client->prev;
        }
    }

    if(client == first_client) {
        if(client->prev) {
            first_client = client->prev;
        } else {
            first_client = client->next;
        }
    }

    if(pthread_mutex_unlock(&client_mutex) != 0) {
        perror("pthread_mutex_unlock(client_mutex)");
        exit(EXIT_FAILURE);
    }
}

void kill_client(client_t *client) {
    if(client == NULL)
        return;

    client_list_del(client);

    if(client->sockfd)
        close(client->sockfd);

    if(client->addr)
        free(client->addr);

    free(client);
}

int sock_write(int sockfd, char *buf, size_t len) {
    size_t write_sz, buf_idx = 0;

    while((write_sz = send(sockfd, buf + buf_idx, len - buf_idx, 0)) > 0) {
        buf_idx += write_sz;
    }

    return 0;
}

void send_pending_messages(client_t *client) {
    message_t *message = first_message;

    if(client->last_message) {
        if(client->last_message->next == NULL)
            return;

        message = client->last_message->next;
    }

    while(message) {
        fprintf(stderr, "sending message %p to client\n", (void *)message);

        sock_write(client->sockfd, message->str, message->len);

        client->last_message = message;
        message = message->next;
    }
}

void *handle(void *arg) {
    client_t *client = arg;
    ssize_t read_sz, buf_idx = 0, buf_sz = BUF_SIZE;
    char *buf = malloc(buf_sz);

    if(buf == NULL) {
        perror("malloc()");
        kill_client(client);
        return((void *) 0);
    }

    while((read_sz = recv(client->sockfd, buf + buf_idx, buf_sz - buf_idx, 0)) > 0) {
        /*fprintf(stderr, "received %lu+%lu bytes from client\n", read_sz, buf_idx);*/

        buf_idx += read_sz;

        /* is buffer full */
        if(buf_idx == buf_sz) {
            buf_sz *= 2;

            if(buf_sz > BUF_MAX) {
                fprintf(stderr, "buffer overflow, discarding message and disconnecting client\n");

                free(buf);
                kill_client(client);
                return((void *) 0);
            }

            /*fprintf(stderr, "increasing message buffer size to %lu\n", buf_sz);*/
            buf = realloc(buf, buf_sz);

            if(buf == NULL) {
                perror("realloc()");
                kill_client(client);
                return((void *) 0);
            }

            continue;
        }

        /* message is complete */
        if(buf[0] == '!') {
            if(client->is_sender) {
                create_message(buf + 1, buf_idx - 1);
            } else {
                fprintf(stderr, "client is not allowed to send messages!\n");
            }
        } else if(strncmp(buf, "ping\n", 5) == 0) {
            /* ping! */
        } else {
            fprintf(stderr, "Unknown message: %s\n", buf);
        }

        buf_idx = 0;

        send_pending_messages(client);
    }

    free(buf);

    fprintf(stderr, "client disconnected!\n");

    if(read_sz == -1) {
        perror("read()");
    }

    kill_client(client);

    return((void *) 0);
}

void create_client(int sockfd, struct sockaddr_in *addr, socklen_t *addrlen) {
    char addrstr[INET6_ADDRSTRLEN];
    client_t *client = calloc(1, sizeof(client_t));
    if(client == NULL) {
        perror("calloc()");
        close(sockfd);
        return;
    }

    if(inet_ntop(AF_INET, &(addr->sin_addr), addrstr, INET6_ADDRSTRLEN) == NULL) {
        perror("inet_ntop()");
        kill_client(client);
        return;
    }

    fprintf(stderr, "client connected from %s:%d\n", addrstr, ntohs(addr->sin_port));

    client->sockfd = sockfd;
    client->socklen = addrlen;

    if((client->addr = calloc(1, *client->socklen)) == NULL) {
        perror("calloc()");
        kill_client(client);
        return;
    }

    memcpy(client->addr, addr, *addrlen);

    client->is_sender = 1;
    client->last_message = first_message;

    client_list_add(client);

    if(pthread_create(&client->tid , NULL, &handle, client) != 0) {
        perror("pthread_create()");
        kill_client(client);
        return;
    }
}

void serve(int serverfd) {
    int maxfd = 0;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    struct timeval timeout;

    memset(&addr, 0, sizeof(struct sockaddr_in));

    while(1) {
        fd_set fds;

        FD_ZERO(&fds);
        FD_SET(serverfd, &fds);

        if(serverfd >= maxfd)
            maxfd = serverfd + 1;

        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        /* listen for new clients */
        if(select(maxfd, &fds, NULL, NULL, &timeout) > 0 && FD_ISSET(serverfd, &fds)) {
            int clientfd = accept(serverfd, (struct sockaddr *) &addr, &addrlen);
            if(clientfd == -1) {
                perror("accept()");
                continue;
            }

            create_client(clientfd, &addr, &addrlen);
        }

        reap_messages();
    }
}

int main() {
    int sockfd, reuse = 1;
    struct sockaddr_in addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) {
        perror("socket()");
        exit(EXIT_FAILURE);
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));

    memset(&addr, 0, sizeof(struct sockaddr_in));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(LISTEN_PORT);

    if(bind(sockfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) == -1) {
        perror("bind()");
        exit(EXIT_FAILURE);
    }

    if(listen(sockfd, LISTEN_BACKLOG) == -1) {
        perror("listen()");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr, "listening on port %d\n", LISTEN_PORT);

    serve(sockfd);

    close(sockfd);

    exit(EXIT_SUCCESS);
}
