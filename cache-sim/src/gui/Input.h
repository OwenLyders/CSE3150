#pragma once

#include "App.h"

// The command for an Allegro key code, or Command::None for keys the GUI doesn't use.
Command commandForKey(int keycode);

// Whether holding the key down should repeat the command (stepping and speed do).
bool repeatsWhenHeld(Command command);
