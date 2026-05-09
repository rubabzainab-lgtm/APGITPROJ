#pragma once
#include <cstddef>

template <typename T>
struct DLLNode {
    T data;
    DLLNode<T>* prev;
    DLLNode<T>* next;
    explicit DLLNode(const T& d) : data(d), prev(nullptr), next(nullptr) {}
};

template <typename T>
class DoublyLinkedList {
public:
    DoublyLinkedList() : head_(nullptr), tail_(nullptr), size_(0) {}

    ~DoublyLinkedList() {
        DLLNode<T>* cur = head_;
        while (cur) {
            DLLNode<T>* nxt = cur->next;
            delete cur;
            cur = nxt;
        }
    }

    DLLNode<T>* pushFront(const T& val) {
        DLLNode<T>* node = new DLLNode<T>(val);
        node->next = head_;
        node->prev = nullptr;
        if (head_) head_->prev = node;
        head_ = node;
        if (!tail_) tail_ = node;
        ++size_;
        return node;
    }

    void moveToFront(DLLNode<T>* node) {
        if (node == head_) return;
        unlink(node);
        node->prev = nullptr;
        node->next = head_;
        if (head_) head_->prev = node;
        head_ = node;
        if (!tail_) tail_ = node;
    }

    T popTail() {
        if (!tail_) {
            T def{};
            return def;
        }
        DLLNode<T>* node = tail_;
        T val = node->data;
        unlink(node);
        delete node;
        return val;
    }

    void remove(DLLNode<T>* node) {
        unlink(node);
        delete node;
    }

    DLLNode<T>* head() const { return head_; }
    DLLNode<T>* tail() const { return tail_; }
    int size()         const { return size_; }
    bool empty()       const { return size_ == 0; }

private:
    void unlink(DLLNode<T>* node) {
        if (node->prev) node->prev->next = node->next;
        else            head_ = node->next;
        if (node->next) node->next->prev = node->prev;
        else            tail_ = node->prev;
        node->prev = node->next = nullptr;
        --size_;
    }

    DLLNode<T>* head_;
    DLLNode<T>* tail_;
    int          size_;
};
