#include "logistics/simulation/SimClock.h"
#include "logistics/detail/checked_math.hpp"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace logistics {
SimClock::SimClock(Minutes start, Minutes step) : now_(start), step_(step) {
    if (!std::isfinite(start) || start < 0 || !std::isfinite(step) || step <= 0) {
        throw std::invalid_argument("clock start must be finite/nonnegative and step finite/positive");
    }
}
void SimClock::advance() { advance(step_); }
void SimClock::advance(Minutes minutes) {
    if (!std::isfinite(minutes) || minutes < 0) { throw std::invalid_argument("clock increment must be finite and nonnegative"); }
    const double next = detail::checked_sum(now_, minutes);
    if (minutes > 0 && next == now_) { throw std::overflow_error("clock increment is below numeric precision"); }
    now_ = next;
}
std::string SimClock::format() const { return format(now_); }
std::string SimClock::format(Minutes minutes) {
    if (!std::isfinite(minutes) || minutes < 0) { throw std::invalid_argument("invalid time to format"); }
    const double whole = std::floor(minutes);
    const double days = std::floor(whole / 1440);
    const auto within_day = static_cast<int>(std::fmod(whole, 1440));
    std::ostringstream text;
    if (days > 0) { text << "D+" << std::fixed << std::setprecision(0) << days << ' '; }
    text << std::setfill('0') << std::setw(2) << within_day / 60 << ':' << std::setw(2) << within_day % 60;
    return text.str();
}
}  // namespace logistics
