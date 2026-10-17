#pragma once

#include <cmath>
#include <limits>
#include <stdexcept>

namespace logistics::detail {

inline double checked_sum(double left, double right) {
    if (!std::isfinite(left) || !std::isfinite(right) || left < 0.0 || right < 0.0 ||
        right > std::numeric_limits<double>::max() - left) {
        throw std::overflow_error("path totals exceed the supported finite nonnegative range");
    }
    return left + right;
}

}  // namespace logistics::detail
