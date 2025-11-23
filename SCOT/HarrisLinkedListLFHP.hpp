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

#ifndef _TIM_HARRIS_LINKED_LIST_LF_HP_H_
#define _TIM_HARRIS_LINKED_LIST_LF_HP_H_

#include <atomic>
#include <thread>
#include <forward_list>
#include <set>
#include <iostream>
#include <string>
#include "HazardPointers.hpp"

template<typename T, size_t N = 1>
class HarrisLinkedListLFHP {

private:
    struct Node {
        T* key;
        std::atomic<Node*> next;

        Node(T* key) : key{key}, next{nullptr} {}
    };

    alignas(128) std::atomic<Node*> head;

    const int maxThreads;

    // We need one extra hazard pointer
    HazardPointers<Node> hp {4, maxThreads};
    const int kHp0 = 0; // next
    const int kHp1 = 1; // curr
    const int kHp2 = 2; // the first unsafe node
    const int kHp3 = 3; // the last safe node (prev)

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
    HarrisLinkedListLFHP(const int maxThreads) : maxThreads{maxThreads} {
        head.store(new Node(nullptr)); // sentinel node
    }

    ~HarrisLinkedListLFHP() { }

    std::string className() { return "HarrisLinkedListHP"; }

    bool insert(T* key, const int tid)
    {
        std::atomic<Node*> *prev;
        Node *curr, *next, *node = new Node(key);
        while (true) {
            if (find(key, &prev, &curr, &next, tid)) {
                delete node;
                hp.clear(tid);
                return false;
            }
            node->next.store(curr, std::memory_order_relaxed);
            Node *tmp = curr;
            if (prev->compare_exchange_strong(tmp, node)) {
                hp.clear(tid);
                return true;
            }
        }
    }

    bool remove(T *key, const int tid)
    {
        std::atomic<Node*> *prev;
        Node *curr, *next;
        hp.take_snapshot(tid);
        while (true) {
            if (!find(key, &prev, &curr, &next, tid)) {
                hp.clear(tid);
                return false;
            }
            Node *tmp = next;
            if (!curr->next.compare_exchange_strong(tmp, markPtr(next)))
                continue;
            tmp = curr;
            if (prev->compare_exchange_strong(tmp, unmarkPtr(next))) {
                hp.clear(tid);
                hp.retire(curr, tid);
            } else {
                hp.clear(tid);
            }
            return true;
        }
    }

    bool search(T *key, const int tid)
    {
        std::atomic<Node*> *prev;
        Node *curr, *next, *tmp;

again:
        prev = &head;
        curr = hp.protect(kHp1, *prev, tid);
        next = hp.protect(kHp0, curr->next, tid);

        while (true)
        {
            do {
                if (curr->key != nullptr && !(*curr->key < *key)) goto done;
                prev = &curr->next;
                hp.protectPtrRelease(kHp3, curr, tid);
                curr = unmarkPtr(next);
                if (curr == nullptr) goto done;
                hp.protectPtrRelease(kHp1, curr, tid);
                next = hp.protect(kHp0, curr->next, tid);
            } while (!checkPtrMarked(next));
            Node *prev_next = hp.protectPtrRelease(kHp2, curr, tid);
            do {
                curr = unmarkPtr(next);
                if (curr == nullptr) goto done;
                hp.protectPtrRelease(kHp1, curr, tid);
                next = hp.protect(kHp0, curr->next, tid);
                if ((tmp = prev->load()) != prev_next) {
                    // An optimized version of protect()
                    prev_next = tmp;
                    do {
                        if (checkPtrMarked(prev_next)) goto again;
                        // Use kHp2 here instead of kHp1 to allow
                        // both prev_next and curr protection and ability
                        // to continue with either of do-while loops
                        curr = hp.protectPtr(kHp2, prev_next, tid);
                    } while ((prev_next = prev->load()) != curr);
                    if (curr == nullptr) goto done;
                    next = hp.protect(kHp0, curr->next, tid);
                }
            } while (checkPtrMarked(next));
        }

done:
        bool ret = (curr && curr->key != nullptr && *curr->key == *key);
        hp.clear(tid);
        return ret;
    }

    long long calculate_space(const int tid)
    {
        return hp.cal_space(sizeof(Node), tid);
    }

private:
    bool find(T *key, std::atomic<Node*> **pprev, Node **pcurr, Node **pnext, const int tid)
    {
        std::atomic<Node*> *prev;
        Node *curr, *next, *prev_next, *tmp;

again:
        prev_next = nullptr;
        prev = &head;
        curr = hp.protect(kHp1, *prev, tid);
        next = hp.protect(kHp0, curr->next, tid);

        while (true)
        {
            do {
                if (curr->key != nullptr && !(*curr->key < *key)) goto cleanup;
                prev_next = nullptr;
                prev = &curr->next;
                hp.protectPtrRelease(kHp3, curr, tid);
                curr = unmarkPtr(next);
                if (curr == nullptr) goto done;
                hp.protectPtrRelease(kHp1, curr, tid);
                next = hp.protect(kHp0, curr->next, tid);
            } while (!checkPtrMarked(next));
            prev_next = hp.protectPtrRelease(kHp2, curr, tid);
            do {
                curr = unmarkPtr(next);
                if (curr == nullptr) goto cleanup;
                hp.protectPtrRelease(kHp1, curr, tid);
                next = hp.protect(kHp0, curr->next, tid);
                if ((tmp = prev->load()) != prev_next) {
                    // An optimized version of protect()
                    prev_next = tmp;
local_recovery:     do {
                        if (checkPtrMarked(prev_next)) goto again;
                        // Use kHp2 here instead of kHp1 to allow
                        // both prev_next and curr protection and ability
                        // to continue with either of do-while loops
                        curr = hp.protectPtr(kHp2, prev_next, tid);
                    } while ((prev_next = prev->load()) != curr);
                    if (curr == nullptr) goto done;
                    next = hp.protect(kHp0, curr->next, tid);
                }
            } while (checkPtrMarked(next));
        }

cleanup:
        // Some nodes in between
        if (prev_next != nullptr && prev_next != curr) {
            if (!prev->compare_exchange_strong(prev_next, curr)) {
                goto local_recovery;
            }
            // Retire nodes
            do {
                Node *tmp = unmarkPtr(prev_next->next.load(std::memory_order_relaxed));
                hp.retire(prev_next, tid);
                prev_next = tmp;
            } while (prev_next != curr);
        }

done:
        *pcurr = curr;
        *pprev = prev;
        *pnext = next;
        return (curr && curr->key != nullptr && *curr->key == *key);
    }
};

#endif /* _TIM_HARRIS_LINKED_LIST_LF_HP_H_ */
