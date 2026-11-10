#include "Input.h"

#include <allegro5/allegro.h>

Command commandForKey(int keycode) {
    switch (keycode) {
        case ALLEGRO_KEY_SPACE:
            return Command::Step;
        case ALLEGRO_KEY_P:
            return Command::TogglePlay;
        case ALLEGRO_KEY_UP:
            return Command::Faster;
        case ALLEGRO_KEY_DOWN:
            return Command::Slower;
        case ALLEGRO_KEY_R:
            return Command::Reset;
        case ALLEGRO_KEY_T:
            return Command::NextTrace;
        case ALLEGRO_KEY_ESCAPE:
            return Command::Quit;
        default:
            return Command::None;
    }
}

bool repeatsWhenHeld(Command command) {
    return command == Command::Step || command == Command::Faster || command == Command::Slower;
}
