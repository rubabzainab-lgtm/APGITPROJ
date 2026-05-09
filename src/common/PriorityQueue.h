#pragma once
#include <stdexcept>

// Min-heap based priority queue.
// Lower priority value = higher urgency (admin=0, user=1).
template <typename T>
struct PQEntry {
    int priority;
    T   data;
    bool operator<(const PQEntry<T>& o) const { return priority < o.priority; }
};

template <typename T>
class PriorityQueue {
public:
    explicit PriorityQueue(int capacity = 256)
        : capacity_(capacity), size_(0) {
        heap_ = new PQEntry<T>[capacity_];
    }

    ~PriorityQueue() { delete[] heap_; }

    void enqueue(const T& val, int priority) {
        if (size_ >= capacity_) resize();
        heap_[size_] = {priority, val};
        siftUp(size_);
        ++size_;
    }

    T dequeue() {
        if (isEmpty()) throw std::underflow_error("PriorityQueue underflow");
        T val = heap_[0].data;
        heap_[0] = heap_[--size_];
        if (size_ > 0) siftDown(0);
        return val;
    }

    const T& peek() const {
        if (isEmpty()) throw std::underflow_error("PriorityQueue empty");
        return heap_[0].data;
    }

    int peekPriority() const {
        if (isEmpty()) throw std::underflow_error("PriorityQueue empty");
        return heap_[0].priority;
    }

    bool isEmpty() const { return size_ == 0; }
    int  size()    const { return size_; }

private:
    void siftUp(int i) {
        while (i > 0) {
            int parent = (i - 1) / 2;
            if (heap_[parent].priority <= heap_[i].priority) break;
            swap(parent, i);
            i = parent;
        }
    }

    void siftDown(int i) {
        while (true) {
            int smallest = i;
            int left  = 2 * i + 1;
            int right = 2 * i + 2;
            if (left  < size_ && heap_[left].priority  < heap_[smallest].priority) smallest = left;
            if (right < size_ && heap_[right].priority < heap_[smallest].priority) smallest = right;
            if (smallest == i) break;
            swap(i, smallest);
            i = smallest;
        }
    }

    void swap(int a, int b) {
        PQEntry<T> tmp = heap_[a];
        heap_[a] = heap_[b];
        heap_[b] = tmp;
    }

    void resize() {
        int newCap = capacity_ * 2;
        PQEntry<T>* newHeap = new PQEntry<T>[newCap];
        for (int i = 0; i < size_; ++i) newHeap[i] = heap_[i];
        delete[] heap_;
        heap_ = newHeap;
        capacity_ = newCap;
    }

    PQEntry<T>* heap_;
    int capacity_;
    int size_;
};
