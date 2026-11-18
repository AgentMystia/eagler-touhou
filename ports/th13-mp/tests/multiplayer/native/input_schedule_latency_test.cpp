// Wall-clock schedule probe. Uses the production FrameCadence and
// SlowdownGovernor. Half-frame phase keeps a jittered 60 Hz callback from
// splitting into empty and double ticks, and its p95 stays under one frame.
// An empty two-seat stage step measured 0.015 ms, so a 5-frame rollback fits
// in the 12 ms display budget. The governor stays at 1x while prediction
// depth stays at 4-5 on a 60 ms link. Those measurements are why the cadence
// phase and the slowdown governor are left in place.
#include "../../../portable/sdl/FrameCadence.hpp"
#include "multiplayer/FrameSchedule.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

struct Stats {
    double p50 = 0, p95 = 0, mean = 0;
};

Stats summarize(std::vector<double> values) {
    Stats out;
    if (values.empty()) return out;
    std::sort(values.begin(), values.end());
    double sum = 0;
    for (double v : values) sum += v;
    out.mean = sum / double(values.size());
    out.p50 = values[(values.size() - 1) / 2];
    out.p95 = values[std::min(values.size() - 1, (values.size() * 95) / 100)];
    return out;
}

void print_stats(const char* name, const Stats& stats, const char* extra) {
    std::printf("{\"scenario\":\"%s\",\"p50Ms\":%.3f,\"p95Ms\":%.3f,\"meanMs\":%.3f%s}\n",
                name, stats.p50, stats.p95, stats.mean, extra);
}

// Steady 60 Hz display with ±1 ms callback jitter. A key press is sampled on
// the next logic tick of that callback. Half-frame phase is the production
// reset_cadence debt.
Stats single_player(bool half_phase, int& zero_callbacks, int& double_callbacks) {
    constexpr double kHz = 1. / 60.;
    std::vector<double> latency;
    zero_callbacks = 0;
    double_callbacks = 0;
    touhou::sdl::FrameCadence cadence;
    cadence.debt = half_phase ? touhou::sdl::FrameCadence::interval / 2 : 0;
    double t = 0;
    // Skip the first 120 callbacks so the phase debt has settled.
    for (int i = 0; i < 120; ++i) {
        const double jitter = ((i * 17) % 5 - 2) * 0.001;
        t += kHz + jitter;
        cadence.advance(std::max(0.0, kHz + jitter));
    }
    for (int i = 0; i < 600; ++i) {
        const double jitter = ((i * 17) % 5 - 2) * 0.001;
        const double dt = std::max(0.0, kHz + jitter);
        const double key_at = t + dt * ((i % 7) / 7.0);
        t += dt;
        const unsigned ticks = cadence.advance(dt);
        if (ticks == 0) ++zero_callbacks;
        if (ticks >= 2) ++double_callbacks;
        if (ticks > 0) latency.push_back((t - key_at) * 1000.0);
    }
    return summarize(latency);
}

// Local input is sampled when a fresh logic frame is earned. Remote
// confirmation lags by the injected one-way delay. Depth is how far the
// local sim sits past the newest confirmed remote frame. The production
// governor stretches the clock from that depth.
struct NetResult {
    Stats input, interval;
    double max_stretch = 1;
    unsigned max_depth = 0;
    unsigned stalled = 0;
};

NetResult online(double rtt_ms, double jitter_ms, int drop_every, double step_ms) {
    constexpr double kFrame = 1000. / 60.;
    constexpr double kBudget = 12.0;
    th13::multiplayer::SlowdownGovernor governor;
    NetResult result;
    std::vector<double> input_ms, interval_ms;
    double t = 0;
    double last_present = 0;
    unsigned sim = 0;
    unsigned confirmed_frame = 0;
    unsigned last_remote = 0;
    unsigned predicted_remote = 0;
    struct Packet { double arrive; unsigned frame; unsigned buttons; };
    std::vector<Packet> packets;
    double key_waiting = -1;
    for (int n = 0; n < 1800; ++n) {
        const double stretch = governor.stretch();
        result.max_stretch = std::max(result.max_stretch, stretch);
        const double key_phase = (n % 11) / 11.0;
        if (n % 11 == 0) key_waiting = t + kFrame * key_phase;
        t += kFrame / stretch;
        // Remote direction changes every 8 logic frames. Prediction holds the
        // last confirmed buttons, so a change rolls back to that frame.
        const unsigned remote_now = (sim / 8) & 1u;
        double one_way = rtt_ms / 2.0 + (((n * 13) % 9) - 4) * (jitter_ms / 4.0);
        if (drop_every > 0 && n % drop_every == 0) one_way += 16.0;
        if (one_way < 0) one_way = 0;
        packets.push_back({t + one_way, sim, remote_now});
        unsigned rollback = 0;
        std::vector<Packet> keep;
        for (const auto& packet : packets) {
            if (packet.arrive > t) { keep.push_back(packet); continue; }
            if (packet.frame >= confirmed_frame && packet.buttons != predicted_remote &&
                packet.frame < sim)
                rollback = std::max(rollback, sim - packet.frame);
            if (packet.frame >= confirmed_frame) {
                confirmed_frame = packet.frame;
                last_remote = packet.buttons;
            }
        }
        packets.swap(keep);
        predicted_remote = last_remote;
        const unsigned depth = th13::multiplayer::SlowdownGovernor::depth_of(sim, confirmed_frame);
        result.max_depth = std::max(result.max_depth, depth);
        governor.update(depth);
        double budget = kBudget;
        if (rollback) {
            const double replay = rollback * step_ms;
            if (replay > budget) {
                ++result.stalled;
                budget = 0;
            } else budget -= replay;
        }
        if (budget >= step_ms) {
            budget -= step_ms;
            ++sim;
            if (key_waiting >= 0 && t >= key_waiting) {
                input_ms.push_back(t - key_waiting);
                key_waiting = -1;
            }
            if (last_present > 0) interval_ms.push_back(t - last_present);
            last_present = t;
        }
    }
    result.input = summarize(input_ms);
    result.interval = summarize(interval_ms);
    return result;
}

}  // namespace

int main() {
    int zero_half = 0, double_half = 0, zero_plain = 0, double_plain = 0;
    const Stats half = single_player(true, zero_half, double_half);
    const Stats plain = single_player(false, zero_plain, double_plain);
    char extra[160];
    std::snprintf(extra, sizeof extra, ",\"zeroCallbacks\":%d,\"doubleCallbacks\":%d",
                  zero_half, double_half);
    print_stats("sp-half-phase", half, extra);
    std::snprintf(extra, sizeof extra, ",\"zeroCallbacks\":%d,\"doubleCallbacks\":%d",
                  zero_plain, double_plain);
    print_stats("sp-no-phase", plain, extra);

    const double steps[] = {1.0, 3.0, 6.0};
    const struct Profile { const char* name; double rtt; double jitter; int drop; } profiles[] = {
        {"mp-loopback", 0, 0, 0},
        {"mp-60ms", 60, 40, 3},
        {"mp-120ms-heavy-loss", 120, 40, 2},
    };
    for (const auto& profile : profiles) {
        for (double step : steps) {
            const NetResult net = online(profile.rtt, profile.jitter, profile.drop, step);
            std::snprintf(extra, sizeof extra,
                          ",\"stepMs\":%.1f,\"intervalP50Ms\":%.3f,\"intervalP95Ms\":%.3f,"
                          "\"maxStretch\":%.3f,\"maxDepth\":%u,\"stalled\":%u",
                          step, net.interval.p50, net.interval.p95,
                          net.max_stretch, net.max_depth, net.stalled);
            std::string name = std::string(profile.name) + "-step" + std::to_string(int(step));
            print_stats(name.c_str(), net.input, extra);
        }
    }
    // The half-frame phase must not add a frame of steady-state latency, and
    // it must keep the jittered display from splitting into empty callbacks.
    if (!(half.p95 < 20.0 && zero_half == 0 && double_half == 0)) return 1;
    if (!(plain.p95 < 20.0)) return 1;
    return 0;
}
