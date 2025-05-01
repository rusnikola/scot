/******************************************************************************
 * Copyright (c) 2014-2016, Pedro Ramalhete, Andreia Correia
 * Copyright (c) 2024-2025, Md Amit Hasan Arovi, Ruslan Nikolaev
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Concurrency Freaks nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************
 */

#ifndef _TIM_HARRIS_MAGED_MICHAEL_LINKED_LIST_NO_MEMORY_RECLAMATION_H_
#define _TIM_HARRIS_MAGED_MICHAEL_LINKED_LIST_NO_MEMORY_RECLAMATION_H_

#include <atomic>
#include <thread>
#include <forward_list>
#include <set>
#include <iostream>
#include <string>



/**
 * This is the linked list by Maged M. Michael without any memory reclamation.
 * It's meant to be used as a baseline for benchmarks.
 * Lock-Free Linked List as described in Maged M. Michael paper (Figure 4):
 * http://www.cs.tau.ac.il/~afek/p73-Lock-Free-HashTbls-michael.pdf
 *
 * 
 * <p>
 * This set has three operations:
 * <ul>
 * <li>add(x)      - Lock-Free
 * <li>remove(x)   - Lock-Free
 * <li>contains(x) - Lock-Free
 * </ul><p>
 * <p>
 * @author Pedro Ramalhete
 * @author Andreia Correia
 */
template<typename T, size_t N = 1> 
class HarrisMichaelLinkedListNR {

private:
    struct Node {
        T* key;
        std::atomic<Node*> next;

        Node(T* key) : key{key}, next{nullptr} { }

        bool casNext(Node *cmp, Node *val) {
            return next.compare_exchange_strong(cmp, val);
        }
    };

    // Pointer to head sentinel node of the list
    alignas(128) std::atomic<Node*> head;

    const int maxThreads;

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

    HarrisMichaelLinkedListNR(const int maxThreads) : maxThreads{maxThreads} {
        head.store(new Node(nullptr));
    }


    // We don't expect the destructor to be called if this instance can still be in use
    ~HarrisMichaelLinkedListNR() {
    }

    std::string className() { return "HarrisMichaelLinkedListNR"; }


    /**
     * This method is named 'Insert()' in the original paper.
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     * <p>
     * Progress Condition: Lock-Free
     *
     */
    bool insert(T* key, const int tid)
    {
        Node *curr, *next;
        std::atomic<Node*> *prev;
        Node* newNode = new Node(key);
        while (true) {
            if (find(key, &prev, &curr, &next, tid)) {
                delete newNode;              // There is already a matching key
                return false;
            }
            newNode->next.store(curr, std::memory_order_relaxed);
            Node *tmp = curr;
            if (prev->compare_exchange_strong(tmp, newNode)) { // seq-cst
                return true;
            }
        }
    }


    /**
     * This method is named 'Delete()' in the original paper.
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     */
    bool remove(T* key, const int tid)
    {
        Node *curr, *next;
        std::atomic<Node*> *prev;
        while (true) {
            /* Try to find the key in the list. */
            if (!find(key, &prev, &curr, &next, tid)) {
                return false;
            }
            
            /* Mark if needed. */
            Node *tmp = next;
            if (!curr->next.compare_exchange_strong(tmp, markPtr(next))) {
                continue; /* Another thread interfered. */
            }

            tmp = curr;
            prev->compare_exchange_strong(tmp, next); /* Unlink */
            /*
             * If we want to prevent the possibility of there being an
             * unbounded number of unmarked nodes, add "else _find(head,key)."
             * This is not necessary for correctness.
             */
            return true;
        }
    }


    /**
     * This is named 'Search()' on the original paper
     * Taken from Figure 7 of the paper:
     * "High Performance Dynamic Lock-Free Hash Tables and List-Based Sets"
     * <p>
     * Progress Condition: Lock-Free
     */
    bool search (T* key, const int tid)
    {
        Node *curr, *next;
        std::atomic<Node*> *prev;
        bool isContains = find(key, &prev, &curr, &next, tid);
        return isContains;
    }
    
     long long calculate_space(const int tid){
        return 0;
    }


private:

    /**
     * TODO: This needs to be code reviewed... it's not production-ready
     * <p>
     * Progress Condition: Lock-Free
     */
    bool find (T* key, std::atomic<Node*> **par_prev, Node **par_curr, Node **par_next, const int tid)
    {
        std::atomic<Node*> *prev;
        Node *curr, *next;

try_again:
        prev = &head;
        curr = prev->load();
        while (true) {
            // Protect next with a hazard pointer.
            if (curr == nullptr) break; // 'next' is not properly initialized in this case
            next = curr->next.load();
            if (prev->load() != curr) goto try_again;
            if (unmarkPtr(next) == next) { // !cmark in the paper
                if (curr->key != nullptr && !(*curr->key < *key)) { // Check for null to handle head
                    *par_curr = curr;
                    *par_prev = prev;
                    *par_next = next;
                    return (*curr->key == *key);
                }
                prev = &curr->next;
            } else {
                // Update the link and retire the node.
                Node *tmp = curr;
                next = unmarkPtr(next);
                if (!prev->compare_exchange_strong(tmp, next)) {
                    goto try_again;
                }
            }
            curr = next;
        }
        *par_curr = curr;
        *par_prev = prev;
        *par_next = next;
        return false;
    }
};

#endif /* _TIM_HARRIS_MAGED_MICHAEL_LINKED_LIST_NO_MEMORY_RECLAMATION_H_ */
