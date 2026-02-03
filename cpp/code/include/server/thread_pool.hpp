#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>

class ThreadPool {
public:
    ThreadPool(size_t numberOfThreads);
    ~ThreadPool();

    void enqueue(std::function<void()> task);

private:
    std::vector<std::thread> workerThreads;
    std::queue<std::function<void()>> clientTasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};
