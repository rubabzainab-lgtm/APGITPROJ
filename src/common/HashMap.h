#pragma once
#include <cstring>
#include <cstddef>
#include <stdexcept>

// Custom hash map with separate chaining (linked list per bucket).
// Key = const char* (string), Value = template T.
// For integer keys use the int specialization below.

template <typename V>
struct HashNode {
    char*       key;
    V           value;
    HashNode<V>* next;

    HashNode(const char* k, const V& v) : value(v), next(nullptr) {
        int len = 0;
        while (k[len]) ++len;
        key = new char[len + 1];
        for (int i = 0; i <= len; ++i) key[i] = k[i];
    }

    ~HashNode() { delete[] key; }
};

template <typename V>
class HashMap {
public:
    explicit HashMap(int buckets = 256)
        : bucketCount_(buckets), size_(0) {
        buckets_ = new HashNode<V>*[bucketCount_];
        for (int i = 0; i < bucketCount_; ++i) buckets_[i] = nullptr;
    }

    ~HashMap() {
        for (int i = 0; i < bucketCount_; ++i) {
            HashNode<V>* cur = buckets_[i];
            while (cur) {
                HashNode<V>* nxt = cur->next;
                delete cur;
                cur = nxt;
            }
        }
        delete[] buckets_;
    }

    void put(const char* key, const V& val) {
        int idx = hash(key);
        HashNode<V>* cur = buckets_[idx];
        while (cur) {
            if (strcmp(cur->key, key) == 0) { cur->value = val; return; }
            cur = cur->next;
        }
        HashNode<V>* node = new HashNode<V>(key, val);
        node->next = buckets_[idx];
        buckets_[idx] = node;
        ++size_;
    }

    bool get(const char* key, V& outVal) const {
        int idx = hash(key);
        HashNode<V>* cur = buckets_[idx];
        while (cur) {
            if (strcmp(cur->key, key) == 0) { outVal = cur->value; return true; }
            cur = cur->next;
        }
        return false;
    }

    bool contains(const char* key) const {
        V dummy;
        return get(key, dummy);
    }

    bool remove(const char* key) {
        int idx = hash(key);
        HashNode<V>* cur  = buckets_[idx];
        HashNode<V>* prev = nullptr;
        while (cur) {
            if (strcmp(cur->key, key) == 0) {
                if (prev) prev->next = cur->next;
                else      buckets_[idx] = cur->next;
                delete cur;
                --size_;
                return true;
            }
            prev = cur;
            cur  = cur->next;
        }
        return false;
    }

    int  size()  const { return size_; }
    bool empty() const { return size_ == 0; }

    // Iterate all entries via callback
    template <typename Fn>
    void forEach(Fn fn) const {
        for (int i = 0; i < bucketCount_; ++i) {
            HashNode<V>* cur = buckets_[i];
            while (cur) {
                fn(cur->key, cur->value);
                cur = cur->next;
            }
        }
    }

private:
    int hash(const char* key) const {
        // djb2
        unsigned long h = 5381;
        while (*key) { h = ((h << 5) + h) + (unsigned char)(*key); ++key; }
        return (int)(h % (unsigned long)bucketCount_);
    }

    HashNode<V>** buckets_;
    int           bucketCount_;
    int           size_;
};


// Integer-keyed hash map (for page ID → DLL node mapping in LRU cache)
template <typename V>
struct IntHashNode {
    int          key;
    V            value;
    IntHashNode<V>* next;
    IntHashNode(int k, const V& v) : key(k), value(v), next(nullptr) {}
};

template <typename V>
class IntHashMap {
public:
    explicit IntHashMap(int buckets = 1024)
        : bucketCount_(buckets), size_(0) {
        buckets_ = new IntHashNode<V>*[bucketCount_];
        for (int i = 0; i < bucketCount_; ++i) buckets_[i] = nullptr;
    }

    ~IntHashMap() {
        for (int i = 0; i < bucketCount_; ++i) {
            IntHashNode<V>* cur = buckets_[i];
            while (cur) {
                IntHashNode<V>* nxt = cur->next;
                delete cur;
                cur = nxt;
            }
        }
        delete[] buckets_;
    }

    void put(int key, const V& val) {
        int idx = hash(key);
        IntHashNode<V>* cur = buckets_[idx];
        while (cur) {
            if (cur->key == key) { cur->value = val; return; }
            cur = cur->next;
        }
        IntHashNode<V>* node = new IntHashNode<V>(key, val);
        node->next = buckets_[idx];
        buckets_[idx] = node;
        ++size_;
    }

    bool get(int key, V& outVal) const {
        int idx = hash(key);
        IntHashNode<V>* cur = buckets_[idx];
        while (cur) {
            if (cur->key == key) { outVal = cur->value; return true; }
            cur = cur->next;
        }
        return false;
    }

    bool contains(int key) const {
        V dummy;
        return get(key, dummy);
    }

    bool remove(int key) {
        int idx = hash(key);
        IntHashNode<V>* cur  = buckets_[idx];
        IntHashNode<V>* prev = nullptr;
        while (cur) {
            if (cur->key == key) {
                if (prev) prev->next = cur->next;
                else      buckets_[idx] = cur->next;
                delete cur;
                --size_;
                return true;
            }
            prev = cur;
            cur  = cur->next;
        }
        return false;
    }

    int  size()  const { return size_; }
    bool empty() const { return size_ == 0; }

private:
    int hash(int key) const { return ((unsigned int)key * 2654435761u) % (unsigned int)bucketCount_; }

    IntHashNode<V>** buckets_;
    int              bucketCount_;
    int              size_;
};
