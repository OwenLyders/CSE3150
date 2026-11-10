#pragma once

#include "core/CacheConfig.h"
#include "core/Options.h"
#include "core/Simulator.h"
#include "core/TraceGenerator.h"

#include <cstddef>
#include <memory>
#include <vector>

// What the user can ask for. Input.cpp maps keys to these.
enum class Command { None, Step, TogglePlay, Faster, Slower, Reset, NextTrace, Quit };

// The GUI's state, without any drawing: the traces it can show, a simulator for the current
// one, and playback. It uses no Allegro. Renderer only reads it, and App changes the
// simulator only through Simulator::step() and reset().
class App {
public:
    // Loads the --trace file (if any) and the five built-in traces. Starts on the file, the
    // --builtin choice, or else the first built-in. Throws if the trace file cannot be read.
    explicit App(const Options& options);

    // Returns false when the user wants to quit.
    bool handle(Command command);

    // Moves playback forward by `seconds` of wall-clock time.
    void update(double seconds);

    // Processes `count` requests right away (for the --steps flag).
    void advance(std::size_t count);

    const Simulator& simulator() const { return *sim_; }
    const NamedTrace& trace() const { return traces_[traceIndex_]; }
    std::size_t traceIndex() const { return traceIndex_; }
    std::size_t traceCount() const { return traces_.size(); }

    bool playing() const { return playing_; }
    double stepsPerSecond() const;
    double secondsSinceStep() const { return sinceStep_; }  // drives the flash on the touched line

private:
    void load(std::size_t traceIndex);
    void restart();
    void stepOnce();

    CacheConfig config_;
    std::vector<NamedTrace> traces_;
    std::size_t traceIndex_ = 0;
    std::unique_ptr<Simulator> sim_;

    bool playing_ = false;
    std::size_t speedIndex_;
    double untilNextStep_ = 0.0;
    double sinceStep_;
};
