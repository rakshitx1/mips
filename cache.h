#pragma once
#include <cstdint>

class LFUCache {
   public:
    explicit LFUCache(uint32_t capacity);
    ~LFUCache();

    // Returns value if found, or UINT32_MAX as a "not found" marker
    uint32_t get(uint32_t key);

    // Sets key to value in cache
    void put(uint32_t key, uint32_t value);

   private:
    struct Node;
    struct FreqList;
    struct Impl;
    Impl* impl;
};
