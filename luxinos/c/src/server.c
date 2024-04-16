#define _GNU_SOURCE

#include "cli.h"
#include "console.h"
#include "game.h"
#include "protocol.h"
#include "socket.h"
#include <signal.h>
#include <stddef.h>
#include <stdio.h>

#define MAX_CLIENTS 10
#define HOST INADDR_ANY

// Ensures the server descripter is in a global variable so
// it can be accessed by signal handlers for closing it.
/// Server's file descripter.
static int serverfd;

// Same thing for cli arguments.
/// Cli arguments of the application.
static cli_args args;

/// A user.
typedef struct {
    /// User id.
    size_t id;
    /// User's socket file descripter.
    int fd;
    /// Whether the user has finished its game and quitted.
    bool finished_game;
    /// Padding.
    char _padding[3];
} user;
/// Connected users.
typedef struct {
    user users[MAX_CLIENTS];
    size_t len;
} users;

/// Processed/processing games.
typedef struct {
    ge_game games[MAX_CLIENTS];
    size_t len;
} games;

/// Pollfd instances to wait on.
typedef struct {
    pollfd pfds[MAX_CLIENTS + 1];
    size_t len;
} pollfds;

/// Server games.
static games sr_games = (games){.len = 0};
/// Server users.
static users sr_users = (users){.len = 0};
/// Server poll fds.
static pollfds sr_pfds = (pollfds){.len = 0};

// Closes the connection and exits when the SIGINT signal is received.
static void __attribute((noreturn))
sigint_handler(int sig_num __attribute__((unused))) {
    for (size_t i = 0; i < sr_users.len; i++) {
        user u = sr_users.users[i];

        if (u.finished_game) continue;
        // Sends the exit message to each clients.
        pl_msg_write(u.fd, "exit",
                     (pl_message){
                         .id = u.id,
                         .kind = mk_exit,
                         .raw_bytes_len = 4,
                     });

        // Close the socket connection.
        close(u.fd);
    }

    // Closes the server socket.
    close(serverfd);

    if (args.unix_socket) {
        // Unlinks the socket domain path.
        unlink(args.unix_socket_file);
    }

    printf("\nConnection closed.\n");
    fflush(stdout);

    exit(EXIT_SUCCESS);
}

/// @brief Finds a game based on the id.
/// @param id User's id.
/// @return A pointer to the `game` struct.
static ge_game *find_game(size_t id) {
    for (size_t i = 0; i < sr_games.len; i++) {
        if (sr_games.games[i].finished) continue;

        if (sr_games.games[i].guesser == id ||
            sr_games.games[i].chooser == id) {
            return &sr_games.games[i];
        }
    }

    return NULL;
}

/// @brief Checks if a user is already in a game.
/// @param id User's id.
static bool is_in_game(size_t id) { return find_game(id) != NULL; }

/// @brief Checks if a user finished its game.
/// @param id User's id.
static bool is_finished(size_t id) {
    // User ids start from 1 (id - 1).
    return sr_users.users[id - 1].finished_game;
}

/// @brief Announces the opponents to all clients.
static void announce_opponents(void) {
    size_t n_opps = 0;
    size_t opps[MAX_CLIENTS];
    bzero(&opps, sizeof(opps));

    // Finds available users.
    for (size_t i = 0; i < sr_users.len; i++) {
        size_t user_id = sr_users.users[i].id;
        // Should check if the opponent is not currently in a game.
        if (!is_in_game(user_id) && !is_finished(user_id)) {
            opps[n_opps] = user_id;
            n_opps += 1;
        }
    }

    // If this becomes true it means only the current user is ready to start a
    // game.
    if (n_opps <= 1) return;

    // Loops through those users and announce the opponents.
    for (size_t i = 0; i < n_opps; i++) {
        // User ids start from 1 (opps[i] - 1).
        pl_poll_msg_write(sr_users.users[opps[i] - 1].fd, (char *)opps,
                          (pl_message){
                              .id = opps[i],
                              .kind = mk_show_opponents,
                              .raw_bytes_len = sizeof(size_t) * n_opps,
                          },
                          PL_DEFAULT_TIMEOUT);
    }
}

/// @brief Regenerates the pollfds to handle new users.
/// @param limit_reached Whether the limitation for accepting new users has
/// reached.
static void poll_new_users(bool limit_reached) {
    bzero(sr_pfds.pfds, sizeof(sr_pfds.pfds));

    if (!limit_reached) {
        sr_pfds.len = 1;
        sr_pfds.pfds[0] = (pollfd){
            .revents = 0,
            .fd = serverfd,
            .events = POLLIN,
        };
    } else sr_pfds.len = 0;

    for (size_t i = 0; i < sr_users.len; i++) {
        if (sr_users.users[i].finished_game) continue;

        sr_pfds.pfds[sr_pfds.len] = (pollfd){
            .revents = 0,
            .events = POLLIN,
            .fd = sr_users.users[i].fd,
        };
        sr_pfds.len += 1;
    }
}

/// @brief Authenticates a new client.
/// @param fd File descripter.
static void authenticate(int fd, const char *pass) {
    size_t user_id;
    pl_message *read;

    // Asking the new clinet to enter the password.
    pl_poll_msg_write(fd, "",
                      (pl_message){
                          .id = 0,
                          .kind = mk_enter_passwd,
                      },
                      PL_DEFAULT_TIMEOUT);

    // Wait for it to answer.
    read = pl_poll_msg_read(fd, PL_DEFAULT_TIMEOUT);
    if (!read || read->raw_bytes_len != strlen(pass) ||
        strncmp(pass, read->raw_bytes, strlen(pass)) != 0) {
        // Terminate it if the password is wrong.
        pl_poll_msg_write(fd, "",
                          (pl_message){
                              .id = 0,
                              .kind = mk_wrong_passwd,
                          },
                          PL_DEFAULT_TIMEOUT);
        close(fd);
        return;
    }

    // New user
    user_id = sr_users.len + 1;
    sr_users.users[sr_users.len] = (user){
        .fd = fd,
        .id = user_id,
        .finished_game = false,
    };
    sr_users.len += 1;

    // Assign a user id to the client.
    pl_poll_msg_write(fd, "",
                      (pl_message){
                          .id = user_id,
                          .kind = mk_assign_uid,
                      },
                      PL_DEFAULT_TIMEOUT);

    printf("A new user (id = %lu) authenticated.\n", user_id);

    poll_new_users(false);

    announce_opponents();
}

/// @brief Sends a game message to both clients of a game.
/// @param game Game instance.
/// @param kind Message kind.
/// @param bytes Message bytes.
/// @param len Message bytes' length.
static void send_game_msg(const ge_game *game, pl_message_kind kind,
                          const char *bytes, size_t len) {
    // User ids start from 1 (game->X - 1).
    int guesser_fd = sr_users.users[game->guesser - 1].fd;
    int chooser_fd = sr_users.users[game->chooser - 1].fd;

    // Writing to both clients of a game.
    pl_poll_msg_write(guesser_fd, bytes,
                      (pl_message){
                          .id = game->guesser,
                          .kind = kind,
                          .raw_bytes_len = len,
                      },
                      PL_DEFAULT_TIMEOUT);
    pl_poll_msg_write(chooser_fd, bytes,
                      (pl_message){
                          .id = game->chooser,
                          .kind = kind,
                          .raw_bytes_len = len,
                      },
                      PL_DEFAULT_TIMEOUT);
}

int main(int argc, char *argv[]) {
    args = parse_cli_args(argc, argv);

    // Handles the SIGINT (i.e Crtl+C) signal.
    signal(SIGINT, sigint_handler);

    // Makes sure to zero out everything before start.
    bzero(sr_games.games, sizeof(sr_games.games));
    bzero(sr_users.users, sizeof(sr_users.users));

    serverfd =
        success_or_die(st_server_setup(HOST, args.port, args.unix_socket_file,
                                       MAX_CLIENTS, args.unix_socket),
                       "Failed to setup a server socket");

    poll_new_users(false);

    if (args.unix_socket) {
        printf("Listening on `%s` unix socket file...\n",
               args.unix_socket_file);
    } else printf("Listening on the %d port...\n", args.port);

    for (;;) {
        success_or_die(poll(sr_pfds.pfds, sr_pfds.len, PL_NO_TIMEOUT),
                       "Faild to poll");

        // Listening socket.
        if (sr_users.len < MAX_CLIENTS &&
            sr_pfds.pfds[0].revents == sr_pfds.pfds[0].events) {
            for (;;) {
                int fd;

                if (sr_users.len < MAX_CLIENTS) {
                    if ((fd = st_accept(sr_pfds.pfds[0].fd)) > 0) {
                        authenticate(fd, args.pass);
                    } else break;
                } else {
                    printf("Server cannot accept more users. Limit has been "
                           "reached!\n");

                    // Stop accepting new connections.
                    poll_new_users(true);
                    break;
                }
            }
        }

        // Starts from 1 since the first element is the listening socket.
        for (size_t i = 1; i < sr_pfds.len; i++) {
            pl_message *m;
            // File descripter of this pollfd.
            int fd = sr_pfds.pfds[i].fd;

            // This pollfd did not succeed.
            if (sr_pfds.pfds[i].revents != sr_pfds.pfds[i].events) continue;

            m = pl_msg_read(fd);
            if (m) switch (m->kind) {
                case mk_select_opponent: {
                    // Game instance without the secret guess word.
                    ge_game game_without_word;

                    // Stores the game.
                    ge_game *game = &sr_games.games[sr_games.len];
                    *game = *ge_game_from_msg(m);
                    // Assigns an ID to it.
                    game->id = sr_games.len;
                    // Increaments the length.
                    sr_games.len += 1;

                    // Unauthorized action.
                    if ((game->chooser != m->id && game->guesser != m->id) ||
                        game->finished)
                        continue;

                    // The exact data should be in `game_without_word` but
                    // without the secret word.
                    game_without_word = *game;
                    bzero(game_without_word.word,
                          sizeof(game_without_word.word));

                    // Announces that the selection succeeded.
                    send_game_msg(game, mk_select_opponent,
                                  (char *)&game_without_word, sizeof(ge_game));

                } break;

                case mk_guess: {
                    ge_game game = *ge_game_from_msg(m);
                    ge_game game_in_proc = sr_games.games[game.id];

                    if (strcmp(game.word, game_in_proc.word) != 0) {
                        send_game_msg(&game, mk_wrong_guess, (char *)&game,
                                      sizeof(ge_game));
                    } else {
                        send_game_msg(&game, mk_correct_guess, NULL, 0);
                    }
                } break;

                case mk_hint: {
                    ge_game game = *ge_game_from_msg(m);
                    // Sends the hint to the guesser.
                    // User ids start from 1 (game.guesser - 1).
                    pl_poll_msg_write(sr_users.users[game.guesser - 1].fd,
                                      (char *)&game,
                                      (pl_message){
                                          .id = game.guesser,
                                          .kind = mk_hint,
                                          .raw_bytes_len = sizeof(ge_game),
                                      },
                                      PL_DEFAULT_TIMEOUT);
                } break;

                case mk_exit: {
                    ge_game *user_game;

                    printf("User %zu quitted.\n", m->id);

                    // Close the socket connection.
                    // User ids start from 1 (m->id - 1).
                    close(sr_users.users[m->id - 1].fd);

                    // Keep track of deleted users.
                    // User ids start from 1 (m->id - 1).
                    sr_users.users[m->id - 1].finished_game = true;

                    // Set the game to finished if the user has played one.
                    if ((user_game = find_game(m->id))) {
                        user_game->finished = true;
                    }
                } break;

                default:
                    break;
                }
        }
    }
}
