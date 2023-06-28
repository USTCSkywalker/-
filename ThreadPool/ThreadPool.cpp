// ThreadPool.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <vector>
#include <chrono>

#include "ThreadPool.h"

int main() {
    ThreadPool pool(4); // 创建一个线程池，池中线程数为4
    std::vector< std::future<int> > results;    // 创建一个保存std::future<int>的数组，用于存储4个异步线程的结果

    for (int i = 0; i < 8; ++i) {   // 创建8个任务
        results.emplace_back(   // 一次保存每个异步结果
            pool.enqueue([i] {  // 将每个任务插入到任务队列中，每个任务的功能均为“打印+睡眠1s+打印+返回结果”
                std::cout << "hello " << i << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::cout << "world " << i << std::endl;
                return i * i;
                })
        );
    }

    for (auto&& result : results)   // 一次取出保存在results中的异步结果
        std::cout << result.get() << ' ';
    std::cout << std::endl;

    return 0;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
