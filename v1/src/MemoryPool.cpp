#include "../include/MemoryPool.h"

namespace H_memoryPool {
MemoryPool::MemoryPool(size_t BlockSize)
    : BlockSize_ (BlockSize)
    , SlotSize_ (0)
    , firstBlock_ (nullptr)
    , curSlot_ (nullptr)
    , freeList_ (nullptr)
    , lastSlot_ (nullptr) {}

MemoryPool::~MemoryPool() {
    Slot* cur = firstBlock_;
    while (cur) {
        Slot* next = cur->next;
        operator delete(reinterpret_cast<void*>(cur));
        cur = next;
    }
}

void MemoryPool::init(size_t size) {
    assert(size > 0);
    SlotSize_ = size;
    firstBlock_ = nullptr;
    curSlot_ = nullptr;
    freeList_ = nullptr;
    lastSlot_ = nullptr;
}

void* MemoryPool::allocate() {
    // 优先使用空闲链表中的内存槽
    Slot* slot = popFreeList();
    if (slot != nullptr) return slot;
    Slot* temp;
    {
        std::lock_guard<std::mutex> lock(mutexForBlock_);
        if (curSlot_ >= lastSlot_) {
            // 当前内存块已无内存槽可用, 开辟新的内存
            allocateNewBlock();
        }
        temp = curSlot_;
        // 这里不能直接 curSlot_ += SlotSize_ 因为curSlot_是Slot*类型, 所以需要除以SlotSize_再加1
        curSlot_ += SlotSize_ / sizeof(Slot);
    }
    return temp;
}

void MemoryPool::deallocate(void* ptr) {
    if (!ptr) return;
    Slot* slot = reinterpret_cast<Slot*>(ptr);
    pushFreeList(slot);
}

void MemoryPool::allocateNewBlock() {
    // 头插法插入新的内存块
    void* newBlock = operator new(BlockSize_);
    reinterpret_cast<Slot*>(newBlock)->next = firstBlock_;
    firstBlock_ = reinterpret_cast<Slot*>(newBlock);

    char* body = reinterpret_cast<char*>(newBlock) + sizeof(Slot*);
    size_t padingSize = padPointer(body, SlotSize_);  // 计算需要填充内存的大小
    curSlot_ = reinterpret_cast<Slot*>(body + padingSize);
    lastSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<char*>(newBlock) + BlockSize_ - SlotSize_ + 1);
    freeList_ = nullptr;
}

// 让指针对齐到槽大小的倍数位置
size_t MemoryPool::padPointer(char* ptr, size_t align) {
    // align 是槽大小
    return (align - reinterpret_cast<size_t>(ptr)) % align;
}

// 实现无锁入队操作
bool MemoryPool::pushFreeList(Slot* slot) {
    while (true) {
        Slot* oldHead = freeList_.load(std::memory_order_relaxed);
        slot->next.store(oldHead, std::memory_order_relaxed);
        // 尝试将新节点设置为头结点
        if (freeList_.compare_exchange_weak(oldHead, slot, std::memory_order_release, std::memory_order_relaxed)) {
            return true;
        }
        // 失败，说明另一个线程可能已经修改了freeList_
        // CAS 失败则重试
    }
}

// 实现无锁出队操作
Slot* MemoryPool::popFreeList() {
    while (true) {
        Slot* oldHead = freeList_.load(std::memory_order_relaxed);
        if (oldHead == nullptr) return nullptr;
        Slot* newHead = oldHead->next.load(std::memory_order_relaxed);
        // 尝试将新节点设置为头结点
        if (freeList_.compare_exchange_weak(oldHead, newHead, std::memory_order_release, std::memory_order_relaxed)) {
            return oldHead;
        }
    }
}

void HashBucket::initMemoryPool() {
    for (int i = 0; i < MEMORY_POOL_NUM; i++) {
        getMomoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
    }
}

//  单例模式
MemoryPool& HashBucket::getMomoryPool(int index) {
    static MemoryPool memoryPools[MEMORY_POOL_NUM];
    return memoryPools[index];
}
}