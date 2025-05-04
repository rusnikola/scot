#ifndef _HAZARD_ERAS_H_
#define _HAZARD_ERAS_H_

#include <atomic>
#include <iostream>
#include <vector>
#include <algorithm>
#include <new> 

/*
 * <h1> Optimized Hazard Eras </h1>
 *
 * This is an optimized version of Hazard Eras (HE) memory reclamation,
 * inspired by IBR, with improved cache locality, batch retirements,
 * and stack-based memory optimizations.
 *
 * Based on the paper:
 * "Hazard Eras - Non-Blocking Memory Reclamation" by Pedro Ramalhete and Andreia Correia.
 */

struct HENode {
    struct HENode *smr_next;
    uint64_t newEra;
    uint64_t delEra;
};

template<typename T>
class HazardEras {

private:
    static const uint64_t NONE = 0;
    static const int      HE_MAX_THREADS = 384;
    static const int      MAX_HES = 5;        // This is named 'K' in the HP paper
    static const int      CLPAD = 128 / sizeof(std::atomic<T*>);
    static const int      HE_THRESHOLD_R = 128; // Retirement batch threshold
    static const size_t   epoch_freq = 12;

    const int maxHEs;
    const int maxThreads;

    alignas(128) std::atomic<uint64_t>  eraClock {1};
    alignas(128) std::atomic<uint64_t>* he[HE_MAX_THREADS];
    alignas(128) char pad[0];

    typedef struct retired_node_controller {
        HENode *first;
        size_t epoch_counter;
        size_t list_counter;
        ssize_t sum;
        size_t count;
        ssize_t space;
        alignas(128) char pad[0];        
    } retired_node_controller_t;

    retired_node_controller_t* rnc;

public:
    HazardEras(int _maxHEs, int _maxThreads) : maxHEs{_maxHEs}, maxThreads{_maxThreads} {
        rnc = static_cast<retired_node_controller_t*>(aligned_alloc(128, sizeof(retired_node_controller_t) * HE_MAX_THREADS));
        if (rnc == nullptr) {
            std::cerr << "Error: Failed to allocate memory for retired_node_controller_t array\n";
            exit(1);
        }
        for (int it = 0; it < HE_MAX_THREADS; it++) {
            he[it] = new std::atomic<uint64_t>[CLPAD * 2];
            for (int ihe = 0; ihe < MAX_HES; ihe++) {
                he[it][ihe].store(NONE, std::memory_order_relaxed);
            }
            rnc[it].first = nullptr;
            rnc[it].epoch_counter = 0;
            rnc[it].list_counter = 0;
            rnc[it].sum = 0;
            rnc[it].count = 0;
            rnc[it].space = 0;
        }
        static_assert(std::is_same<decltype(T::newEra), uint64_t>::value, "T::newEra must be uint64_t");
        static_assert(std::is_same<decltype(T::delEra), uint64_t>::value, "T::delEra must be uint64_t");
    }

    ~HazardEras()
    {
        for (int tid = 0; tid < maxThreads; tid++) {
            HENode *obj = rnc[tid].first;
            while (obj != nullptr) {
                HENode *smr_next = obj->smr_next;
                rnc[tid].space--;
                delete obj;
                obj = smr_next;
            }
        }
        free(rnc);
    }

    inline T *init_object(T *obj, const int mytid)
    {
        obj->newEra = eraClock.load();
        return obj;
    }

    inline void clear(const int tid)
    {
        for (int ihe = 0; ihe < maxHEs; ihe++) {
            he[tid][ihe].store(NONE, std::memory_order_release);
        }
    }

    inline T* protect(int index, const std::atomic<T*>& atom, const int tid)
    {
        auto prevEra = he[tid][index].load(std::memory_order_relaxed);
        while (true) {
            T* ptr = atom.load();
            auto era = eraClock.load(std::memory_order_acquire);
            if (era == prevEra) return ptr;
            he[tid][index].store(era);
            prevEra = era;
        }
    }

    inline void protectEraRelease(int index, int other, const int tid)
    {
        auto era = he[tid][other].load(std::memory_order_relaxed);
        if (he[tid][index].load(std::memory_order_relaxed) == era) return;
        he[tid][index].store(era, std::memory_order_release);
    }

    inline T* protectPtr(int index, const std::atomic<T*>& atom, uint64_t& prevEra, const int tid)
    {
        T* ptr = atom.load(std::memory_order_acquire);
        auto era = eraClock.load();
        if (prevEra != era) {
            prevEra = era;
            he[tid][index].store(era, std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_seq_cst);
        }
        return ptr;
    }

    void retire(T* ptr, const int mytid)
    {
        rnc[mytid].space++;
        auto currEra = eraClock.load();
        ptr->delEra = currEra;
        rnc[mytid].epoch_counter++;
        if (rnc[mytid].epoch_counter % (epoch_freq * maxThreads) == 0)
            eraClock.fetch_add(1);

        ptr->smr_next = rnc[mytid].first;
        rnc[mytid].first = ptr;
        rnc[mytid].list_counter++;
        if (rnc[mytid].list_counter % HE_THRESHOLD_R != 0) return;

        uint64_t he_eras[HE_MAX_THREADS * MAX_HES];
        uint64_t prev = NONE;
        size_t he_size = 0;
        for (int tid = 0; tid < maxThreads; tid++) {
            for (int ihe = 0; ihe < maxHEs; ihe++) {
                uint64_t val = he[tid][ihe].load();
                if (val != NONE && val != prev)
                    prev = he_eras[he_size++] = val;
            }
        }

        HENode **prev_p = &rnc[mytid].first;
        HENode *obj = rnc[mytid].first;
        while (obj != nullptr) {
            HENode *smr_next = obj->smr_next;
            for (size_t i = 0; i < he_size; i++) {
                const auto era = he_eras[i];
                if (era >= obj->newEra && era <= obj->delEra) {
                    prev_p = &obj->smr_next;
                    goto next;
                }
            }
            rnc[mytid].space--;
            *prev_p = smr_next;
            delete obj;
next:       obj = smr_next;
        }
    }

    inline void take_snapshot(const int tid) {
        rnc[tid].sum += rnc[tid].space;
        rnc[tid].count++;
    }

    inline long long cal_space(size_t size, const int tid) {
        return (long long) (rnc[tid].sum) / ((ssize_t) rnc[tid].count);
    }
};

#endif /* _HAZARD_ERAS_H_ */
