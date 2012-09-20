#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define LISTEN_BACKLOG  50
#define LISTEN_PORT     10001
#define BUF_SIZE        128
#define BUF_MAX         (1024 * 1024)
#define REAP_INTERVAL   10
#define SEND_INTERVAL   2

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
    size_t mstr_idx;
    message_t *message, *last_message;
    int is_sender;
    ssize_t rbuf_idx, rbuf_sz;
    char *rbuf;
} client_t;

static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

client_t *first_client = NULL;
client_t *last_client = NULL;

size_t fd_client_index_limit = 0;
client_t **fd_client_index = NULL;

pthread_t reader_tid;
pthread_t writer_tid;

message_t *create_message(char *str, size_t len) {
    message_t *message = malloc(sizeof(message_t));

    if(message == NULL) {
        perror("calloc(message_t)");
        return NULL;
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

    return message;
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
            if(message == client->message || message == client->last_message)
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

    if(client->sockfd) {
        fd_client_index[client->sockfd] = NULL;
        close(client->sockfd);
    }

    if(client->addr)
        free(client->addr);

    free(client);
}

int send_pending_messages(client_t *client) {
    message_t *message = client->message;
    int count = 0;

    /*
    fprintf(stderr, "checking for pending message to client %p\n", (void *) client);
    */

    if(message == NULL && client->last_message) {
        if(client->last_message->next == NULL)
            return count;

        message = client->last_message->next;
        client->mstr_idx = 0;
    }

    if(message == NULL) {
        message = first_message;
        client->mstr_idx = 0;
    }

    while(message) {
        size_t write_sz = 0;

        fprintf(stderr, "sending message %p to client\n", (void *)message);

        client->message = message;

        while(client->mstr_idx < message->len) {
            write_sz = send(client->sockfd, message->str + client->mstr_idx, message->len - client->mstr_idx, MSG_DONTWAIT | MSG_NOSIGNAL);

            if(write_sz == -1)
                break;

            fprintf(stderr, "wrote %lu+%lu/%lu bytes of message %p to client %p\n", client->mstr_idx, write_sz, message->len, (void *) message, (void *) client);

            client->mstr_idx += write_sz;

            if(client->mstr_idx >= message->len)
                break;
        }

        if(write_sz == -1 && errno == EAGAIN)
            break;

        if(write_sz == -1) {
            perror("send()");
            kill_client(client);
            pthread_exit((void *) 0);
            return count;
        }

        client->last_message = message;
        client->message = message = message->next;
        client->mstr_idx = 0;

        count++;
    }

    return count;
}

void client_read(client_t *client) {
    ssize_t read_sz;

    if(client->rbuf == NULL) {
        client->rbuf_idx = 0;
        client->rbuf_sz = BUF_SIZE;

        client->rbuf = malloc(client->rbuf_sz);
        if(client->rbuf == NULL) {
            perror("malloc()");
            kill_client(client);
            pthread_exit((void *) 0);
        }
    }

    while((read_sz = recv(client->sockfd, client->rbuf + client->rbuf_idx, client->rbuf_sz - client->rbuf_idx, MSG_DONTWAIT | MSG_NOSIGNAL)) > 0) {
        fprintf(stderr, "received %lu+%lu bytes from client %p\n", read_sz, client->rbuf_idx, (void *) client);

        client->rbuf_idx += read_sz;

        /* is buffer full */
        if(client->rbuf_idx == client->rbuf_sz) {
            client->rbuf_sz *= 2;

            if(client->rbuf_sz > BUF_MAX) {
                fprintf(stderr, "buffer overflow, discarding message\n");

                free(client->rbuf);
                return;
            }

            /*fprintf(stderr, "increasing message buffer size to %lu\n", buf_sz);*/
            client->rbuf = realloc(client->rbuf, client->rbuf_sz);

            if(client->rbuf == NULL) {
                perror("realloc()");
                return;
            }

            continue;
        }

        /* message is complete */
        if(client->is_sender) {
            message_t *message = create_message(client->rbuf, client->rbuf_idx);

            if(message)
                fprintf(stderr, "created new message %p (length: %lu) from client %p\n", (void *) message, message->len, (void *) client);
        } else {
            fprintf(stderr, "client is not allowed to send messages!\n");
        }

        client->rbuf_idx = 0;
    }

    if(read_sz == -1 && errno == EAGAIN)
        return;

    if(read_sz == -1) {
        perror("read()");
        kill_client(client);
        pthread_exit((void *) 0);
    }
}

void create_client(int sockfd, struct sockaddr_in *addr, socklen_t *addrlen) {
    char addrstr[INET6_ADDRSTRLEN];
    client_t *client;

    if(sockfd > fd_client_index_limit) {
        fprintf(stderr, "cannot create any more clients (sockfd > _SO_OPEN_MAX)!\n");
        return;
    }

    if(fd_client_index[sockfd]) {
        fprintf(stderr, "FD/client index inconsistent (already set)!\n");
        return;
    }

    client = calloc(1, sizeof(client_t));
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
    client->message = first_message;
    client->last_message = NULL;

    client_list_add(client);

    fd_client_index[sockfd] = client;
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

#undef min
#define min(x, y) ((x) < (y) ? (x) : (y))

#undef max
#define max(x, y) ((x) > (y) ? (x) : (y))

void *reader(void *arg) {
    struct timeval timeout;

    while(1) {
        fd_set fds;
        int r, nfds;
        size_t i;

        FD_ZERO(&fds);

        for(i = 0; i < fd_client_index_limit; i++) {
            if(fd_client_index[i]) {
                FD_SET(i, &fds);
                nfds = max(nfds, i);
            }
        }

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        r = select(nfds + 1, &fds, NULL, NULL, &timeout);

        if(r == -1 && errno == EINTR)
            continue;

        if(r == -1) {
            perror("select()");
            exit(EXIT_FAILURE);
        }

        for(i = 0; i < fd_client_index_limit; i++) {
            if(FD_ISSET(i, &fds)) {
                client_read(fd_client_index[i]);
            }
        }

        sleep(1);
    }
}

void create_reader(void) {
    fd_client_index_limit = min(sysconf(_SC_OPEN_MAX), FD_SETSIZE);

    fprintf(stderr, "Allocating %lu bytes for FD/client index!\n", fd_client_index_limit * sizeof(client_t *));

    fd_client_index = calloc(sysconf(_SC_OPEN_MAX), sizeof(client_t *));
    if(fd_client_index == NULL) {
        perror("calloc()");
        exit(EXIT_FAILURE);
    }

    if(pthread_create(&reader_tid, NULL, &reader, NULL) != 0) {
        perror("pthread_create()");
        fprintf(stderr, "could not create reader thread\n");
        exit(EXIT_FAILURE);
    }
}

void *writer(void *arg) {
    struct timeval timeout;

    while(1) {
        fd_set fds;
        int nfds, r;
        size_t i;
        int count = 0;

        FD_ZERO(&fds);

        for(i = 0; i < fd_client_index_limit; i++) {
            if(fd_client_index[i]) {
                FD_SET(i, &fds);
                nfds = max(nfds, i);
            }
        }

        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;

        r = select(nfds + 1, NULL, &fds, NULL, &timeout);

        if(r == -1 && errno == EINTR)
            continue;

        if(r == -1) {
            perror("select()");
            exit(EXIT_FAILURE);
        }

        for(i = 0; i < fd_client_index_limit; i++) {
            if(FD_ISSET(i, &fds))
                count += send_pending_messages(fd_client_index[i]);
        }

        if(count == 0)
            sleep(1);
    }
}

void create_writer(void) {
    if(pthread_create(&writer_tid, NULL, &writer, NULL) != 0) {
        perror("pthread_create()");
        fprintf(stderr, "could not create writer thread\n");
        exit(EXIT_FAILURE);
    }
}

int main() {
    int sockfd, reuse = 1;
    struct sockaddr_in addr;

    create_reader();
    create_writer();

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
