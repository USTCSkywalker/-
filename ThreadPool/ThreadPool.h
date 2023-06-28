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
    std::vector< std::thread > workers; // ���ڴ���̵߳����飬��vector��������
    std::queue< std::function<void()> > tasks;  // ���ڴ������Ķ��У���queue���н��б��档��������Ϊstd::function<void()>����������д�ŵ���һ��������

    // ͬ��
    std::mutex queue_mutex; // һ������������еĻ��������ڲ�����������߳�ȡ��������Ҫ�������������а�ȫ����
    std::condition_variable condition;  // һ������֪ͨ�߳��������״̬����������������������֪ͨ�߳̿���ִ�У��������wait״̬
    bool stop;  // ��ʶ�̳߳ص�״̬�����ڹ����������ж��̳߳�״̬���˽�
};

// �̳߳صĹ��캯��
inline ThreadPool::ThreadPool(size_t threads)   // ���ղ���threads��ʾ�̳߳���Ҫ�������ٸ��߳�
    : stop(false)   // ��ʼ����Ա����stopΪfalse����ʾ�̳߳���������
{
    for (size_t i = 0; i < threads; ++i)    // ���δ���threads���̣߳��������߳�����workers��
        workers.emplace_back(
            [this]  // ������β������һ������
            {
                for (;;)    // ��ѭ������ʾÿ���̶߳�����������ִ��
                {
                    std::function<void()> task; // ���ڽ��պ�������������е�������ʵ����
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);   // �˳�ʱ�Զ��ͷ��̳߳ص�queue_mutex
                        this->condition.wait(lock,
                            [this] { return this->stop || !this->tasks.empty(); }); // ���̳߳���ֹͣ������������в�Ϊ�գ��򲻻���뵽wait״̬
                        if (this->stop && this->tasks.empty())
                            return; // ��������������֪ͨ���߳̾ͻ�������½���
                        task = std::move(this->tasks.front());
                        this->tasks.pop();  // ���̳߳��Ѿ�ֹͣ���������Ϊ�գ����̷߳���
                    }
                    task(); // ����������еĵ�һ��������task��ǣ�Ȼ����������и����񵯳�
                }
            }
            );
}

// ��������ӵ��̳߳ص����������
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type>   // ���ش洢��std::future�е�F(Args��)�������͵��첽ִ�н��
{
    using return_type = typename std::result_of<F(Args...)>::type;  // ʹ��return_type��ʾF(Args...)�ķ�������

    auto task = std::make_shared< std::packaged_task<return_type()> >(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );  // ����һ������ָ��task��ָ��һ����std::bind(std::forward<F>(f), std::forward<Args>(args)...����ʼ����std::packaged_task<return_type()>����

    std::future<return_type> res = task->get_future();  // taskָ�򴫵ݽ����ĺ���
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        // ���µ��������ڼ��������̳߳�ֹͣ�����׳��쳣
        // don't allow enqueueing after stopping the pool
        if (stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task]() { (*task)(); }); // ��task��ָ���f(args)���뵽tasks���������
    }
    condition.notify_one(); // �������������к���Ҫ����һ���߳�
    return res; // ���߳�ִ����ϣ����첽ִ�еĽ������
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex); // ��������м���
        stop = true;    // ��ֹͣ�������Ϊtrue��������ʹ���µĲ����������Ҳ��ִ��ʧ��
    }
    condition.notify_all(); // ʹ�������������������̣߳������̶߳�������ִ��
    for (std::thread& worker : workers)
        worker.join();  // ��ÿ���߳�����Ϊjoin���ȵ�ÿ���߳̽�����Ϻ����߳����˳�
}

#endif
