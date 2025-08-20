#pragma once

#include <queue>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <stdexcept>

template <typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(size_t maxSize = 100)
        : _maxSize(maxSize) {

    }

    void push(T &item) {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_queue.size() >= _maxSize) {
            _queue.pop_front();
        }
        _queue.push_back(std::move(item));
        _cond.notify_one();
    }

    void push(T &&item) {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push_back(std::move(item));
        _cond.notify_one();
    }

    void push_front(T &&item) {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push_front(std::move(item));
        _cond.notify_one();
    }

    bool tryPop(T &item) {
        std::unique_lock<std::mutex> lock(_mutex);
        if (_queue.empty()) {
            return false;
        }
        item = std::move(_queue.front());
        _queue.pop_front();
        return true;
    }

    bool waitPop(T& item, int timeoutMs = 10) {
        std::unique_lock<std::mutex> lock(_mutex);
        if (!_cond.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                            [this] {
            return !_queue.empty();
        }))
            return false;
        item = std::move(_queue.front());
        _queue.pop_front();
        return true;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.empty();
    }

    T back() const {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_queue.empty()) {
            throw std::runtime_error("Queue is empty");
        }
        return _queue.back();
    }

    bool tryBack(T &item) const {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_queue.empty()) return false;
        item = _queue.back();
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.clear();
    }

private:
    mutable std::deque<T> _queue;
    mutable std::mutex _mutex;
    std::condition_variable _cond;
    size_t _maxSize;
};