#pragma once
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

inline void check(bool condition, const std::string& message) {
    if (!condition) { throw std::runtime_error(message); }
}
inline void check_near(double actual, double expected) {
    check(std::isfinite(actual) && std::abs(actual - expected) < 1e-9, "numeric value mismatch");
}
template <typename Exception, typename Action>
void expect_throw(Action action, const std::string& message_fragment = "") {
    try { action(); }
    catch (const Exception& error) {
        check(std::string(error.what()).find(message_fragment) != std::string::npos,
              std::string("unexpected diagnostic: ") + error.what());
        return;
    }
    throw std::runtime_error("expected exception was not thrown");
}
using TestCase = std::pair<std::string, std::function<void()>>;
inline int run_tests(int argc, char* argv[], const std::vector<TestCase>& tests) {
    int failures = 0;
    int executed = 0;
    for (const auto& test : tests) {
        if (argc > 1 && test.first != argv[1]) { continue; }
        ++executed;
        try { test.second(); std::cout << "PASS " << test.first << '\n'; }
        catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL " << test.first << ": " << error.what() << '\n';
        }
    }
    return failures == 0 && executed > 0 ? 0 : 1;
}
