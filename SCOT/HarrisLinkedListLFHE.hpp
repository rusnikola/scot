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

#ifndef _TIM_HARRIS_LINKED_LIST_LF_HE_H_
#define _TIM_HARRIS_LINKED_LIST_LF_HE_H_

#include <atomic>
#include <thread>
#include <forward_list>
#include <set>
#include <iostream>
#include <string>
#include "HazardEras.hpp"

template<typename T, size_t N = 1>
class HarrisLinkedListLFHE {

private:

    struct Node : HENode {
        T* key;
        std::atomic<Node*> next;

        Node(T* key) : key{key}, next{nullptr}  { }
    };


    alignas(128) std::atomic<Node*> head;

    const int maxThreads;

    // We need 4 hazard eras
    HazardEras<Node> he {4, maxThreads};
    const int kHe0 = 0; // next
    const int kHe1 = 1; // curr
    const int kHe2 = 2; // the first unsafe node
    const int kHe3 = 3; // the last safe node (prev)

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

    HarrisLinkedListLFHE(const int maxThreads) : maxThreads{maxThreads} {
        head.store(he.init_object(new Node(nullptr), 0)); // sentinel node
    }

    ~HarrisLinkedListLFHE() { }

    std::string className() { return "HarrisLinkedListHE"; }

    bool insert(T* key, const int tid)
    {
        Node *curr, *next;
        std::atomic<Node*> *prev;
        Node *newNode = he.init_object(new Node(key), tid);
        while (true) {
            if (find(key, &prev, &curr, &next, tid)) {
                delete newNode;              // There is already a matching key
                he.clear(tid);
                return false;
            }
            newNode->next.store(curr, std::memory_order_relaxed);
            Node *tmp = curr;
            if (prev->compare_exchange_strong(tmp, newNode)) { // seq-cst
                he.clear(tid);
                return true;
            }
        }
    }

    bool remove(T* key, const int tid)
    {
        Node *curr, *next;
        std::atomic<Node*> *prev;
        he.take_snapshot(tid);
        while (true) {
            /* Try to find the key in the list. */
            if (!find(key, &prev, &curr, &next, tid)) {
                he.clear(tid);
                return false;
            }
            /* Mark if needed. */
            Node *tmp = next;
            if (!curr->next.compare_exchange_strong(tmp, markPtr(next))) {
                continue; /* Another thread interfered. */
            }

            tmp = curr;
            if (prev->compare_exchange_strong(tmp, unmarkPtr(next))) { /* Unlink */
                he.clear(tid);
                he.retire(unmarkPtr(curr), tid); /* Reclaim */
            } else {
                he.clear(tid);
            }

            return true;
        }
    }

    bool search(T *key, const int tid)
    {
        std::atomic<Node*> *prev;
        Node *curr, *next;

again:
        prev = &head;
        curr = he.protect(kHe1, *prev, tid);
        next = he.protect(kHe0, curr->next, tid);

        while (true)
        {
            do {
                if (curr->key != nullptr && !(*curr->key < *key)) goto done;
                prev = &curr->next;
                he.protectEraRelease(kHe3, kHe1, tid);
                curr = unmarkPtr(next);
                if (curr == nullptr) goto done;
                he.protectEraRelease(kHe1, kHe0, tid);
                next = he.protect(kHe0, curr->next, tid);
            } while (!checkPtrMarked(next));
            Node *prev_next = curr;
            he.protectEraRelease(kHe2, kHe1, tid);
            while (true) {
                curr = unmarkPtr(next);
                if (curr == nullptr) goto done;
                he.protectEraRelease(kHe1, kHe0, tid);
                next = he.protect(kHe0, curr->next, tid);
                if (prev->load() != prev_next) {
                    curr = he.protect(kHe1, *prev, tid);
                    if (checkPtrMarked(curr)) goto again;
                    if (curr == nullptr) goto done;
                    next = he.protect(kHe0, curr->next, tid);
                    if (!checkPtrMarked(next)) break;
                    prev_next = curr;
                    he.protectEraRelease(kHe2, kHe1, tid);
                    continue;
                }
                if (!checkPtrMarked(next)) break;
            }
        }

done:
        bool ret = (curr && curr->key != nullptr && *curr->key == *key);
        he.clear(tid);
        return ret;
    }

    long long calculate_space(const int tid){
        return he.cal_space(sizeof(Node), tid);
    }


private:
    bool find(T *key, std::atomic<Node*> **pprev, Node **pcurr, Node **pnext, const int tid)
    {
        std::atomic<Node*> *prev;
        Node *curr, *next, *prev_next;

again:
        prev_next = nullptr;
        prev = &head;
        curr = he.protect(kHe1, *prev, tid);
        next = he.protect(kHe0, curr->next, tid);

        while (true)
        {
            do {
                if (curr->key != nullptr && !(*curr->key < *key)) goto cleanup;
                prev_next = nullptr;
                prev = &curr->next;
                he.protectEraRelease(kHe3, kHe1, tid);
                curr = unmarkPtr(next);
                if (curr == nullptr) goto done;
                he.protectEraRelease(kHe1, kHe0, tid);
                next = he.protect(kHe0, curr->next, tid);
            } while (!checkPtrMarked(next));
            prev_next = curr;
            he.protectEraRelease(kHe2, kHe1, tid);
            while (true) {
                curr = unmarkPtr(next);
                if (curr == nullptr) goto cleanup;
                he.protectEraRelease(kHe1, kHe0, tid);
                next = he.protect(kHe0, curr->next, tid);
                if (prev->load() != prev_next) {
local_recovery:     curr = he.protect(kHe1, *prev, tid);
                    if (checkPtrMarked(curr)) goto again;
                    if (curr == nullptr) goto done;
                    prev_next = curr;
                    next = he.protect(kHe0, curr->next, tid);
                    if (!checkPtrMarked(next)) break;
                    he.protectEraRelease(kHe2, kHe1, tid);
                    continue;
                }
                if (!checkPtrMarked(next)) break;
            }
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
                he.retire(prev_next, tid);
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

#endif /* _TIM_HARRIS_LINKED_LIST_LF_HE_H_ */
