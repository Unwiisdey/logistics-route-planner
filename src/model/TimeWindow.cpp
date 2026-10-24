#include "logistics/model/TimeWindow.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace logistics {
void TimeWindow::validate() const {
    if (!std::isfinite(earliest) || !std::isfinite(latest) || earliest < 0 || latest < earliest) {
        throw std::invalid_argument("invalid time window");
    }
}
WindowResult TimeWindow::evaluate(double arrival) const {
    validate();
    if (!std::isfinite(arrival) || arrival < 0) { throw std::invalid_argument("invalid arrival time"); }
    const double start = std::max(arrival, earliest);
    const double late = std::max(0.0, start - latest);
    // Numerical boundary tolerance only (1e-9 minute), not a business grace period.
    return {start, start - arrival, late <= 1e-9 ? 0 : late};
}
double TimeWindow::late_penalty(double arrival, double points_per_minute) const {
    if (!std::isfinite(points_per_minute) || points_per_minute < 0) {
        throw std::invalid_argument("late penalty rate must be finite and nonnegative");
    }
    const double late = evaluate(arrival).late_minutes;
    // Ignore only the existing 1e-9-minute numerical noise, also at integer boundaries.
    const double nearest = std::round(late);
    const double billable = std::ceil(std::abs(late - nearest) <= 1e-9 ? nearest : late);
    if (billable > 0 && points_per_minute > std::numeric_limits<double>::max() / billable) {
        throw std::overflow_error("late penalty exceeds the supported finite range");
    }
    return billable * points_per_minute;
}
}  // namespace logistics
