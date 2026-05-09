#pragma once
#include <cstring>
#include <functional>

// Generic AVL Tree (auto-balancing BST) keyed on int with associated value T.
template <typename T>
struct AVLNode {
    int       key;
    T         value;
    int       height;
    AVLNode*  left;
    AVLNode*  right;
    AVLNode(int k, const T& v) : key(k), value(v), height(1), left(nullptr), right(nullptr) {}
};

template <typename T>
class AVLTree {
public:
    AVLTree() : root_(nullptr), size_(0) {}
    ~AVLTree() { destroy(root_); }

    void insert(int key, const T& val) {
        root_ = insert(root_, key, val);
    }

    bool search(int key, T& outVal) const {
        AVLNode<T>* node = find(root_, key);
        if (!node) return false;
        outVal = node->value;
        return true;
    }

    bool contains(int key) const { return find(root_, key) != nullptr; }

    void remove(int key) { root_ = remove(root_, key); }

    // In-order traversal via callback
    template <typename Fn>
    void inOrder(Fn fn) const { inOrder(root_, fn); }

    // Range query: all keys in [lo, hi]
    template <typename Fn>
    void rangeQuery(int lo, int hi, Fn fn) const { rangeQuery(root_, lo, hi, fn); }

    int  size()  const { return size_; }
    bool empty() const { return size_ == 0; }
    int  height() const { return height(root_); }

private:
    AVLNode<T>* root_;
    int size_;

    int height(AVLNode<T>* n) const { return n ? n->height : 0; }

    int balanceFactor(AVLNode<T>* n) const {
        return n ? height(n->left) - height(n->right) : 0;
    }

    void updateHeight(AVLNode<T>* n) {
        if (!n) return;
        int lh = height(n->left);
        int rh = height(n->right);
        n->height = 1 + (lh > rh ? lh : rh);
    }

    AVLNode<T>* rotateRight(AVLNode<T>* y) {
        AVLNode<T>* x  = y->left;
        AVLNode<T>* T2 = x->right;
        x->right = y;
        y->left  = T2;
        updateHeight(y);
        updateHeight(x);
        return x;
    }

    AVLNode<T>* rotateLeft(AVLNode<T>* x) {
        AVLNode<T>* y  = x->right;
        AVLNode<T>* T2 = y->left;
        y->left  = x;
        x->right = T2;
        updateHeight(x);
        updateHeight(y);
        return y;
    }

    AVLNode<T>* balance(AVLNode<T>* n) {
        updateHeight(n);
        int bf = balanceFactor(n);
        if (bf > 1) {
            if (balanceFactor(n->left) < 0)
                n->left = rotateLeft(n->left);
            return rotateRight(n);
        }
        if (bf < -1) {
            if (balanceFactor(n->right) > 0)
                n->right = rotateRight(n->right);
            return rotateLeft(n);
        }
        return n;
    }

    AVLNode<T>* insert(AVLNode<T>* n, int key, const T& val) {
        if (!n) { ++size_; return new AVLNode<T>(key, val); }
        if (key < n->key)      n->left  = insert(n->left,  key, val);
        else if (key > n->key) n->right = insert(n->right, key, val);
        else                   { n->value = val; return n; }
        return balance(n);
    }

    AVLNode<T>* minNode(AVLNode<T>* n) const {
        while (n->left) n = n->left;
        return n;
    }

    AVLNode<T>* remove(AVLNode<T>* n, int key) {
        if (!n) return nullptr;
        if (key < n->key)      n->left  = remove(n->left,  key);
        else if (key > n->key) n->right = remove(n->right, key);
        else {
            --size_;
            if (!n->left || !n->right) {
                AVLNode<T>* child = n->left ? n->left : n->right;
                delete n;
                return child;
            }
            AVLNode<T>* successor = minNode(n->right);
            n->key   = successor->key;
            n->value = successor->value;
            ++size_;
            n->right = remove(n->right, successor->key);
        }
        return balance(n);
    }

    AVLNode<T>* find(AVLNode<T>* n, int key) const {
        if (!n) return nullptr;
        if (key == n->key) return n;
        if (key < n->key)  return find(n->left, key);
        return find(n->right, key);
    }

    template <typename Fn>
    void inOrder(AVLNode<T>* n, Fn fn) const {
        if (!n) return;
        inOrder(n->left, fn);
        fn(n->key, n->value);
        inOrder(n->right, fn);
    }

    template <typename Fn>
    void rangeQuery(AVLNode<T>* n, int lo, int hi, Fn fn) const {
        if (!n) return;
        if (n->key > lo) rangeQuery(n->left, lo, hi, fn);
        if (n->key >= lo && n->key <= hi) fn(n->key, n->value);
        if (n->key < hi) rangeQuery(n->right, lo, hi, fn);
    }

    void destroy(AVLNode<T>* n) {
        if (!n) return;
        destroy(n->left);
        destroy(n->right);
        delete n;
    }
};
