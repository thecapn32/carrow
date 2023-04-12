#include "tcp.h"
#include "evloop.h"

#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


int
tcp_connect(struct tcpconn *conn, const char *hostname, const char*port) {
    int err;
    int ret;
    int optlen = 4;
    int cfd;
    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *try;

    /* After epoll(2) indicates writability, use getsockopt(2) to read the
       SO_ERROR option at level SOL_SOCKET to determine whether connect()
       completed successfully (SO_ERROR is zero) or unsuccessfully
       (SO_ERROR is one of the usual error codes listed here,
       explaining the reason for the failure).
    */
    if (conn->status == TCSCONNECTING) {
        ret = getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, &err, &optlen);
        if (ret || err) {
            goto failed;
        }

        goto success;
    }

    /* Resolve hostname */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;         /* Allow IPv4 only.*/
    // hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = AI_NUMERICSERV;
    hints.ai_protocol = 0;
    if (getaddrinfo(hostname, port, &hints, &result) != 0) {
        goto failed;
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address.
    */
    for (try = result; try != NULL; try = try->ai_next) {
        cfd = socket(try->ai_family, try->ai_socktype | SOCK_NONBLOCK, 
                try->ai_protocol);
        if (cfd < 0) {
            continue;
        }

        if (connect(cfd, try->ai_addr, try->ai_addrlen) == 0) {
            /* Connection success */
            break;
        }

        if (errno == EINPROGRESS) {
            /* Waiting to connect */
            break;
        }

        close(cfd);
        cfd = -1;
    }

    if (cfd < 0) {
        freeaddrinfo(result);
        return -1;
    }

    /* Address resolved, updating state. */
    memcpy(&(conn->remoteaddr), try->ai_addr, sizeof(struct sockaddr));
    conn->fd = cfd;
    freeaddrinfo(result);
    if (errno == EINPROGRESS) {
        /* Connection is in-progress, Waiting. */
        /* The socket is nonblocking and the connection cannot be
           completed immediately. It is possible to epoll(2) for
           completion by selecting the socket for writing.
        */
    }

success:
    /* Hooray, Connected! */
    conn->status = TCSCONNECTED;
    errno = 0;
    socklen_t socksize = sizeof(conn->localaddr);
    if (getsockname(conn->fd, &(conn->localaddr), &socksize)) {
        goto failed;
    }

    return 0;

wait:
    conn->status = TCSCONNECTING;
    return 0;

failed:
    conn->status = TCSFAILED;
    close(conn->fd);
    return -1;
}
