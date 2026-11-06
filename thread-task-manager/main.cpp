#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// Схема: submit() -> очередь -> свободный поток -> future.get().
class TaskManager {
    std::mutex mutex_;
    std::condition_variable ready_;
    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    bool stopping_ = false;

    void run() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(mutex_);
                ready_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
                if (stopping_ && tasks_.empty()) return;
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task(); // Выполняем вне блокировки: другие потоки берут свои задачи.
        }
    }

    void finish() {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        ready_.notify_all();
        for (auto& worker : workers_) worker.join();
    }

public:
    explicit TaskManager(unsigned count = 4) {
        if (count == 0) throw std::invalid_argument("Thread count must be positive");
        try {
            for (unsigned i = 0; i < count; ++i)
                workers_.emplace_back([this] { run(); });
        } catch (...) {
            finish(); // Завершаем уже созданные потоки, если создание следующего не удалось.
            throw;
        }
    }

    ~TaskManager() { finish(); } // Сначала выполняем оставшиеся задачи, затем join().

    template<class F>
    auto submit(F&& function) {
        using Result = std::invoke_result_t<std::decay_t<F>&>;
        auto task = std::make_shared<std::packaged_task<Result()>>(
            std::forward<F>(function));
        auto result = task->get_future();
        {
            std::lock_guard lock(mutex_);
            if (stopping_) throw std::runtime_error("TaskManager is stopping");
            tasks_.emplace([task] { (*task)(); });
        }
        ready_.notify_one();
        return result;
    }
};

int main() {
    TaskManager manager(2);

    auto first  = manager.submit([] { return 10 + 20; });
    auto second = manager.submit([] { return 5 * 6; });

    std::cout << first.get() << '\n';  // 30; ждём результат первой задачи.
    std::cout << second.get() << '\n'; // 30; ждём результат второй задачи.
    return 0;
}
