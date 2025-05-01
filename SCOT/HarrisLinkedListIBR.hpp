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

#ifndef _TIM_HARRIS_LINKED_LIST_IBR_H_
#define _TIM_HARRIS_LINKED_LIST_IBR_H_

#include <atomic>
#include <thread>
#include <forward_list>
#include <set>
#include <iostream>
#include <string>
#include "IBR.hpp"

template<typename T, size_t N = 1> 
class HarrisLinkedListIBR {

private:
    struct Node : IBRNode {
        T* key;
        std::atomic<Node*> next;
        
        Node(T* key) : key{key}, next{nullptr} { }
    };


    alignas(128) std::atomic<Node*> head;

    const int maxThreads;
    
    IBR<Node> ibr {maxThreads};

    static inline Node *markPtr(Node *node) {
        return (Node*)((size_t) node | 0x1UL);
    }

    static inline Node *unmarkPtr(Node *node) {
        return (Node*)((size_t) node & (~0x1UL));
    }

    static inline size_t checkPtrMarked(Node *node) {
        return ((size_t) node & 0x1UL);
    }

public:
    HarrisLinkedListIBR(const int maxThreads) : maxThreads{maxThreads} {
        head.store(ibr.init_object(new Node(nullptr), 0)); // sentinel node
    }

    ~HarrisLinkedListIBR() { }

    std::string className() { return "HarrisLinkedListIBR"; }

    bool insert(T *key, const int tid)
    {
        std::atomic<Node*> *prev;
        Node *curr, *next, *node = ibr.init_object(new Node(key), tid);
        ibr.start_op(tid);
        while (true) {
            if (find(key, &prev, &curr, &next, tid)) {
                delete node;
                ibr.end_op(tid);
                return false;
            }
            node->next.store(curr, std::memory_order_relaxed);
            Node *tmp = curr;
            if (prev->compare_exchange_strong(tmp, node)) {
                ibr.end_op(tid);
                return true;
            }
        }
    }

    bool remove(T *key, const int tid)
    {
        std::atomic<Node*> *prev;
        Node *curr, *next;
        ibr.start_op(tid);
        ibr.take_snapshot(tid);
        while (true) {
            if (!find(key, &prev, &curr, &next, tid)) {
                ibr.end_op(tid);
                return false;
            }
            Node *tmp = next;
            if (!curr->next.compare_exchange_strong(tmp, markPtr(next)))
                continue;
            tmp = curr;
            if (prev->compare_exchange_strong(tmp, unmarkPtr(next))) {
                ibr.end_op(tid);
                ibr.retire(curr, tid);
            } else {
                ibr.end_op(tid);
            }
            return true;
        }
    }

    bool search(T *key, const int tid)
    {
        std::atomic<Node*> *prev;
        Node *curr, *next, *prev_next;

        ibr.start_op(tid);
again:
        prev_next = nullptr;
        prev = &head;
        curr = ibr.protect(*prev, tid);

        while (true)
        {
            if (curr == nullptr) break;
            next = ibr.protect(curr->next, tid);
            if (!checkPtrMarked(next)) {
                if (curr->key != nullptr && !(*curr->key < *key)) break;
                prev = &curr->next;
                prev_next = next; // next is unmarked
            } else {
                if (prev->load() != prev_next) goto again;
            }
            curr = unmarkPtr(next);
        }
        bool ret = (curr && curr->key != nullptr && *curr->key == *key);
        ibr.end_op(tid);
        return ret;
    }

    long long calculate_space(const int tid)
    {
        return ibr.cal_space(sizeof(Node), tid);
    }

private:
    bool find(T *key, std::atomic<Node*> **pprev, Node **pcurr, Node **pnext, const int tid)
    {
        std::atomic<Node*> *prev;
        Node *curr, *next, *prev_next;

again:
        prev_next = nullptr;
        prev = &head;
        curr = ibr.protect(*prev, tid);

        while (true)
        {
            if (curr == nullptr) break;
            next = ibr.protect(curr->next, tid);
            if (!checkPtrMarked(next)) {
                if (curr->key != nullptr && !(*curr->key < *key)) break;
                prev = &curr->next;
                prev_next = next; // next is unmarked
            } else {
                if (prev->load() != prev_next) goto again;
            }
            curr = unmarkPtr(next);
        }

        // Some nodes in between
        if (prev_next != curr) {
            if (!prev->compare_exchange_strong(prev_next, curr)) goto again;
            // Retire nodes
            do {
                Node *tmp = unmarkPtr(prev_next->next.load(std::memory_order_relaxed));
                ibr.retire(prev_next, tid);
                prev_next = tmp;
            } while (prev_next != curr);
        }

        *pcurr = curr;
        *pprev = prev;
        *pnext = next;
        return (curr && curr->key != nullptr && *curr->key == *key); 
    }
};

#endif /* _TIM_HARRIS_LINKED_LIST_IBR_H_ */
