/// This is a socket interface for easier networking. This API is built with
/// non-blocking in mind. With that said, don't forget to handle `EGAIN` or
/// `EINPROGRESS`. You use poll of course.

#pragma once

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

// We have some typedefs for socket structs so we can use them like `MyStruct
// my_var;` instead of `struct MyStruct my_var;`.
typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;
typedef struct sockaddr_un sockaddr_un;
typedef struct timeval timeval;
typedef struct pollfd pollfd;

/// Returns `-1` from the corresponding function if `expr` fails. Note that the
/// `name` variable should already be defined at top level of the scope.
#define ret_on_err(name, expr)                                                 \
    name = expr;                                                               \
    if (name < 0) return -1

/// Poll kind.
typedef enum {
    pk_read,
    pk_write,
} st_poll_kind;

/// @brief Perfoms `poll` on a single file descripter for a single event.
/// @param fd File descripter.
/// @param kind Poll kind.
/// @param timeout Waiting timeout.
/// @return Poll result.
static int st_single_poll(int fd, st_poll_kind kind, int timeout) {
    return poll(
        &(pollfd){
            .fd = fd,
            .revents = 0,
            .events = kind == pk_read ? POLLIN : POLLOUT,
        },
        1, timeout);
}

/// @brief Writes the data to `sockaddr_un` and returns a `sockaddr`.
/// @param saddr A pointer to an allocated `sockaddr_un` struct.
/// @param unsock_path Path to the unix socket file.
/// @return `sockaddr_un` casted to `sockaddr`.
static sockaddr *st_unix_sockaddr(sockaddr_un *saddr, const char *unsock_path) {
    bzero(saddr, sizeof(sockaddr_un));

    saddr->sun_family = AF_UNIX;
    strcpy((char *)&saddr->sun_path, unsock_path);

    return (sockaddr *)saddr;
}

/// @brief Writes the data to `sockaddr_in` and returns a `sockaddr`.
/// @param saddr A pointer to an allocated `sockaddr_in` struct.
/// @param addr Listening host.
/// @param port Listening port.
/// @return `sockaddr_in` casted to `sockaddr`.
static sockaddr *st_tcp_sockaddr(sockaddr_in *saddr, in_addr_t addr,
                                 in_port_t port) {
    bzero(saddr, sizeof(sockaddr_in));

    // ntohl, ntohs, htonl, and htons are used to convert network data which
    // use big-endian to little-endian or vice versa. However, it's safe to
    // not use them on big-endian machines as they don't do anything.
    saddr->sin_family = AF_INET;
    saddr->sin_port = htons(port);
    if (addr != INADDR_ANY) saddr->sin_addr.s_addr = addr;
    else saddr->sin_addr.s_addr = htonl(INADDR_ANY);

    return (sockaddr *)saddr;
}

/// @brief Initiates a new socket.
/// @param unix_socket Pass `true` to use unix socket and pass `false` to use
/// tcp.
/// @return Socket's file descripter.
static int st_new(bool unix_socket) {
    int domain = unix_socket ? AF_UNIX : AF_INET;
    int fd, socket_flags, socket_set_flags, socket_set_opts;

    // Initiates a TCP socket scoket.
    ret_on_err(fd, socket(domain, SOCK_STREAM, 0));

    if (!unix_socket) {
        // Ensures the address becomes available immediately after closing the
        // socket.
        ret_on_err(socket_set_opts, setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                                               &(int){1}, sizeof(int)));
    }

    // Allows non-blocking socket communication.
    ret_on_err(socket_flags, fcntl(fd, F_GETFL, 0));
    ret_on_err(socket_set_flags, fcntl(fd, F_SETFL, socket_flags | O_NONBLOCK));

    return fd;
}

/// @brief Binds and listens on a socket.
/// @param saddr A pointer to the `sockaddr_in` struct.
/// @param fd Socket's file descripter
/// @param conn_queue_cap Connection queue capacity.
/// @return Listen's file descripter.
static int st_serve(int fd, sockaddr *saddr, socklen_t socklen,
                    int conn_queue_cap) {
    int bindres;

    // __CONST_SOCKADDR_ARG = const sockaddr *
    ret_on_err(bindres, bind(fd, saddr, socklen));

    // The second arguemnt of listen which is `__n` defines the maximum length
    // to which the queue of pending connections for socket_file_descriptor may
    // grow.
    return listen(fd, conn_queue_cap);
}

/// @brief Connects to the socket.
/// @param fd Socket's file descripter.
/// @param saddr A pointer to the `sockaddr_in` struct.
/// @return Server's file descripter.
static int st_connect(int fd, sockaddr *saddr, socklen_t slen) {
    return connect(fd, saddr, slen);
}

/// @brief Setups a server network socket.
/// @param addr TCP host which is ignored if `use_unix` is true.
/// @param port TCP port which is ignored if `use_unix` is true.
/// @param unsock_path Path to the unix socket file which is ignored if
/// `use_unix` is false.
/// @param max_conns Maximum connections.
/// @param use_unix Wheter to use unix domain socket instead of tcp.
/// @return Server's file descripter or `-1` in case of an error.
static int st_server_setup(in_addr_t addr, in_port_t port,
                           const char *unsock_path, int max_conns,
                           bool use_unix) {
    int fd, serv_res;
    sockaddr *serveraddr;
    socklen_t serveraddr_len;
    sockaddr_in tcp_serveraddr;
    sockaddr_un unix_serveraddr;

    serveraddr = use_unix ? st_unix_sockaddr(&unix_serveraddr, unsock_path)
                          : st_tcp_sockaddr(&tcp_serveraddr, addr, port);
    serveraddr_len = use_unix ? sizeof(sockaddr_un) : sizeof(sockaddr_in);

    ret_on_err(fd, st_new(use_unix));

    ret_on_err(serv_res, st_serve(fd, serveraddr, serveraddr_len, max_conns));

    return fd;
}

/// @brief Setups a client network socket.
/// @param addr TCP host which is ignored if `use_unix` is true.
/// @param port TCP port which is ignored if `use_unix` is true.
/// @param unsock_path Path to the unix socket file which is ignored if
/// `use_unix` is false.
/// @param use_unix Wheter to use unix domain socket instead of tcp.
/// @return Client's file descripter or `-1` in case of an error.
static int st_client_setup(in_addr_t addr, in_port_t port,
                           const char *unsock_path, bool use_unix) {
    int fd, poll_res, socket_error;
    sockaddr *serveraddr;
    socklen_t serveraddr_len;
    sockaddr_in tcp_serveraddr;
    sockaddr_un unix_serveraddr;

    serveraddr = use_unix ? st_unix_sockaddr(&unix_serveraddr, unsock_path)
                          : st_tcp_sockaddr(&tcp_serveraddr, addr, port);
    serveraddr_len = use_unix ? sizeof(sockaddr_un) : sizeof(sockaddr_in);

    ret_on_err(fd, st_new(use_unix));

    st_connect(fd, serveraddr, serveraddr_len);

    ret_on_err(poll_res, st_single_poll(fd, pk_write, -1));

    getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error,
               &(socklen_t){sizeof(int)});
    if (socket_error != 0) {
        errno = socket_error;
        return -1;
    }

    return fd;
}

/// @brief Accepts new connections.
/// @param fd Socket's file descripter.
/// @return Client's file descripter.
static int st_accept(int fd) { return accept(fd, NULL, NULL); }

/// @brief Reads from the socket.
/// @param fd Corresponding file descripter.
/// @param buf A buffer.
/// @param buflen Buffer's length.
/// @return Read's result.
static ssize_t st_read(int fd, void *buf, size_t buflen) {
    return read(fd, buf, buflen);
}

/// @brief Writes to the socket.
/// @param fd Corresponding file descripter.
/// @param buf A buffer.
/// @param buflen Buffer's length.
/// @return Write's result.
static ssize_t st_write(int fd, void *buf, size_t buflen) {
    return write(fd, buf, buflen);
}
