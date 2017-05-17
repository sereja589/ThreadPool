#include <iostream>
#include <thread>
#include <vector>
#include <future>
#include <type_traits>
#include <queue>
#include <memory>

class ThreadPool {
public:
    ThreadPool(int threadCount);
    virtual ~ThreadPool();

    template <typename CallableType, typename ... ArgsType>
    auto addTask(CallableType &&func, ArgsType &&... args);

    void stop();

private:
    struct BaseTaskHolder {
        virtual void operator()() = 0;
    };

    template <typename Callable>
    struct TaskHolder : public BaseTaskHolder {
        std::unique_ptr<Callable> funcPtr;

        template <typename Func>
        TaskHolder(Func &&func) : funcPtr(std::make_unique<Func>(std::forward<Func>(func))) {

        }

        void operator()() override {
            (*funcPtr)();
        }

        TaskHolder(const TaskHolder&) = delete;
        TaskHolder(TaskHolder &&) {

        }
    };

    using BaseTaskHolderPtr = std::unique_ptr<BaseTaskHolder>;

    std::vector<std::thread> threads;
    std::queue<BaseTaskHolderPtr> tasksQueue;
    std::mutex tasksQueueMutex;
    std::condition_variable cvNewTask;

    std::atomic_bool isEnable;

    void invoke() {
        while (true) {
            std::unique_lock<std::mutex> lock(tasksQueueMutex);
            if (!tasksQueue.empty()) {
                BaseTaskHolderPtr p = std::move(tasksQueue.front());
                tasksQueue.pop();
                lock.unlock();
                (*p)();
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
    std::packaged_task<ReturnType()> task([func, args...] () { return func(args...); });
    auto ret = task.get_future();
    {
        std::unique_lock<std::mutex> lock(tasksQueueMutex);
        using TaskType = TaskHolder<decltype(task)>;
        std::move(task);
        tasksQueue.push(std::make_unique<TaskType>(std::move(task)));
    }
    cvNewTask.notify_one();
    return ret;
}

void ThreadPool::stop() {
    isEnable = false;
}

int isPrime(int x) {
//    std::cout << "work" << std::endl;
    for (int i = 2; i * i <= x; ++i) {
        if (x % i == 0) {
            return false;
        }
    }
    return true;
}

int main() {
    ThreadPool pool(2);
    std::vector<std::future<int>> v;
    int a;
    while (std::cin >> a) {
        v.push_back(pool.addTask(isPrime, a));
    }
    for (auto &f : v) {
        std::cout << f.get() << std::endl;
    }
    pool.stop();
}