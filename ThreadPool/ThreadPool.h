#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool {
public:
    ThreadPool(size_t);
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;
    ~ThreadPool();
private:
    std::vector< std::thread > workers; // 用于存放线程的数组，用vector容器保存
    std::queue< std::function<void()> > tasks;  // 用于存放任务的队列，用queue队列进行保存。任务类型为std::function<void()>，任务队列中存放的是一个个函数

    // 同步
    std::mutex queue_mutex; // 一个访问任务队列的互斥锁，在插入任务或者线程取出任务都需要借助互斥锁进行安全访问
    std::condition_variable condition;  // 一个用于通知线程任务队列状态的条件变量，若有任务则通知线程可以执行，否则进入wait状态
    bool stop;  // 标识线程池的状态，用于构造与析构中对线程池状态的了解
};

// 线程池的构造函数
inline ThreadPool::ThreadPool(size_t threads)   // 接收参数threads表示线程池中要创建多少个线程
    : stop(false)   // 初始化成员变量stop为false，表示线程池正在运行
{
    for (size_t i = 0; i < threads; ++i)    // 依次创建threads个线程，并放入线程数组workers中
        workers.emplace_back(
            [this]  // 在容器尾部插入一个对象
            {
                for (;;)    // 死循环，表示每个线程都将反复这样执行
                {
                    std::function<void()> task; // 用于接收后续从任务队列中弹出的真实任务
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);   // 退出时自动释放线程池的queue_mutex
                        this->condition.wait(lock,
                            [this] { return this->stop || !this->tasks.empty(); }); // 若线程池已停止或者任务队列中不为空，则不会进入到wait状态
                        if (this->stop && this->tasks.empty())
                            return; // 若后续条件变量通知，线程就会继续向下进行
                        task = std::move(this->tasks.front());
                        this->tasks.pop();  // 若线程池已经停止且任务队列为空，则线程返回
                    }
                    task(); // 将任务队列中的第一个任务用task标记，然后将任务队列中该任务弹出
                }
            }
            );
}

// 将任务添加到线程池的任务队列中
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type>   // 返回存储在std::future中的F(Args…)返回类型的异步执行结果
{
    using return_type = typename std::result_of<F(Args...)>::type;  // 使用return_type表示F(Args...)的返回类型

    auto task = std::make_shared< std::packaged_task<return_type()> >(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );  // 创建一个智能指针task，指向一个用std::bind(std::forward<F>(f), std::forward<Args>(args)...来初始化的std::packaged_task<return_type()>对象

    std::future<return_type> res = task->get_future();  // task指向传递进来的函数
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        // 在新的作用域内加锁，若线程池停止，则抛出异常
        // don't allow enqueueing after stopping the pool
        if (stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task]() { (*task)(); }); // 将task所指向的f(args)插入到tasks任务队列中
    }
    condition.notify_one(); // 任务加入任务队列后，需要唤醒一个线程
    return res; // 待线程执行完毕，将异步执行的结果返回
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex); // 对任务队列加锁
        stop = true;    // 将停止标记设置为true，后续即使有新的插入任务操作也会执行失败
    }
    condition.notify_all(); // 使用条件变量唤醒所有线程，所有线程都会往下执行
    for (std::thread& worker : workers)
        worker.join();  // 将每个线程设置为join，等到每个线程结束完毕后，主线程再退出
}

#endif
