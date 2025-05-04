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

#ifndef _IBR_H_
#define _IBR_H_

#include <atomic>
#include <malloc.h>

struct IBRNode {
    struct IBRNode *smr_next;
    uint64_t birth_epoch;
    uint64_t retired_epoch;
};

template<typename T>
class IBR {
private:
    static const size_t epoch_freq = 12;
    static const size_t empty_freq = 128;

    alignas(128) std::atomic<uint64_t>  global_epoch {0};
    alignas(128) char pad[0];

    typedef struct ibr_reservation {
        alignas(128) std::atomic<uint64_t> low;
        std::atomic<uint64_t> high;
        alignas(128) char pad[0];
    } ibr_reservation_t;

    typedef struct ibr_private_data {
        IBRNode *first;
        size_t epoch_counter;
        size_t list_counter;
        ssize_t sum;
        size_t count;
        ssize_t space;
        alignas(128) char pad[0];        
    } ibr_private_data_t;

    const int maxThreads;
    ibr_reservation_t     *epoch;
    ibr_private_data_t    *thr;

public:
    IBR(const int _maxThreads) : maxThreads{_maxThreads}
    {
        thr = static_cast<ibr_private_data_t*>(aligned_alloc(128, sizeof(ibr_private_data_t) * maxThreads));
        if (thr == nullptr) {
            std::cerr << "Error: Failed to allocate memory for ibr_private_data_t array\n";
            exit(1);
        }
        epoch = static_cast<ibr_reservation_t*>(aligned_alloc(128, sizeof(ibr_reservation_t) * maxThreads));
        if (epoch == nullptr) {
            std::cerr << "Error: Failed to allocate memory for ibr_reservation_t array\n";
            exit(1);
        }
        for (int it = 0; it < maxThreads; it++) {
            epoch[it].low.store(UINT64_MAX, std::memory_order_relaxed);
            epoch[it].high.store(UINT64_MAX, std::memory_order_relaxed);
            thr[it].first = nullptr;
            thr[it].epoch_counter = 0;
            thr[it].list_counter = 0;
            thr[it].sum = 0;
            thr[it].count = 0;
            thr[it].space = 0;
        }
        static_assert(std::is_same<decltype(T::birth_epoch), uint64_t>::value, "T::birth_epoch must be uint64_t");
        static_assert(std::is_same<decltype(T::retired_epoch), uint64_t>::value, "T::retired_epoch must be uint64_t");
    }

    ~IBR()
    {
        for (int tid = 0; tid < maxThreads; tid++) {
            IBRNode *obj = thr[tid].first;
            while (obj != nullptr) {
                IBRNode *smr_next = obj->smr_next;
                thr[tid].space--;
                delete obj;
                obj = smr_next;
            }
        }
        free(thr);
        free(epoch);
    }

    inline T *init_object(T *obj, const int mytid)
    {
        thr[mytid].epoch_counter++;

        if (thr[mytid].epoch_counter % (epoch_freq * maxThreads) == 0) 
            global_epoch.fetch_add(1);

        obj->birth_epoch = global_epoch.load(std::memory_order_acquire);
        return obj;
    }

    inline void start_op(const int tid)
    {
        uint64_t era = global_epoch.load(std::memory_order_acquire);
        epoch[tid].low.store(era, std::memory_order_release);
        epoch[tid].high.store(era, std::memory_order_release);
    }

    inline void end_op(const int tid)
    {
        epoch[tid].low.store(UINT64_MAX, std::memory_order_release);
        epoch[tid].high.store(UINT64_MAX, std::memory_order_release);
    }

    inline T *protect(const std::atomic<T*> &atom, const int tid)
    {
        auto prevEra = epoch[tid].high.load(std::memory_order_relaxed);
        while (true) {
            T* ptr = atom.load();
            auto era = global_epoch.load(std::memory_order_acquire);
            if (era == prevEra) return ptr;
            epoch[tid].high.store(era);
            prevEra = era;
        }
    }

    void retire(T *ptr, const int mytid)
    {
        thr[mytid].space++;
        auto currEra = global_epoch.load(std::memory_order_acquire);
        ptr->retired_epoch = currEra;
        ptr->smr_next = thr[mytid].first;
        thr[mytid].first = ptr;
        thr[mytid].list_counter++;
        if (thr[mytid].list_counter % empty_freq != 0) return;

        uint64_t low[maxThreads], high[maxThreads];
        for (int tid = 0; tid < maxThreads; tid++) {
            low[tid] = epoch[tid].low.load(std::memory_order_acquire);
            high[tid] = epoch[tid].high.load(std::memory_order_acquire);
        }

        IBRNode **prev_p = &thr[mytid].first;
        IBRNode *obj = thr[mytid].first;
        while (obj != nullptr) {
            IBRNode *smr_next = obj->smr_next;
            if (can_delete(obj->birth_epoch, obj->retired_epoch, mytid, low, high)) {
                thr[mytid].space--;
                *prev_p = smr_next;
                delete obj;
            } else {
                prev_p = &obj->smr_next;
            }
            obj = smr_next;
        }
    }

    inline void take_snapshot(const int tid)
    {
        thr[tid].sum += thr[tid].space;
        thr[tid].count++;
    }

    inline long long cal_space(size_t size, const int tid)
    {
        return (long long) (thr[tid].sum) / ((ssize_t) thr[tid].count);
    }

private:
    inline bool can_delete(uint64_t birth_epoch, uint64_t retired_epoch, const int mytid, uint64_t *low, uint64_t *high)
    {
        for (int tid = 0; tid < maxThreads; tid++) {
            if (high[tid] < birth_epoch || low[tid] > retired_epoch) continue;
            return false;
        }
        return true;
    }
};

#endif /* _IBR_H_ */
