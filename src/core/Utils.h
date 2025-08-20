#pragma once

#include <string>
#include <vector>
#include <optional>
#include "media/ThreadSafeQueue.h"

namespace Utils {
    std::string formatTime(double seconds);

    std::string selectVideoFile(const std::vector<std::string> &files);

    template<typename T>
    std::optional<T> waitPopOpt(ThreadSafeQueue<T> &queue, int timeoutMs = 10) {
        T item;
        if (queue.waitPop(item, timeoutMs)) {
            return item;
        }
        return std::nullopt;
    }
}