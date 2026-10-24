#pragma once

namespace logistics {
struct WindowResult {
    double service_start;
    double waiting_minutes;
    double late_minutes;
    bool on_time() const noexcept { return late_minutes == 0; }
};

// Continuous minutes from simulation-day midnight; endpoints are inclusive.
struct TimeWindow {
    double earliest{0.0};
    double latest{0.0};
    void validate() const;
    WindowResult evaluate(double arrival) const;
    // Started late minutes (rounded up) times a finite nonnegative rate.
    double late_penalty(double arrival, double points_per_minute) const;
};
}  // namespace logistics
