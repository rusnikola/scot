/******************************************************************************
 * Copyright (c) 2014-2017, Pedro Ramalhete, Andreia Correia
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

#ifndef _HAZARD_POINTERS_ORIG_H_
#define _HAZARD_POINTERS_ORIG_H_

#include <atomic>
#include <iostream>
#include <vector>


template<typename T>
class HazardPointersOrig {

private:
    static const int      HP_MAX_THREADS = 384;
    static const int      HP_MAX_HPS = 5;
    static const int      CLPAD = 128/sizeof(std::atomic<T*>);
    static const int      HP_THRESHOLD_R = 128;
    static const int      MAX_RETIRED = HP_MAX_THREADS*HP_MAX_HPS;

    const int             maxHPs;
    const int             maxThreads;

    alignas(128) std::atomic<T*>*      hp[HP_MAX_THREADS];

    alignas(128) std::vector<T*>       retiredList[HP_MAX_THREADS*CLPAD];
    alignas(128) char pad[0];

    typedef struct retired_node_controller {
        size_t list_counter;
        ssize_t sum;
        size_t count;
        ssize_t space;
        alignas(128) char pad[0];
    } retired_node_controller_t;

    retired_node_controller_t* rnc;

public:
    HazardPointersOrig(int maxHPs=HP_MAX_HPS, int maxThreads=HP_MAX_THREADS) : maxHPs{maxHPs}, maxThreads{maxThreads} {
        rnc = static_cast<retired_node_controller_t*>(aligned_alloc(128, sizeof(retired_node_controller_t) * HP_MAX_THREADS));
        for (int it = 0; it < HP_MAX_THREADS; it++) {
            hp[it] = new std::atomic<T*>[CLPAD*2];
            retiredList[it*CLPAD].reserve(MAX_RETIRED);
            for (int ihp = 0; ihp < HP_MAX_HPS; ihp++) {
                hp[it][ihp].store(nullptr, std::memory_order_relaxed);
            }

            rnc[it].list_counter = 0;
            rnc[it].sum = 0;
            rnc[it].count = 0;
            rnc[it].space = 0;
        }
    }

    ~HazardPointersOrig() {
        for (int tid = 0; tid < maxThreads; tid++) {
            auto &list = retiredList[tid * CLPAD];
            for (auto obj : list) {
                delete obj;
            }
            list.clear();
        }
        free(rnc);
    }


    /**
     * Progress Condition: wait-free bounded (by maxHPs)
     */
    inline void clear(const int tid) {
        for (int ihp = 0; ihp < maxHPs; ihp++) {
            hp[tid][ihp].store(nullptr, std::memory_order_release);
        }
    }


    /**
     * Progress Condition: wait-free population oblivious
     */
    inline void clearOne(int ihp, const int tid) {
        hp[tid][ihp].store(nullptr, std::memory_order_release);
    }


    /**
     * Progress Condition: lock-free
     */
    inline T* protect(int index, const std::atomic<T*>& atom, const int tid) {
        T* n = nullptr;
        T* ret;
        while ((ret = atom.load()) != n) {
            hp[tid][index].store((T*)((size_t) ret & ~3ULL));
            n = ret;
        }
        return ret;
    }


    /**
     * This returns the same value that is passed as ptr, which is sometimes useful
     * Progress Condition: wait-free population oblivious
     */
    inline T* protectPtr(int index, T* ptr, const int tid) {
        hp[tid][index].store(ptr);
        return ptr;
    }

    /**
     * This returns the same value that is passed as ptr, which is sometimes useful
     * Progress Condition: wait-free population oblivious
     */
    inline T* protectPtrRelease(int index, T* ptr, const int tid) {
        hp[tid][index].store(ptr, std::memory_order_release);
        return ptr;
    }


    /**
     * Progress Condition: wait-free bounded (by the number of threads squared)
     */
    void retire(T* ptr, const int tid) {
        rnc[tid].space++;
        retiredList[tid*CLPAD].push_back(ptr);
        rnc[tid].list_counter++;
        if (rnc[tid].list_counter % HP_THRESHOLD_R != 0 ) return;

        for (unsigned iret = 0; iret < retiredList[tid*CLPAD].size();) {
            auto obj = retiredList[tid*CLPAD][iret];
            for (size_t i = 0; i < maxThreads; i++) {
                for (size_t j = 0; j < maxHPs; j++) {
                    if (hp[i][j].load() == obj) {
                        iret++;
                        goto next;
                    }
                }
            }
            rnc[tid].space--;
            retiredList[tid*CLPAD].erase(retiredList[tid*CLPAD].begin() + iret);
            delete obj;
next:       ;
        }
    }

    inline void take_snapshot(const int tid){
        rnc[tid].sum += rnc[tid].space;
        rnc[tid].count++;
    }

    inline long long cal_space(size_t size, const int tid){
        return (long long) (rnc[tid].sum) / ((ssize_t) rnc[tid].count);
    }
};

#endif /* _HAZARD_POINTERS_ORIG_H_ */
