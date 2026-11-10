#include "App.h"

#include "core/TraceReader.h"

#include <algorithm>
#include <string>
#include <utility>

namespace {

// Playback speeds in requests per second. Up and Down move through this list.
const double kSpeeds[] = {0.5, 1, 2, 4, 8, 16, 32, 64, 128};
constexpr std::size_t kSpeedCount = sizeof(kSpeeds) / sizeof(kSpeeds[0]);
constexpr std::size_t kDefaultSpeed = 2;  // 2 per second

constexpr double kLongAgo = 1e9;  // "no step yet", so nothing flashes

std::string fileName(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

}  // namespace

App::App(const Options& options) : config_(options.config), speedIndex_(kDefaultSpeed), sinceStep_(kLongAgo) {
    if (options.traceFile) {
        traces_.push_back({fileName(*options.traceFile), "Requests read from a trace file",
                           TraceReader::load(*options.traceFile, config_.addressBits)});
    }
    std::size_t first = 0;
    for (NamedTrace& trace : TraceGenerator(config_).builtins()) {
        if (options.builtin && trace.name == *options.builtin) first = traces_.size();
        traces_.push_back(std::move(trace));
    }
    load(first);
}

bool App::handle(Command command) {
    switch (command) {
        case Command::Step:
            playing_ = false;
            stepOnce();
            break;
        case Command::TogglePlay:
            if (playing_) {
                playing_ = false;
            } else {
                if (sim_->finished()) restart();  // at the end, play starts over
                playing_ = true;
                untilNextStep_ = 0.0;  // take the first step right away
            }
            break;
        case Command::Faster:
            speedIndex_ = std::min(speedIndex_ + 1, kSpeedCount - 1);
            untilNextStep_ = std::min(untilNextStep_, 1.0 / stepsPerSecond());  // takes effect now
            break;
        case Command::Slower:
            if (speedIndex_ > 0) --speedIndex_;
            break;
        case Command::Reset:
            playing_ = false;
            restart();
            break;
        case Command::NextTrace:
            load((traceIndex_ + 1) % traces_.size());
            break;
        case Command::Quit:
            return false;
        case Command::None:
            break;
    }
    return true;
}

void App::update(double seconds) {
    seconds = std::min(seconds, 0.25);  // after a stall (say, dragging the window) don't race ahead
    sinceStep_ += seconds;
    if (!playing_) return;

    untilNextStep_ -= seconds;
    while (untilNextStep_ <= 0.0 && !sim_->finished()) {
        stepOnce();
        untilNextStep_ += 1.0 / stepsPerSecond();
    }
    if (sim_->finished()) playing_ = false;
}

void App::advance(std::size_t count) {
    for (std::size_t i = 0; i < count && !sim_->finished(); ++i) stepOnce();
}

double App::stepsPerSecond() const {
    return kSpeeds[speedIndex_];
}

void App::load(std::size_t traceIndex) {
    traceIndex_ = traceIndex;
    sim_ = std::make_unique<Simulator>(config_, traces_[traceIndex_].requests);
    playing_ = false;
    sinceStep_ = kLongAgo;
}

void App::restart() {
    sim_->reset();
    sinceStep_ = kLongAgo;
}

void App::stepOnce() {
    if (sim_->finished()) return;
    sim_->step();
    sinceStep_ = 0.0;
}
