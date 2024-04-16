#pragma once

#include "console.h"
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>

/// Cli arguments of the program.
typedef struct {
    /// Entered password.
    const char *pass;
    /// TCP port.
    in_port_t port;
    /// Whether to use unix socket.
    bool unix_socket;
    /// Struct padding.
    char _padding[5];
    /// Path to the unix socket file.
    const char *unix_socket_file;
} cli_args;

/// @brief Parses cli arguments.
/// @param argc Arguments count.
/// @param argv Arguments array.
/// @return `cli_args` struct.
static cli_args parse_cli_args(int argc, char *argv[]) {
    // Sets default values.
    cli_args args =
        (cli_args){.port = 8080,
                   .unix_socket = false,
                   .pass = "password12345",
                   .unix_socket_file = "/tmp/guessing-game-unix-socket"};

    if (argc > 4) {
        die("Invalid number of arguments. You should pass either either one or "
            "two or three.\n");
    }
    if (argc < 2) {
        die("Invalid number of arguments. You should the password as the first "
            "argument.\n");
    }

    // Password.
    args.pass = argv[1];

    // If we got any arguments after password.
    if (argc >= 3) {
        // TCP or Unix.
        if (strcmp(argv[2], "unix") == 0) {
            args.unix_socket = true;
        } else if (strcmp(argv[2], "tcp") == 0) {
            args.unix_socket = false;
        } else {
            die("The second argument is invalid. You pass either `tcp` or "
                "`unix`.\n");
        }

        // TCP port or unix socket file path.
        if (argc == 4) {
            size_t len = strlen(argv[3]);
            if (len >= 108) {
                die("The third argument is invalid. It shouldn't be more than "
                    "108 characters.\n");
            }

            if (!args.unix_socket) {
                int parsed_port = atoi(argv[3]);
                if (parsed_port < 1) {
                    die("The third argument is invalid. Can't parse it as a "
                        "number for using it as the tcp port.\n");
                }

                args.port = (in_port_t)parsed_port;
            } else {
                args.unix_socket_file = argv[3];
            }
        }
    }

    return args;
}
