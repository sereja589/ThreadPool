#include <iostream>
#include <thread>
#include <vector>
#include <future>
#include <type_traits>
#include <queue>
#include <memory>
#include <stdexcept>

class ThreadPool {
public:
    ThreadPool(int threadCount);
    virtual ~ThreadPool();

    template <typename CallableType, typename ... ArgsType>
    auto addTask(CallableType &&func, ArgsType &&... args);

    void stop();

private:
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasksQueue;
    std::mutex tasksQueueMutex;
    std::condition_variable cvNewTask;

    std::atomic_bool isEnable;

    void invoke() {
        while (true) {
            std::unique_lock<std::mutex> lock(tasksQueueMutex);
            if (!tasksQueue.empty()) {
                std::function<void()> curTask = std::move(tasksQueue.front());
                tasksQueue.pop();
                lock.unlock();
                curTask();
                continue;
            }
            if (!isEnable) {
                return;
            }
            cvNewTask.wait(lock, [this](){ return !tasksQueue.empty() || isEnable; });
        }
    }
};

ThreadPool::ThreadPool(int threadCount) {
    isEnable = true;
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back( [this] () { invoke(); } );
    }
}

ThreadPool::~ThreadPool() {
    stop();
    for (auto &t : threads) {
        t.join();
    }
}

template<typename CallableType, typename ... ArgsType>
auto ThreadPool::addTask(CallableType &&func, ArgsType &&... args) {
    using ReturnType = decltype(func(args...));
    using TaskType = std::packaged_task<ReturnType()>;
    std::shared_ptr<TaskType> ptask =
            std::make_shared<TaskType>(std::bind(std::forward<CallableType>(func), std::forward<ArgsType>(args)...));
    auto ret = ptask->get_future();
    std::function<void()> taskWraper([pfunc = std::move(ptask)]() { (*pfunc)(); });
    {
        std::unique_lock<std::mutex> lock(tasksQueueMutex);
        tasksQueue.push(std::move(taskWraper));
    }
    cvNewTask.notify_one();
    return ret;
}

void ThreadPool::stop() {
    isEnable = false;
}

int isPrime(int x) {
    std::cout << "work" << std::endl;
    for (int i = 2; i * i <= x; ++i) {
        if (x % i == 0) {
            return false;
        }
    }
    return true;
}

int sum(int a, int b, int c) {
    return a + b + c;
}

class Func {
public:
    Func() {}
    int operator()(int a, int b, int c) {
        return a + b + c;
    }

private:
    Func(const Func&) {
        std::cout << 123 << std::endl;
    }

public:
    Func(Func&&) {

    }
};

int main() {
    ThreadPool pool(2);
    std::vector<std::future<int>> v;
    int a, b, c;
    while (std::cin >> a) {
        v.push_back(pool.addTask(isPrime, a));
    }
    for (auto &f : v) {
        std::cout << f.get() << std::endl;
    }
    pool.stop();
}