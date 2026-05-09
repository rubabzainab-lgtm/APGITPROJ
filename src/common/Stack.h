#pragma once
#include <cstddef>
#include <stdexcept>

template <typename T>
class Stack {
public:
    explicit Stack(int capacity = 1024)
        : capacity_(capacity), top_(-1) {
        data_ = new T[capacity_];
    }

    ~Stack() { delete[] data_; }

    void push(const T& val) {
        if (top_ + 1 >= capacity_) resize();
        data_[++top_] = val;
    }

    T pop() {
        if (isEmpty()) throw std::underflow_error("Stack underflow");
        return data_[top_--];
    }

    T& peek() {
        if (isEmpty()) throw std::underflow_error("Stack empty");
        return data_[top_];
    }

    const T& peek() const {
        if (isEmpty()) throw std::underflow_error("Stack empty");
        return data_[top_];
    }

    bool isEmpty() const { return top_ < 0; }
    int  size()    const { return top_ + 1; }
    void clear()         { top_ = -1; }

private:
    void resize() {
        int newCap = capacity_ * 2;
        T* newData = new T[newCap];
        for (int i = 0; i <= top_; ++i) newData[i] = data_[i];
        delete[] data_;
        data_ = newData;
        capacity_ = newCap;
    }

    T*  data_;
    int capacity_;
    int top_;
};
