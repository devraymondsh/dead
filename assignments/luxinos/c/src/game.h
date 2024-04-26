#pragma once

#include "protocol.h"
#include <stddef.h>

/// A game.
typedef struct {
    /// Game's id
    size_t id;
    /// Guesser's id.
    size_t guesser;
    /// Chooser's (aka word giver) id.
    size_t chooser;
    /// Whether the game has been finished or not.
    bool finished;
    /// Word to guess. 50 bytes + 5 bytes of padding.
    char word[55];
} ge_game;

/// @brief Gives a `game` pointer from a `message` struct.
/// @return A `game` struct.
static ge_game *ge_game_from_msg(pl_message *msg) {
    return (ge_game *)((size_t)msg->raw_bytes);
}
