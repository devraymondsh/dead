#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// @brief Exits with an error message if the corresponding operation fails.
/// @param res Operation's result.
/// @param msg Error message to print on failure.
/// @return The successful result.
static int success_or_die(int res, const char *msg) {
    if (res < 0) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
    return res;
}

/// @brief Prints the error message if the corresponding operation fails. Zero
/// status is considered a failure.
/// @param res Operation's result.
/// @param msg Error message to print on failure.
/// @return The result.
static int success_or_print(int res, const char *msg) {
    if (res <= 0) fprintf(stderr, "%s", msg);
    return res;
}

/// @brief Exits with an error message.
/// @param msg Error message to print on failure.
static void __attribute((noreturn)) die(const char *msg) {
    fprintf(stderr, "%s", msg);
    exit(EXIT_FAILURE);
}

/// @brief Prints the bytes for debugging purposes.
/// @param ptr A pointer.
/// @param size Byte size.
static void print_bytes(void *ptr, int size) {
    unsigned char *p = ptr;
    int i;
    for (i = 0; i < size; i++) {
        printf("%02hhX ", p[i]);
    }
    printf("\n");
}

/// @brief Reads a line of stdin to `str`.
/// @param str A buffer to store the string.
/// @param n Number of bytes.
static size_t read_stdin(char *str, int n) {
    size_t i = 0;
    char *read = fgets(str, n, stdin);

    if (read) {
        while (str[i] != '\n' && str[i] != '\0') {
            i++;
        }

        if (str[i] == '\n') str[i] = '\0';
    }

    return i;
}

/// @brief Reads the stdin as a number
/// @return Parsing result.
static int read_stdin_num(void) {
    size_t str_len;
    char str[12];

    str_len = read_stdin(str, sizeof(str));
    if (str_len < 1) return -1;

    return atoi(str);
}
