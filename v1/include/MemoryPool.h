#pragma once

#include <atomic>
#include <cassert>
#include <memory>
#include <iostream>
#include <cstdint>
#include <mutex>

namespace H_memoryPool
{
#define MEMORY_POOL_NUM 64
#define SLOT_BASE_SIZE 8
#define MAX_SLOT_SIZE 512 // 8,16,32...512

    // 槽结构体
    struct Slot
    {
        std::atomic<Slot *> next; // 原子指针
    };

    class MemoryPool
    {
    public:
        MemoryPool(size_t BlockSize = 4096); // 每一个内存池都设置为 4096 的大小
        ~MemoryPool();

        void init(size_t);

        void *allocate();
        void deallocate(void *);

    private:
        void allocateNewBlock();
        size_t padPointer(char *p, size_t align);

        // 使用 CAS 操作进行无锁入队和出队, 并保证线程安全
        bool pushFreeList(Slot *slot);
        Slot *popFreeList();

    private:
        int BlockSize_;                // 内存块大小
        int SlotSize_;                 // 槽大小
        Slot *firstBlock_;             // 指向内存池管理的首个实际内存块
        Slot *curSlot_;                // 指向当前未被使用的槽(目前可用的第一个内存块)
        std::atomic<Slot *> freeList_; // 指向空闲链表的头指针
        Slot *lastSlot_;               // 当前内存块最后能够存放元素的位置标识
        std::mutex mutexForBlock_;     // 保证多线程情况下避免不必要的重复内存开辟导致浪费
    };

    class HashBucket
    {
    public:
        static void initMemoryPool();
        static MemoryPool &getMomoryPool(int index);

        static void *useMemory(size_t size)
        {
            if (size <= 0)
                return nullptr;
            if (size > MAX_SLOT_SIZE)
                return operator new(size); // 如果大于最大槽大小，则直接使用 new 操作符分配内存
            // size = size / 8 向上取整, 内存分配只能大不能小
            return getMomoryPool(((size + 7) / SLOT_BASE_SIZE) + 1).allocate();
        }

        static void freeMemory(void *ptr, size_t size)
        {
            if (!ptr)
                return;
            if (size > MAX_SLOT_SIZE)
            {
                operator delete(ptr);
                return;
            }
            getMomoryPool(((size + 7) / SLOT_BASE_SIZE) + 1).deallocate(ptr);
        }

        template <typename T, typename... Args>
        friend T *newElement(Args &&...args);

        template <typename T>
        friend void deleteElement(T *ptr);
    };

    template <typename T, typename... Args>
    T *newElement(Args &&...args)
    {
        T *ptr = nullptr;
        // 根据元素大小，选择合适的内存池
        if ((ptr = reinterpret_cast<T *>(HashBucket::useMemory(sizeof(T)))) != nullptr)
        {
            // 在分配的内存上构造对象
            new (ptr) T(std::forward<Args>(args)...);
        }
        return ptr;
    }

    template <typename T>
    void deleteElement(T *ptr)
    {
        if (ptr)
        {
            // 调用对象的析构函数
            ptr->~T();
            // 内存回收
            HashBucket::freeMemory(reinterpret_cast<void *>(ptr), sizeof(T));
        }
    }

}