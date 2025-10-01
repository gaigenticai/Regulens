#pragma once

#include <chrono>
#include <atomic>

namespace regulens {

/**
 * @brief Simple timer utility for measuring elapsed time
 */
class Timer {
public:
    /**
     * @brief Create a timer starting from now
     */
    Timer() : start_time_(std::chrono::steady_clock::now()) {}

    /**
     * @brief Reset the timer to start from now
     */
    void reset() {
        start_time_ = std::chrono::steady_clock::now();
    }

    /**
     * @brief Get elapsed time since timer creation/reset
     * @return Elapsed time in milliseconds
     */
    std::chrono::milliseconds elapsed() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time_);
    }

    /**
     * @brief Get elapsed time in seconds
     * @return Elapsed time in seconds
     */
    double elapsed_seconds() const {
        return std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start_time_).count();
    }

    /**
     * @brief Check if specified time has elapsed
     * @param duration Duration to check
     * @return true if duration has elapsed
     */
    bool has_elapsed(std::chrono::milliseconds duration) const {
        return elapsed() >= duration;
    }

private:
    std::chrono::steady_clock::time_point start_time_;
};

/**
 * @brief High-resolution performance timer for benchmarking
 */
class PerformanceTimer {
public:
    PerformanceTimer() : start_time_(std::chrono::high_resolution_clock::now()) {}

    void reset() {
        start_time_ = std::chrono::high_resolution_clock::now();
    }

    std::chrono::nanoseconds elapsed() const {
        return std::chrono::high_resolution_clock::now() - start_time_;
    }

    double elapsed_microseconds() const {
        return std::chrono::duration<double, std::micro>(
            std::chrono::high_resolution_clock::now() - start_time_).count();
    }

    double elapsed_milliseconds() const {
        return std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - start_time_).count();
    }

    double elapsed_seconds() const {
        return std::chrono::duration<double>(
            std::chrono::high_resolution_clock::now() - start_time_).count();
    }

private:
    std::chrono::high_resolution_clock::time_point start_time_;
};

/**
 * @brief Atomic timer that can be safely accessed from multiple threads
 */
class AtomicTimer {
public:
    AtomicTimer() {
        reset();
    }

    void reset() {
        start_time_.store(std::chrono::steady_clock::now().time_since_epoch().count(),
                         std::memory_order_relaxed);
    }

    std::chrono::milliseconds elapsed() const {
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        auto start = start_time_.load(std::memory_order_relaxed);
        return std::chrono::milliseconds((now - start) / 1000000); // Convert nanoseconds to milliseconds
    }

    bool has_elapsed(std::chrono::milliseconds duration) const {
        return elapsed() >= duration;
    }

private:
    std::atomic<std::chrono::steady_clock::rep> start_time_;
};

} // namespace regulens
