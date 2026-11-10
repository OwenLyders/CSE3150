#pragma once

#include "App.h"

#include <allegro5/allegro_font.h>

#include <cstddef>
#include <optional>
#include <string>

// Draws the whole window from the App's state. It only reads; nothing here changes the
// simulation.
//
//   +----------------------------------------------------------------+
//   | current request: operation, address in hex and colored binary  |
//   +-----------+------------------------+---------------------------+
//   | trace     | FIFO cache             | LRU cache                 |
//   | list      | one row per set,       |                           |
//   |           | one column per way     |                           |
//   +-----------+------------------------+---------------------------+
//   | statistics for each policy                            | keys   |
//   +----------------------------------------------------------------+
class Renderer {
public:
    static constexpr int kWidth = 1280;
    static constexpr int kHeight = 720;
    static constexpr std::size_t kMaxWays = 32;  // more ways than this don't fit in a grid panel

    // Loads the TTF font at fontPath, or falls back to Allegro's small built-in font.
    explicit Renderer(const std::string& fontPath);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool usingFallbackFont() const { return fallback_; }

    void draw(const App& app) const;

private:
    struct Box {
        float x, y, w, h;
    };

    void drawRequestBar(const App& app, Box box) const;
    void drawTrace(const App& app, Box box) const;
    void drawCache(const App& app, std::size_t cacheIndex, Box box) const;
    void drawLine(const CacheLine& line, const CacheConfig& config, Box cell, const AccessResult* touched,
                  bool nextVictim, float flash) const;
    void drawStats(const App& app, Box box) const;

    ALLEGRO_FONT* small_ = nullptr;
    ALLEGRO_FONT* body_ = nullptr;
    ALLEGRO_FONT* large_ = nullptr;
    bool fallback_ = false;
};
