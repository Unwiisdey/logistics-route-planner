#pragma once
#include "logistics/model/common.hpp"
#include <string>

namespace logistics {
class SimClock {
public:
    explicit SimClock(Minutes start = 480.0, Minutes step = 5.0);
    Minutes now() const noexcept { return now_; }
    Minutes step() const noexcept { return step_; }
    void advance();
    void advance(Minutes minutes);
    std::string format() const;
    static std::string format(Minutes minutes);
private:
    Minutes now_;
    Minutes step_;
};
}  // namespace logistics
