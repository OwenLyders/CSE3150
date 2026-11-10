// cachesim_gui: steps through a trace one request at a time and shows a FIFO cache and an
// LRU cache side by side.
//
//   Space step   P play/pause   Up/Down speed   R reset   T next built-in trace   Esc quit
//
// Cache settings come from the same flags as the command-line runner, for example
//   cachesim_gui --cache 64 --block 4 --ways 4 --builtin hot-cold

#include "App.h"
#include "Input.h"
#include "Renderer.h"

#include "core/Format.h"
#include "core/Options.h"

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_ttf.h>

#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const char* const kFontFile = "JetBrainsMono-Regular.ttf";

// Flags only the GUI understands. Everything else goes to parseOptions().
struct GuiFlags {
    std::size_t steps = 0;                  // --steps N: start with N requests already processed
    std::optional<std::string> screenshot;  // --screenshot FILE: save one frame as an image, then quit
};

GuiFlags takeGuiFlags(std::vector<std::string>& args) {
    GuiFlags flags;
    std::vector<std::string> rest;
    for (std::size_t i = 0; i < args.size(); ++i) {
        std::string flag = args[i];
        std::optional<std::string> value;
        const std::size_t equals = flag.find('=');
        if (equals != std::string::npos) {
            value = flag.substr(equals + 1);
            flag = flag.substr(0, equals);
        }
        if (flag != "--steps" && flag != "--screenshot") {
            rest.push_back(args[i]);
            continue;
        }
        if (!value) {
            if (i + 1 >= args.size()) throw std::invalid_argument(flag + " needs a value");
            value = args[++i];
        }
        if (flag == "--screenshot") {
            flags.screenshot = *value;
        } else {
            const std::optional<std::uint64_t> steps = parseNumber(*value);
            if (!steps || *steps > std::numeric_limits<std::size_t>::max()) {
                throw std::invalid_argument("--steps needs a whole number, got '" + *value + "'");
            }
            flags.steps = static_cast<std::size_t>(*steps);
        }
    }
    args = rest;
    return flags;
}

void printUsage() {
    std::cout << "usage: cachesim_gui [options]\n"
                 "\n"
                 "Steps through a trace one request at a time and shows a FIFO cache and an LRU cache\n"
                 "side by side. Starts on the first built-in trace unless you choose one.\n"
                 "\n"
              << optionsHelp()
              << "\n"
                 "GUI only:\n"
                 "  --steps N             start with the first N requests already processed\n"
                 "  --screenshot FILE     draw one frame, save it as an image (for example a .png), quit\n"
                 "\n"
                 "Keys:\n"
                 "  Space step   P play/pause   Up/Down speed   R reset   T next trace   Esc quit\n";
}

// The font ships in assets/. Look in the source tree, then the current folder, then
// next to the executable.
std::string findFont() {
    std::vector<std::string> candidates;
#ifdef CACHESIM_ASSET_DIR
    candidates.push_back(std::string(CACHESIM_ASSET_DIR) + "/" + kFontFile);
#endif
    candidates.push_back(std::string("assets/") + kFontFile);
    if (ALLEGRO_PATH* path = al_get_standard_path(ALLEGRO_RESOURCES_PATH)) {
        al_append_path_component(path, "assets");
        al_set_path_filename(path, kFontFile);
        candidates.push_back(al_path_cstr(path, ALLEGRO_NATIVE_PATH_SEP));
        al_destroy_path(path);
    }
    for (const std::string& candidate : candidates) {
        if (al_filename_exists(candidate.c_str())) return candidate;
    }
    return candidates.front();
}

// On high-DPI screens the backbuffer can be bigger than the 1280x720 we asked for.
// Scale the layout to fill it.
void scaleToDisplay(ALLEGRO_DISPLAY* display) {
    ALLEGRO_TRANSFORM transform;
    al_identity_transform(&transform);
    al_scale_transform(&transform, static_cast<float>(al_get_display_width(display)) / Renderer::kWidth,
                       static_cast<float>(al_get_display_height(display)) / Renderer::kHeight);
    al_use_transform(&transform);
}

int runWindow(ALLEGRO_DISPLAY* display, App& app, const Renderer& renderer) {
    ALLEGRO_TIMER* timer = al_create_timer(1.0 / 60.0);
    ALLEGRO_EVENT_QUEUE* queue = al_create_event_queue();
    if (!timer || !queue) {
        std::cerr << "cachesim_gui: could not create the timer or event queue\n";
        if (timer) al_destroy_timer(timer);
        if (queue) al_destroy_event_queue(queue);
        return 1;
    }
    al_register_event_source(queue, al_get_display_event_source(display));
    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_timer_event_source(timer));
    al_start_timer(timer);

    bool running = true;
    bool redraw = true;
    double lastTime = al_get_time();
    while (running) {
        ALLEGRO_EVENT event;
        al_wait_for_event(queue, &event);

        if (event.type == ALLEGRO_EVENT_TIMER) {
            const double now = al_get_time();
            app.update(now - lastTime);
            lastTime = now;
            redraw = true;
        } else if (event.type == ALLEGRO_EVENT_KEY_CHAR) {
            const Command command = commandForKey(event.keyboard.keycode);
            if (!event.keyboard.repeat || repeatsWhenHeld(command)) {
                running = app.handle(command);
                redraw = true;
            }
        } else if (event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
            running = false;
        }

        if (running && redraw && al_is_event_queue_empty(queue)) {
            renderer.draw(app);
            al_flip_display();
            redraw = false;
        }
    }

    al_destroy_event_queue(queue);
    al_destroy_timer(timer);
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    // Parse everything before opening a window, so a typo just prints an error.
    std::vector<std::string> args(argv + 1, argv + argc);
    GuiFlags gui;
    Options options;
    try {
        gui = takeGuiFlags(args);
        options = parseOptions(args);
    } catch (const std::exception& error) {
        std::cerr << "cachesim_gui: " << error.what() << "\n(run with --help for usage)\n";
        return 2;
    }
    if (options.showHelp) {
        printUsage();
        return 0;
    }
    if (options.config.associativity > Renderer::kMaxWays) {
        std::cerr << "cachesim_gui: the cache grids fit at most " << Renderer::kMaxWays << " ways (got "
                  << options.config.associativity << "); cachesim_cli handles any setting\n";
        return 2;
    }

    std::unique_ptr<App> app;
    try {
        app = std::make_unique<App>(options);
    } catch (const std::exception& error) {
        std::cerr << "cachesim_gui: " << error.what() << "\n";
        return 1;
    }
    app->advance(gui.steps);

    if (!al_init() || !al_install_keyboard() || !al_init_font_addon() || !al_init_ttf_addon() ||
        !al_init_primitives_addon() || !al_init_image_addon()) {
        std::cerr << "cachesim_gui: could not initialize Allegro\n";
        return 1;
    }

    al_set_new_display_option(ALLEGRO_SAMPLE_BUFFERS, 1, ALLEGRO_SUGGEST);  // smoother edges
    al_set_new_display_option(ALLEGRO_SAMPLES, 4, ALLEGRO_SUGGEST);
    ALLEGRO_DISPLAY* display = al_create_display(Renderer::kWidth, Renderer::kHeight);
    if (!display) {
        std::cerr << "cachesim_gui: could not open a " << Renderer::kWidth << "x" << Renderer::kHeight
                  << " window\n";
        return 1;
    }
    al_set_window_title(display, "Cache Simulator: FIFO vs LRU");
    scaleToDisplay(display);

    int status = 0;
    {
        const Renderer renderer(findFont());
        if (renderer.usingFallbackFont()) {
            std::cerr << "cachesim_gui: could not load assets/" << kFontFile << "; using Allegro's built-in font\n";
        }

        if (gui.screenshot) {
            renderer.draw(*app);
            if (!al_save_bitmap(gui.screenshot->c_str(), al_get_backbuffer(display))) {
                std::cerr << "cachesim_gui: could not save " << *gui.screenshot << "\n";
                status = 1;
            }
        } else {
            status = runWindow(display, *app, renderer);
        }
    }  // the renderer frees its fonts here, while the display still exists

    al_destroy_display(display);
    return status;
}
