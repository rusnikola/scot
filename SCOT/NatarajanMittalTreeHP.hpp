/*
 * Copyright (c) 2024-2025, Md Amit Hasan Arovi, Ruslan Nikolaev
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _NATARAJAN_MITTAL_TREE_HP
#define _NATARAJAN_MITTAL_TREE_HP

#include <atomic>
#include <thread>
#include <iostream>
#include <string>
#include <vector>
#include <climits>
#include "HazardPointers.hpp"

template<typename T, size_t N = 1> 
class NatarajanMittalTreeHP {
private:
    struct Node {
        const T *key;
        std::atomic<Node*> left;
        std::atomic<Node*> right;

        Node(const T *k, Node *l, Node *r) : key(k), left(l), right(r) {};
    };

    struct SeekRecord{
        Node *ancestor;
        Node *successor;
        Node *parent;
        Node *leaf;
        alignas(128) char pad[0];
    };

    const int maxThreads;

    Node* R;
    Node* S;

    SeekRecord* records;

    // We need 5 hazard pointers
    HazardPointers<Node> hp {5, maxThreads};
    const int kHp0 = 0; // current
    const int kHp1 = 1; // leaf
    const int kHp2 = 2; // parent
    const int kHp3 = 3; // successor
    const int kHp4 = 4; // ancestor

    #define NT_TAG 1UL
    #define NT_FLG 2UL
    #define NT_KEY_NULL ((const T *) nullptr)

    static inline Node *unmarkPtr(Node *n) {
        return (Node *) ((size_t) n & ~(NT_FLG | NT_TAG));
    }

    static inline Node *markPtr(Node *n, size_t flags) {
        return (Node *) ((size_t) n | flags);
    }

    static inline size_t checkPtr(Node *n, size_t flags) {
        return (size_t) n & flags;
    }

    static inline bool keyIsLess(const T *k1, const T *k2) {
        return (k2 == NT_KEY_NULL) || (*k1 < *k2);
    }

    static inline bool keyIsEqual(const T *k1, const T *k2) {
        return (k2 != NT_KEY_NULL) && (*k1 == *k2);
    }

public:
    NatarajanMittalTreeHP(const int maxThreads) : maxThreads{maxThreads} {
        R = new Node(NT_KEY_NULL, nullptr, nullptr);
        S = new Node(NT_KEY_NULL, nullptr, nullptr);
        R->right.store(new Node(NT_KEY_NULL, nullptr, nullptr));
        R->left.store(S);
        S->right.store(new Node(NT_KEY_NULL, nullptr, nullptr));
        S->left.store(new Node(NT_KEY_NULL, nullptr, nullptr));

        records = new SeekRecord[maxThreads]{};
    }
    
    ~NatarajanMittalTreeHP() {
        delete[] records;
    }

    std::string className() { return "NatarajanMittalTreeHP"; }

    void seek(const T *key, const int tid)
    {
        SeekRecord *seekRecord = &records[tid];

again:
        seekRecord->ancestor = R;
        seekRecord->parent = hp.protect(kHp2, R->left, tid);
        seekRecord->successor = seekRecord->parent;
        std::atomic<Node*> *successorAddr = &R->left;
        hp.protectPtrRelease(kHp3, seekRecord->parent, tid);
        Node *parentField = hp.protect(kHp1, S->left, tid);
        seekRecord->leaf = unmarkPtr(parentField);
        std::atomic<Node*> *leafAddr = &S->left;

        Node *currentField = hp.protect(kHp0, seekRecord->leaf->left, tid);
        Node *current = unmarkPtr(currentField);
        std::atomic<Node*>* currentAddr = &seekRecord->leaf->left;

        while (current != nullptr) {
            if (!checkPtr(parentField, NT_TAG)) {
                seekRecord->ancestor = seekRecord->parent;
                hp.protectPtrRelease(kHp4, seekRecord->parent, tid);
                seekRecord->successor = seekRecord->leaf;
                hp.protectPtrRelease(kHp3, seekRecord->leaf, tid);
                successorAddr = leafAddr;
            }

            if (checkPtr(currentField, NT_TAG | NT_FLG)) {
                if (successorAddr->load() != seekRecord->successor) {
                    goto again;
                }
            }

            seekRecord->parent = seekRecord->leaf;
            hp.protectPtrRelease(kHp2, seekRecord->leaf, tid);
            seekRecord->leaf = current;
            leafAddr = currentAddr;
            hp.protectPtrRelease(kHp1, current, tid);
            parentField = currentField;

            if (keyIsLess(key, current->key)) {
                currentField = hp.protect(kHp0, current->left, tid);
                currentAddr = &current->left;
            } else {
                currentField = hp.protect(kHp0, current->right, tid);
                currentAddr = &current->right;
            }
            current = unmarkPtr(currentField);
        }
    }

    bool search (const T *key, const int tid)
    {
        SeekRecord* seekRecord = &records[tid];
        seek(key, tid);
        bool isContains = keyIsEqual(key, seekRecord->leaf->key);
        hp.clear(tid);
        return isContains;
    }

    long long calculate_space(const int tid)
    {
        return hp.cal_space(sizeof(Node), tid);
    }

    bool cleanup(const T *key, const int tid)
    {
        SeekRecord* seekRecord = &records[tid];
        Node* ancestor = seekRecord->ancestor;
        Node* successor = seekRecord->successor;
        Node* parent = seekRecord->parent;
        Node* leaf = seekRecord->leaf;

        std::atomic<Node*> *successorAddr =
            keyIsLess(key, ancestor->key) ? &ancestor->left : &ancestor->right;

        std::atomic<Node*> *childAddr, *siblingAddr;
        if (keyIsLess(key, parent->key)) {
            childAddr = &parent->left;
            siblingAddr = &parent->right;
        } else {
            childAddr = &parent->right;
            siblingAddr = &parent->left;
        }

        Node *child = childAddr->load();
        if (!checkPtr(child, NT_FLG)) {
            child = siblingAddr->load();
            siblingAddr = childAddr;
        }

        // tag the sibling edge
        std::atomic<size_t> *_siblingAddr = (std::atomic<size_t> *) siblingAddr;
        Node *node = (Node *) (_siblingAddr->fetch_or(NT_TAG) & (~NT_TAG));
        // the previous value is untagged if necessary
        bool ret = successorAddr->compare_exchange_strong(successor, node);
        // reclaim the deleted edge
        if (ret) {
            while (successor != parent) {
                Node *left = successor->left;
                Node *right = successor->right;
                hp.retire(successor, tid);
                if (checkPtr(left, NT_FLG)) {
                    hp.retire(unmarkPtr(left), tid);
                    successor = unmarkPtr(right);
                } else {
                    hp.retire(unmarkPtr(right), tid);
                    successor = unmarkPtr(left);
                }
            }
            hp.retire(unmarkPtr(child), tid);
            hp.retire(successor, tid);
        }
        return ret;
    }

    bool insert(const T *key, const int tid)
    {
        SeekRecord *seekRecord = &records[tid];
        bool ret = false;

        Node *newLeaf = new Node(key, nullptr, nullptr);

        while (true) {
            seek(key, tid);
            Node *leaf = seekRecord->leaf;
            Node *parent = seekRecord->parent;
            if (!keyIsEqual(key, leaf->key)) {
                std::atomic<Node*> *childAddr = keyIsLess(key, parent->key) ?
                                &parent->left : &parent->right;

                Node *newLeft, *newRight;
                if (keyIsLess(key, leaf->key)) {
                    newLeft = newLeaf;
                    newRight = leaf;
                } else {
                    newLeft = leaf;
                    newRight = newLeaf;
                }

                const T *newKey = leaf->key;
                if (newKey != NT_KEY_NULL && *newKey < *key) {
                    newKey = key;
                }
                Node *newInternal = new Node(newKey, newLeft, newRight);

                Node* tmpOld = leaf;
                if (childAddr->compare_exchange_strong(tmpOld, newInternal)) {
                    ret = true;
                    break;
                } else {
                    delete newInternal;
                    Node* child = childAddr->load();
                    if (unmarkPtr(child) == leaf && checkPtr(child, NT_TAG | NT_FLG)) {
                        cleanup(key, tid);
                    }
                }
            }
            else {
                delete newLeaf;
                ret = false;
                break;
            }
        }
        hp.clear(tid);
        return ret;
    }

    bool remove(const T *key, const int tid)
    {
        SeekRecord* seekRecord = &records[tid];
        Node *leaf = nullptr; // injection

        hp.take_snapshot(tid);
        while (true) {
            seek(key, tid);
            Node *parent = seekRecord->parent;
            std::atomic<Node*>* childAddr = keyIsLess(key, parent->key) ?
                            &parent->left : &parent->right;

            if (!leaf) { // injection
                leaf = seekRecord->leaf;

                if (!keyIsEqual(key, leaf->key)) {
                    hp.clear(tid);
                    return false;
                }

                Node *tmpOld = leaf;
                if (childAddr->compare_exchange_strong(tmpOld, markPtr(tmpOld, NT_FLG))) {
                    if (cleanup(key, tid)) {
                        hp.clear(tid);
                        return true;
                    }
                } else {
                    Node *child = childAddr->load();
                    if (unmarkPtr(child) == leaf && checkPtr(child, NT_TAG | NT_FLG)) {
                        cleanup(key, tid);
                    }
                    leaf = nullptr; // failed: reset injection
                }
            } else {
                if (seekRecord->leaf != leaf) {
                    hp.clear(tid);
                    return true;
                } else {
                    if (cleanup(key, tid)) {
                        hp.clear(tid);
                        return true;
                    }
                }
            }
        }
    }
};
#endif 
