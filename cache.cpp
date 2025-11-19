#include "cache.h"

#include <climits>
#include <iostream>
#include <list>
#include <unordered_map>

// Node represents a cache entry
struct LFUCache::Node {
    uint32_t key;
    uint32_t value;
    uint32_t freq;
    Node(uint32_t k, uint32_t v) : key(k), value(v), freq(1) {}
};

// Frequency list for each frequency
struct LFUCache::FreqList {
    std::list<Node*> nodes;
};

struct LFUCache::Impl {
    uint32_t capacity;
    uint32_t minFreq;
    // key -> Node*
    std::unordered_map<uint32_t, Node*> keyMap;
    // freq -> list of Nodes (LRU at back)
    std::unordered_map<uint32_t, FreqList> freqMap;

    Impl(uint32_t cap) : capacity(cap), minFreq(0) {}
    ~Impl() {
        // cleanup heap-allocated nodes
        for (auto& kv : keyMap) delete kv.second;
    }
};

LFUCache::LFUCache(uint32_t capacity) : impl(new Impl(capacity)) {}
LFUCache::~LFUCache() { delete impl; }

// In your get function
uint32_t LFUCache::get(uint32_t key) {
    auto& p = *impl;
    auto it = p.keyMap.find(key);
    if (it == p.keyMap.end()) {
        // Miss
        std::cout << "Cache MISS: key 0x" << std::hex << key << std::dec << std::endl;
        return UINT32_MAX;
    }
    // Hit
    std::cout << "Cache HIT: key 0x" << std::hex << key << std::dec << ", freq now " << (it->second->freq + 1) << std::endl;
    Node* node = it->second;
    auto& oldList = p.freqMap[node->freq].nodes;
    oldList.remove(node);
    if (oldList.empty() && node->freq == p.minFreq) p.minFreq++;
    node->freq++;
    p.freqMap[node->freq].nodes.push_front(node);
    return node->value;
}

void LFUCache::put(uint32_t key, uint32_t value) {
    auto& p = *impl;
    if (p.capacity == 0) return;
    auto it = p.keyMap.find(key);
    if (it != p.keyMap.end()) {
        it->second->value = value;
        get(key);  // already prints cache hit
        return;
    }
    if (p.keyMap.size() >= p.capacity) {
        auto& lfuList = p.freqMap[p.minFreq].nodes;
        Node* toRemove = lfuList.back();
        std::cout << "Cache EVICT: key 0x" << std::hex << toRemove->key << std::dec << " (freq " << toRemove->freq << ")" << std::endl;
        p.keyMap.erase(toRemove->key);
        lfuList.pop_back();
        delete toRemove;
    }
    Node* node = new Node(key, value);
    p.keyMap[key] = node;
    p.freqMap[1].nodes.push_front(node);
    p.minFreq = 1;
    std::cout << "Cache PUT: key 0x" << std::hex << key << std::dec << std::endl;
}
