#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <queue>

class EmptyQueue : std::exception {
public:
    const char* what() const throw() {
        return "EmptyQueue";
    }
};

template <typename T>
class SafeQueue {
public:
    SafeQueue() {}
    SafeQueue(SafeQueue const& other) {
        std::lock_guard<std::mutex> lock(other._mutex);
        _queue = other.que;
    }
    SafeQueue& operator=(SafeQueue const&) = delete;

    void push(T value) {
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push(value);
    }

    void pop(T& res) {
        std::lock_guard<std::mutex> lock(_mutex);
        if(_queue.empty()) throw EmptyQueue();
        res = _queue.front();
        _queue.pop();
    }
    std::shared_ptr<T> pop() {
        std::lock_guard<std::mutex> lock(_mutex);
        if(_queue.empty()) throw EmptyQueue();
        auto const res = std::make_shared<T>(_queue.front());
        _queue.pop();
        return res;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.size();
    }

private:
    std::queue<T> _queue;
    mutable std::mutex _mutex;
};