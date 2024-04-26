/// This is an intermediate interface for socket communication. It uses the
/// socket interface under the hood and provides a message communication API
/// according to its protocol and usecase.

#pragma once

#include "socket.h"
#include <poll.h>
#include <stddef.h>
#include <string.h>

/// Message kind.
typedef enum {
    /// Terminates the connection.
    mk_exit = 0,
    /// Asks (server) or enters (client) the password.
    mk_enter_passwd,
    /// Says the password is wrong.
    mk_wrong_passwd,
    /// Assigns a user id.
    mk_assign_uid,
    /// Shows available opponents.
    mk_show_opponents,
    /// Accepts (server) or selects (client) an opponent.
    mk_select_opponent,
    /// Asks the opponent to be a guesser.
    mk_ask_opponent,
    /// Guesses.
    mk_guess,
    /// Shows that the guess is wrong.
    mk_wrong_guess,
    /// Sends a hint to the guesser.
    mk_hint,
    /// Announces that the guess is correct.
    mk_correct_guess,
} pl_message_kind;

#ifndef PL_RAW_BYTES_SIZE
/// This is a buffer size used for network communication. The default value is
/// 1024.
#define PL_RAW_BYTES_SIZE 1024
#endif

/// Message buffer memory layout:
/// [<12-byte information><`PL_RAW_BYTES_SIZE` raw bytes + 4 bytes of padding>]
typedef struct {
    /// Assigned id. Zero means no id is assigned yet.
    size_t id;
    /// The length of bytes stored in `raw_bytes`.
    size_t raw_bytes_len;
    /// Message kind.
    pl_message_kind kind;
    /// Raw bytes. Size + 4 bytes to fill the padding.
    char raw_bytes[PL_RAW_BYTES_SIZE + 4];
} pl_message;

/// A global buffer used for reading and writing messages.
static pl_message pl_msg_buf;

/// No timeout.
#define PL_NO_TIMEOUT -1
/// The default value for network communication timeout which is 5000.
#define PL_DEFAULT_TIMEOUT 5000

/// @brief Writes the message to the socket according to the protocol.
/// Note that you need to pass the raw bytes through an argument
/// and no need to fill the `raw_bytes` field in `pl_message` manually.
/// @param fd Socket's file descripter.
/// @param bytes Message raw bytes.
/// @param m A message struct.
/// @return The `st_write` result.
static ssize_t pl_msg_write(int fd, const char *bytes, pl_message m) {
    bzero(&pl_msg_buf, sizeof(pl_message));

    // Fills the `pl_msg_buf`.
    pl_msg_buf.id = m.id;
    pl_msg_buf.kind = m.kind;
    pl_msg_buf.raw_bytes_len = m.raw_bytes_len;

    // Copy the bytes if we got any.
    if (pl_msg_buf.raw_bytes_len > 0)
        memcpy(&pl_msg_buf.raw_bytes, bytes, m.raw_bytes_len);

    return st_write(fd, &pl_msg_buf, sizeof(pl_message));
}

/// @brief Writes the message to the socket according to the protocol and waits
/// until the socket is ready to write if it's not. Note that you need to pass
/// the raw bytes through an argument and no need to fill the `raw_bytes` field
/// in `pl_message` manually.
/// @param fd Socket's file descripter.
/// @param bytes Message raw bytes.
/// @param m A message struct.
/// @return The `st_write` result.
static ssize_t pl_poll_msg_write(int fd, const char *bytes, pl_message m,
                                 int timeout) {
    if (st_single_poll(fd, pk_write, timeout) <= 0) return -1;
    return pl_msg_write(fd, bytes, m);
}

/// @brief Reads the message to socket buffer according to the protocol.
/// Note that you need to pass the raw bytes through an argument
/// and no need to fill the `raw_bytes` field in `pl_message` manually.
/// @param fd File descripter.
/// @return A pointer to the corresponding `pl_message`.
static pl_message *pl_msg_read(int fd) {
    // Since we work with null-terminated strings we're making sure the buffer
    // is zeroed out. Yes, there are other ways with better performance but we
    // stick to simplicity here.
    bzero(&pl_msg_buf, sizeof(pl_message));

    if (st_read(fd, &pl_msg_buf, sizeof(pl_message)) < 0) return NULL;

    return (pl_message *)&pl_msg_buf;
}

/// @brief Reads the message to socket buffer according to the protocol and
/// waits until the socket is ready to read if it's not. Note that you need to
/// pass the raw bytes through an argument and no need to fill the `raw_bytes`
/// field in `pl_message` manually.
/// @param fd File descripter.
/// @return A pointer to the corresponding `pl_message`.
static pl_message *pl_poll_msg_read(int fd, int timeout) {
    if (st_single_poll(fd, pk_read, timeout) <= 0) return NULL;
    return pl_msg_read(fd);
}
