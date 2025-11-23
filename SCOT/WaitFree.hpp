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

#ifndef _WAIT_FREE_H_
#define _WAIT_FREE_H_

#define WF_THRESHOLD 32

template<typename T>
class WaitFree {
private:
     static const int WF_MAX_THREADS = 384;
     static const size_t WF_DELAY = 16;

     typedef struct alignas(128) wait_free_controller {
        std::atomic<T*> helper_key;
        std::atomic<size_t> helper_tag;
        size_t next_check;
        size_t local_tag;
        size_t curr_tid;
    } wait_free_controller_t;

    const int maxThreads;
    wait_free_controller_t *wfc;

public:

    WaitFree(int _maxThreads=WF_MAX_THREADS) : maxThreads{_maxThreads} {
        wfc = static_cast<wait_free_controller_t*>(aligned_alloc(128, sizeof(wait_free_controller_t) * WF_MAX_THREADS));
        for (int it = 0; it < WF_MAX_THREADS; it++) {
            wfc[it].helper_key.store(nullptr, std::memory_order_relaxed);
            wfc[it].helper_tag.store(0, std::memory_order_relaxed);
            wfc[it].next_check = WF_DELAY;
            wfc[it].curr_tid = 0;
            wfc[it].local_tag = 1; // _INPUT_ tags are always odd numbers
        }
    }

    ~WaitFree() {}

    inline size_t request_help(T* key, const int tid)
    {
        wfc[tid].helper_key.store(key);
        size_t local_tag = wfc[tid].local_tag;
        wfc[tid].helper_tag.store(local_tag);
        wfc[tid].local_tag = local_tag + 2; // the next odd number
        return local_tag;
    }

    inline bool help_threads(T** p_key, size_t *p_tag, int *p_tid, const int mytid)
    {
        if (--wfc[mytid].next_check != 0)
            return false;

        wfc[mytid].next_check = WF_DELAY;
        size_t curr_tid = wfc[mytid].curr_tid;
        wfc[mytid].curr_tid = (curr_tid + 1) % maxThreads;

        if (curr_tid == mytid)
            return false;

        size_t tag = wfc[curr_tid].helper_tag.load();
        if (!(tag & 0x1U))
            return false; // an output has been produced
        T *key = wfc[curr_tid].helper_key.load();
        if (wfc[curr_tid].helper_tag.load() != tag)
            return false; // a different cycle
        *p_key = key;
        *p_tid = curr_tid;
        *p_tag = tag;
        return true;
    }

    inline size_t check_result(const int tid)
    {
        return wfc[tid].helper_tag.load();
    }

    inline void produce_result(size_t tag, size_t result, const int tid)
    {
        wfc[tid].helper_tag.compare_exchange_strong(tag, result);
    }
};

#endif /* _WAIT_FREE_H_ */
