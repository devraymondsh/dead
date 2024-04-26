#define _GNU_SOURCE

#include "cli.h"
#include "console.h"
#include "game.h"
#include "protocol.h"
#include "socket.h"
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>

#define HOST inet_addr("127.0.0.1")

// Ensures the socket fd in a global variable so it can be accessed by signal
// handlers for closing it.
/// Server's network socket file descripter.
static int socketfd;

/// User ID assigned by the server.
static size_t uid = 0;

/// If the client is in a game.
static bool is_in_game = false;

/// Determines if the client is a guesser.
static bool is_guesser;

/// Details of the current game.
static ge_game current_game =
    (ge_game){.id = 0, .chooser = 0, .guesser = 0, .finished = false};

/// @brief Zeros out the `current_game.word`.
static void zero_current_game(void) {
    bzero(current_game.word, sizeof(current_game.word));
}

// Closes the connection and exits when the SIGINT signal is received.
static void __attribute((noreturn))
sigint_handler(int sig_num __attribute__((unused))) {
    // Informs the server that the client is quitting.
    pl_poll_msg_write(socketfd, "",
                      (pl_message){
                          .id = uid,
                          .kind = mk_exit,
                          .raw_bytes_len = 0,
                      },
                      PL_NO_TIMEOUT);

    // Closes the socket.
    close(socketfd);

    printf("\nConnection closed.\n");
    fflush(stdout);

    exit(EXIT_SUCCESS);
}

/// @brief Tries authenticating with credentials.
/// @param pass Password.
static void authenticate(const char *pass) {
    // Waits for the server to ask for credentials.
    pl_message *m = pl_poll_msg_read(socketfd, PL_NO_TIMEOUT);
    if (!m || m->kind != mk_enter_passwd) {
        die("Failed to read while authenticating with credentials!\n");
    }

    // Send the credentials to the server.
    success_or_die(
        (int)pl_poll_msg_write(socketfd, pass,
                               (pl_message){.id = uid,
                                            .kind = mk_enter_passwd,
                                            .raw_bytes_len = strlen(pass)},
                               PL_NO_TIMEOUT),
        "Failed to authenticate with credentials");

    // Asks the server for the authentication result.
    m = pl_poll_msg_read(socketfd, PL_NO_TIMEOUT);
    if (m->kind != mk_assign_uid) die("Unauthorized!\n");

    uid = m->id;
}

/// @brief Performs a poll againts stdin and the network socket.
/// @return File descripter of the finished one.
static int stdin_vs_socket_poll(void) {
    // Stdin vs Socket poll
    pollfd pollfds[2];

    bzero(&pollfds, sizeof(pollfds));

    // stdin
    pollfds[0].fd = STDIN_FILENO;
    pollfds[0].events = POLLIN;
    // socket
    pollfds[1].fd = socketfd;
    pollfds[1].events = POLLIN;

    success_or_die(poll(pollfds, 2, PL_NO_TIMEOUT), "Failed to poll");

    if (pollfds[1].events == pollfds[1].revents) {
        return socketfd;
    } else if (pollfds[0].events == pollfds[0].revents) {
        return STDIN_FILENO;
    } else {
        die("Unexpected poll result!\n");
    }
}

/// @brief Sends a game message to the server.
/// @param kind Message kind.
/// @return Message write result.
static ssize_t send_game_msg(pl_message_kind kind) {
    return pl_poll_msg_write(socketfd, (char *)&current_game,
                             (pl_message){
                                 .id = uid,
                                 .kind = kind,
                                 .raw_bytes_len = sizeof(ge_game),
                             },
                             PL_NO_TIMEOUT);
}

/// @brief Asks the user to select an opponent and checks if the client actually
/// exists.
/// @param opps Opps array.
/// @param opps_count Opps array length.
static void select_opponent(size_t *opps, size_t opps_count) {
    zero_current_game();

    for (;;) {
        // Tries to parse the stdin as a positive number.
        int parsed_num = success_or_print(
            read_stdin_num(),
            "Failed to parse the stdin as a positive number. Try again.\n");
        // Retries.
        if (parsed_num < 1) continue;

        current_game.chooser = uid;
        current_game.guesser = (size_t)parsed_num;

        // We can't let the user choose itself as its opponent.
        if (current_game.guesser == uid) {
            fprintf(stderr, "You can't choose your own id! Try again.\n");
            continue;
        }

        // Just in case, checks if the selected client acutally exists.
        for (size_t i = 0; i < opps_count; i++) {
            if (opps[i] == current_game.guesser) return;
        }

        // If it didn't make it this far, it means the selected client
        // is invalid.
        fprintf(stderr,
                "Please choose a client that's provided to you! Try again.\n");
    }
}

int main(int argc, char *argv[]) {
    cli_args args = parse_cli_args(argc, argv);

    // Handles the SIGINT (i.e Crtl+C) signal.
    signal(SIGINT, sigint_handler);

    socketfd =
        success_or_die(st_client_setup(HOST, args.port, args.unix_socket_file,
                                       args.unix_socket),
                       "Failed to setup a client socket");

    authenticate(args.pass);

    if (args.unix_socket) {
        printf("Connected to `%s` unix socket file.\n", args.unix_socket_file);
    } else printf("Connected to the %d port.\n", args.port);

    printf("Initially, we have to wait until enough opponents connect to the "
           "server...\n\n");

    for (;;) {
        int poll_fd;
        pl_message *m;

        if (is_in_game) {
            // Waits until it receives stdin or network message.
            poll_fd = stdin_vs_socket_poll();

            // It's a network message.
            if (poll_fd == socketfd) m = pl_msg_read(socketfd);
            // It's stdin.
            else {
                zero_current_game();

                if (is_guesser) {
                // This block asks for the guess in stdin and sends it to the
                // server.
                GUESS:
                    read_stdin(current_game.word, sizeof(current_game.word));
                    if (strlen(current_game.word) < 2) {
                        printf("Should be at least 2 characters.\n");
                        goto GUESS;
                    }

                    send_game_msg(mk_guess);
                } else {
                    // This means the chooser wants to hint the guesser.
                    read_stdin(current_game.word, sizeof(current_game.word));
                    send_game_msg(mk_hint);
                }

                continue;
            }
        } else {
            m = pl_poll_msg_read(socketfd, PL_NO_TIMEOUT);
        }

        switch (m->kind) {
        case mk_show_opponents: {
            // Casting to `size_t` is for escaping the alignment check of the
            // compiler. It's handled by the protocol's design.
            size_t *opps = (size_t *)((size_t)m->raw_bytes);
            size_t opps_count = m->raw_bytes_len / sizeof(size_t);

            // The message is ignored if a game is in process.
            if (is_in_game) continue;

            printf("We got enough opponents to start. Here's a list of them to "
                   "pick.\n");

            for (size_t i = 0; i < opps_count; i++) {
                // We can't let the user choose itself as its opponent.
                if (opps[i] == uid) continue;

                printf("Client number %zu\n", opps[i]);
            }

            printf("Enter the opponent id to begin. You can also wait until a "
                   "user picks you or more users connect.\n");

            poll_fd = stdin_vs_socket_poll();
            // Socket sent a new message. Another user challenged the current
            // user to a game.
            if (poll_fd == socketfd) continue;

            // Asks the user to select an opponent.
            select_opponent(opps, opps_count);

            printf("Enter a word to ask the opponent to guess. It should be "
                   "at least 2 characters and 55 characters at most.\n");

            read_stdin(current_game.word, sizeof(current_game.word));
            if (strlen(current_game.word) < 2)
                die("Should be at least 2 characters.\n");

            // Tries to select an opponent.
            send_game_msg(mk_select_opponent);
        } break;

        case mk_select_opponent: {
            is_in_game = true;
            // Same reason as before.
            current_game = *(ge_game *)((size_t)m->raw_bytes);

            if (current_game.chooser == uid) {
                is_guesser = false;
                printf("Game started! The guesser will start guessing.\n");
                printf("You can type and hit enter at any time in order to "
                       "hint the guesser.\n");
            } else {
                is_guesser = true;
                printf("Game started! You are the guesser. Start guessing.\n");
            }
        } break;

        case mk_wrong_guess: {
            if (is_guesser) {
                printf("Wrong guess!\n");
            } else {
                ge_game *g = ge_game_from_msg(m);
                printf("Opponent guessed wrong: %s\n", g->word);
            }
        } break;

        case mk_hint: {
            ge_game g = *ge_game_from_msg(m);
            printf("Your opponent gave you a hint: %s\n", g.word);
        } break;

        case mk_correct_guess:
            printf("The guess was correct! Game finised.\n");
            sigint_handler(0);
            // break will not be called.

        case mk_exit:
            sigint_handler(0);
            // break will not be called.

        default:
            break;
        }
    }
}
