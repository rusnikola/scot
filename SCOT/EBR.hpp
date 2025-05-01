/******************************************************************************
* Copyright (c) 2016-2017, Pedro Ramalhete, Andreia Correia
* Copyright (c) 2024-2025, Md Amit Hasan Arovi, Ruslan Nikolaev 
* All rights reserved.
*
* Based on the original URCUGraceVersion.hpp but has been fully re-written
* by MD Amit Hasan Arovi and Ruslan Nikolaev to avoid spin locks using
* the idea presented in the Interval Based Reclamation paper (PPoPP 2018).
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

#ifndef _URCU_GRACE_VERSION_H_
#define _URCU_GRACE_VERSION_H_

#include <atomic>
#include <malloc.h>
#include <cstdlib>

struct EBRNode {
    struct EBRNode *smr_next;
    size_t retired_epoch;
};

class EBR {
private:
    typedef struct retired_node_controller {
        EBRNode *head;
        EBRNode *tail;
        size_t epoch_counter;
        size_t list_counter;
        std::atomic<size_t> readerVersion;
        ssize_t sum;
        size_t count;
        ssize_t space;
        alignas(128) char pad[0];
    } retired_node_controller_t;

    static const size_t NOT_READING = 0xFFFFFFFFFFFFFFFE;
    static const size_t UNASSIGNED = 0xFFFFFFFFFFFFFFFD;
    static const size_t epoch_freq = 12;
    static const size_t empty_freq = 128;

    alignas(128) std::atomic<size_t> updaterVersion{0};
    alignas(128) char pad[0];

    const int maxThreads;

    retired_node_controller_t* rnc;

public:
    EBR(const int _maxThreads) : maxThreads{_maxThreads} {
        rnc = static_cast<retired_node_controller_t*>(aligned_alloc(128, sizeof(retired_node_controller_t) * maxThreads));
        if (rnc == nullptr) {
            std::cerr << "Error: Failed to allocate memory for retired_node_controller_t array\n";
            exit(1);
        }
        for (int i = 0; i < maxThreads; i++) {
            rnc[i].head = nullptr;
            rnc[i].tail = nullptr;
            rnc[i].epoch_counter = 0;
            rnc[i].list_counter = 0;
            rnc[i].readerVersion.store(UNASSIGNED, std::memory_order_relaxed);
            rnc[i].sum = 0;
            rnc[i].count = 0;
            rnc[i].space = 0;
        }
     }

    ~EBR() {
        for (int tid = 0; tid < maxThreads; tid++) {
            EBRNode* current_head = rnc[tid].head;
            while (current_head != nullptr) {
                EBRNode* smr_next = current_head->smr_next;
                delete current_head;
                current_head = smr_next;
            }
            rnc[tid].head = nullptr;
            rnc[tid].tail = nullptr;
        }
        free(rnc);
    }

    int register_thread() {
        for (int i = 0; i < maxThreads; i++) {
            if (rnc[i].readerVersion.load() != UNASSIGNED) continue;
            uint64_t curr = UNASSIGNED;
            if (rnc[i].readerVersion.compare_exchange_strong(curr, NOT_READING)) {
                return i;
            }
        }
        std::cerr << "Error: too many threads already registered\n";
        return -1;
    }

    void unregister_thread(int tid) {
        if (rnc[tid].readerVersion.load() == UNASSIGNED) {
            std::cerr << "Error: calling unregister_thread() with a tid that was never registered\n";
            return;
        }
        rnc[tid].readerVersion.store(UNASSIGNED);
    }

    void start_op(const int tid) noexcept {
        const uint64_t rv = updaterVersion.load();
        rnc[tid].readerVersion.store(rv);
    }

    void end_op(const int tid) noexcept {
        rnc[tid].readerVersion.store(NOT_READING, std::memory_order_release);
    }

    void retire(EBRNode* node, const int tid)
    {
        rnc[tid].space++;
        node->retired_epoch = updaterVersion.load();
        node->smr_next = nullptr;
        if (!rnc[tid].head) {
            rnc[tid].head = node;
        } else {
            rnc[tid].tail->smr_next = node;
        }
        rnc[tid].tail = node;
        rnc[tid].epoch_counter++;
        if (rnc[tid].epoch_counter % (epoch_freq * maxThreads) == 0) {
            updaterVersion.fetch_add(1, std::memory_order_acq_rel);
        }
        rnc[tid].list_counter++;
        if (rnc[tid].list_counter % empty_freq == 0) {
            try_empty_list(tid);
        }
    }

    void try_empty_list(const int tid)
    {
        size_t max_safe_epoch = rnc[0].readerVersion;
        for (size_t i = 1; i < maxThreads; i++) {
            size_t epoch = rnc[i].readerVersion;
            if (epoch < max_safe_epoch)
                max_safe_epoch = epoch;
        }
        EBRNode* current_head = rnc[tid].head;
        while (current_head != nullptr) {
            if (current_head->retired_epoch >= max_safe_epoch) {
                rnc[tid].head = current_head;
                return;
            }
            EBRNode* smr_next = current_head->smr_next;
            rnc[tid].space--;
            delete current_head;
            current_head = smr_next;
            rnc[tid].list_counter--;
        }
        rnc[tid].head = nullptr;
        rnc[tid].tail = nullptr;
    }

    inline void take_snapshot(const int tid)
    {
        rnc[tid].sum += rnc[tid].space;
        rnc[tid].count++;
    }

    inline long long cal_space(size_t size, const int tid)
    {
        return (long long) (rnc[tid].sum) / ((ssize_t) rnc[tid].count);
    }
};

#endif
