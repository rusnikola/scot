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

#ifndef _HYALINE_H_
#define _HYALINE_H_

#include <atomic>
#include <malloc.h>
#include "hyaline/lfbsmro.h"

struct HyalineNode : lfbsmro_node { };

template<typename T>
class Hyaline {
private:
    static const size_t epoch_freq = 12;
    static const size_t empty_freq = 128;

    typedef struct hyaline_private_data {
        alignas(128) lfbsmro_handle_t handle;
        lfbsmro_batch_t batch;
        size_t init_counter;
        ssize_t sum;
        size_t count;
        ssize_t space;
        alignas(128) char pad[0];
    } hyaline_private_data_t;

    size_t SMR_EFREQ, SMR_ORDER, SMR_BATCH;
    const int maxThreads;
    struct lfbsmro         *smr;
    hyaline_private_data_t *thr;

public:
    Hyaline(const int _maxThreads) : maxThreads{_maxThreads}
    {
        SMR_ORDER = (sizeof(int) * 8 - __builtin_clz(maxThreads));
        if (!(maxThreads & (maxThreads - 1))) SMR_ORDER--;
        SMR_EFREQ = epoch_freq * maxThreads;
        SMR_BATCH = maxThreads < empty_freq ? empty_freq : maxThreads + 1;
        thr = static_cast<hyaline_private_data_t*>(aligned_alloc(128, sizeof(hyaline_private_data_t) * maxThreads));
        if (thr == nullptr) {
            std::cerr << "Error: Failed to allocate memory for ibr_private_data_t array\n";
            exit(1);
        }
        smr = static_cast<struct lfbsmro*>(aligned_alloc(128, LFBSMRO_SIZE(1UL << SMR_ORDER)));
        if (smr == nullptr) {
            std::cerr << "Error: Failed to allocate memory for ibr_reservation_t array\n";
            exit(1);
        }
        lfbsmro_init(smr, SMR_ORDER);
        for (int tid = 0; tid < maxThreads; tid++) {
            thr[tid].init_counter = 0;
            thr[tid].sum = 0;
            thr[tid].count = 0;
            thr[tid].space = 0;
            lfbsmro_batch_init(&thr[tid].batch);
        }
    }

    ~Hyaline()
    {
        free(thr);
        free(smr);
    }

    inline T *init_object(T *obj, const int tid)
    {
        lfbsmro_init_node(smr, obj, &thr[tid].init_counter, SMR_EFREQ);
        return obj;
    }

    inline void start_op(const int tid)
    {
        size_t enter_num = tid;
        lfbsmro_enter(smr, &enter_num, SMR_ORDER, &thr[tid].handle, 0, LF_DONTCHECK);
    }

    inline void end_op(const int tid)
    {
        lfbsmro_leave(smr, tid, SMR_ORDER, thr[tid].handle, hyaline_free_node, 0, LF_DONTCHECK);
    }

    inline T *protect(const std::atomic<T*> &atom, const int tid)
    {
        return (T *)lfbsmro_deref(smr, tid, (uintptr_t *)&atom);
    }

    void retire(T *ptr, const int tid)
    {
        lfbsmro_retire(smr, SMR_ORDER, ptr, hyaline_free_node, 0,
            &thr[tid].batch, SMR_BATCH);
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
    static void hyaline_free_node(struct lfbsmro *hdr, struct lfbsmro_node *smrnode)
    {
        free(smrnode);
    }
};

#endif /* _HYALINE_H_ */
