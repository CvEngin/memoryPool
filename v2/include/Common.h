#pragma once

#include <cstddef>
#include <atomic>
#include <array>

namespace H_memoryPool
{
    // 对齐数和大小定义
    constexpr size_t ALIGNMENT = 8;
    constexpr size_t MAX_BYTES = 256 * 1024;                 // 256KB
    constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT; // ALIGNMENT 等于指针 void* 的大小

    // 内存块头部信息
    struct BlockHeader
    {
        size_t size;       // 内存块大小
        bool isUse;        // 使用标志
        BlockHeader *next; // 下一个内存块指针
    };

    // 大小类管理
    class SizeClass
    {
    public:
        // 向上射入为对齐数的倍数, 保证返回的大小是ALIGNMENT的倍数
        static size_t roundUp(size_t bytes)
        {
            return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
        }

        static size_t getIndex(size_t bytes)
        {
            bytes = std::max(bytes, ALIGNMENT);
            // 向上取整后-1
            return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
        }
    };

}