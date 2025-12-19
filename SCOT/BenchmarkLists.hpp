/******************************************************************************
 * Copyright (c) 2016-2017, Pedro Ramalhete, Andreia Correia
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

#ifndef _BENCHMARK_LISTS_H_
#define _BENCHMARK_LISTS_H_

#include <atomic>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include "HarrisLinkedListNR.hpp"
#include "HarrisLinkedListEBR.hpp"
#include "HarrisLinkedListLFHP.hpp"
#include "HarrisLinkedListLFHPO.hpp"
#include "HarrisLinkedListLFHE.hpp"
#include "HarrisLinkedListLFIBR.hpp"
#include "HarrisLinkedListLFHyaline.hpp"
#include "HarrisLinkedListWFHP.hpp"
#include "HarrisLinkedListWFHPO.hpp"
#include "HarrisLinkedListWFHE.hpp"
#include "HarrisLinkedListWFIBR.hpp"
#include "HarrisLinkedListWFHyaline.hpp"
#include "HarrisMichaelLinkedListNR.hpp"
#include "HarrisMichaelLinkedListHP.hpp"
#include "HarrisMichaelLinkedListHPO.hpp"
#include "HarrisMichaelLinkedListHE.hpp"
#include "HarrisMichaelLinkedListEBR.hpp"
#include "HarrisMichaelLinkedListIBR.hpp"
#include "HarrisMichaelLinkedListHyaline.hpp"
#include "NatarajanMittalTreeNR.hpp"
#include "NatarajanMittalTreeHP.hpp"
#include "NatarajanMittalTreeHPO.hpp"
#include "NatarajanMittalTreeHE.hpp"
#include "NatarajanMittalTreeEBR.hpp"
#include "NatarajanMittalTreeIBR.hpp"
#include "NatarajanMittalTreeHyaline.hpp"
#include <unistd.h>

using namespace std;
using namespace chrono;

enum DsType {
    DS_TYPE_LISTLF = 0,
    DS_TYPE_LISTWF = 1,
    DS_TYPE_TREE = 2
};

class BenchmarkLists {

private:
    struct UserData  {
        long long seq;
        UserData(long long lseq) {
            this->seq = lseq;
        }
        UserData() {
            this->seq = -2;
        }
        UserData(const UserData &other) : seq(other.seq) { }

        bool operator < (const UserData& other) const {
            return seq < other.seq;
        }
        bool operator == (const UserData& other) const {
            return seq == other.seq;
        }
        long long getSeq() const {
            return seq;
        }
    };

    struct Result {
        nanoseconds nsEnq = 0ns;
        nanoseconds nsDeq = 0ns;
        long long numEnq = 0;
        long long numDeq = 0;
        long long totOpsSec = 0;

        Result() { }

        Result(const Result &other) {
            nsEnq = other.nsEnq;
            nsDeq = other.nsDeq;
            numEnq = other.numEnq;
            numDeq = other.numDeq;
            totOpsSec = other.totOpsSec;
        }

        bool operator < (const Result& other) const {
            return totOpsSec < other.totOpsSec;
        }
    };


    static const long long NSEC_IN_SEC = 1000000000LL;

    int numThreads;

public:
    BenchmarkLists(int numThreads) {
        this->numThreads = numThreads;
    }

    template<typename L, size_t N = 1>
    std::pair<long long, long long> benchmark(const seconds testLengthSeconds, const int numRuns, const int numElements, DsType dsType, int readPercent, int insertPercent, int deletePercent, const std::string& reclamation) {
        long long ops[numThreads][numRuns];
        long long mem[numThreads][numRuns];
        atomic<bool> quit = { false };
        atomic<bool> startFlag = { false };
        L* list = nullptr;
        string className;
        bool isNR = (reclamation == "NR");
        // For performance, use several threads to prefill large key ranges;
        // otherwise, it takes a lot of time for each data point
        const size_t sequential_prefill_threshold = 100000;

        std::unique_ptr<UserData[]> udpool(new UserData[numElements]);
        std::vector<UserData*> udarray(numElements);

        for (size_t i = 0; i < numElements; ++i) {
            udpool[i] = UserData((long long)i);
            udarray[i] = &udpool[i];
        }

        srand((unsigned) time(NULL));

        auto rw_lambda = [this,&quit,&startFlag,&list,&udarray,&numElements,&dsType,&readPercent,&insertPercent](long long *ops, const int tid) {
            long long numOps = 0;
            uint64_t r = rand();
            std::mt19937_64 gen_k(r);
            std::mt19937_64 gen_p(r+1);
            while (!startFlag.load()) { }
            while (!quit.load()) {
                r = gen_k();
                auto ix = r%numElements;
                int op = gen_p()%100;

                if (op < readPercent) {
                    bool success = list->search(udarray[ix], tid);
                } else if (op < (readPercent + insertPercent)) {
                    list->insert(udarray[ix], tid);
                } else {
                    list->remove(udarray[ix], tid);
                }
                numOps += 1;
            }
            *ops = numOps;
        };

        for (int irun = 0; irun < numRuns; irun++) {
            const int prefillThreadCount = std::min((int) sysconf(_SC_NPROCESSORS_ONLN), 384);  // Max # threads to use for prefilling (cap at 384 threads)
            const int maxThreadsNeeded = (numElements > sequential_prefill_threshold) ? std::max(numThreads, prefillThreadCount) : numThreads;
            list = new L(maxThreadsNeeded);

            std::vector<long long> keys;
            uint64_t r = 1;
            std::mt19937_64 gen(1);

            size_t half = numElements / 2;
            keys.reserve(half);
            for (size_t i = 0; i < half; ++i) {
                r = gen();
                keys.push_back(r%numElements);
            }

            // Use sequential prefill for small datasets, parallel for large datasets
            if (numElements <= sequential_prefill_threshold || prefillThreadCount < 2) {
                // Sequential prefill with thread 0
                for (auto& key : keys) {
                    list->insert(new UserData(key), 0);
                }
            } else {
                // Parallel prefill using all threads
                auto prefill_lambda = [&list, &keys, half, prefillThreadCount](const int tid) {
                    size_t chunk_size = (half + prefillThreadCount - 1) / prefillThreadCount;
                    size_t start_idx = tid * chunk_size;
                    size_t end_idx = std::min(start_idx + chunk_size, half);

                    for (size_t i = start_idx; i < end_idx; ++i) {
                        list->insert(new UserData(keys[i]), tid);
                    }
                };

                thread prefillThreads[prefillThreadCount];
                for (int tid = 0; tid < prefillThreadCount; tid++) {
                    prefillThreads[tid] = thread(prefill_lambda, tid);
                }
                for (int tid = 0; tid < prefillThreadCount; tid++) {
                    prefillThreads[tid].join();
                }
            }
            
            if (irun == 0) {
                cout << "##### " << list->className() << " #####  \n";
                className = list->className();
            }
            thread rwThreads[numThreads];
            for (int tid = 0; tid < numThreads; tid++) rwThreads[tid] = thread(rw_lambda, &ops[tid][irun], tid);
            startFlag.store(true);
            
            this_thread::sleep_for(testLengthSeconds);
            quit.store(true);
            for (int tid = 0; tid < numThreads; tid++) rwThreads[tid].join();
            quit.store(false);
            startFlag.store(false);
            for (int tid = 0; tid < numThreads; tid++) mem[tid][irun] = list->calculate_space(tid);

            if (!isNR) {
                // For large key ranges, we are running separately for
                // each individual data point anyway
                if (numElements <= sequential_prefill_threshold){
                    for (int i = 0; i < numElements; i++) {
                        list->remove(udarray[i], 0);
                    }
                }
            }
            delete list;
        }

        vector<long long> agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            agg[irun] = 0;
            for (int tid = 0; tid < numThreads; tid++) {
                agg[irun] += ops[tid][irun];
            }
            agg[irun] /= testLengthSeconds.count();
        }
        
        vector<long long> mem_agg(numRuns);
        for (int irun = 0; irun < numRuns; irun++) {
            long long agg = 0;
            for (int tid = 0; tid < numThreads; tid++) {
                agg += mem[tid][irun];
            }
            mem_agg[irun] = agg;
        }

        // Compute the median, max and min. numRuns must be an odd number
        std::sort(agg.begin(), agg.end());
        auto maxops = agg[numRuns - 1];
        auto minops = agg[0];
        auto medianops = agg[numRuns / 2];
        auto delta = (long)(100. * (maxops - minops) / ((double)medianops));
        
        sort(mem_agg.begin(),mem_agg.end());
        auto mem_maxops = mem_agg[numRuns-1];
        auto mem_minops = mem_agg[0];
        auto mem_medianops = mem_agg[numRuns/2];
        auto mem_delta = (mem_medianops == 0) ? 0 : (long)(100. * (mem_maxops - mem_minops) / ((double)mem_medianops));
        
        for (int irun = 0; irun < numRuns; irun++) {
            std::cout << "\n\n#### RUN " << (irun + 1) << " RESULT: ####" << "\n";
            std::cout << "\n----- Benchmark=" << className <<   "   numElements=" << numElements << "   numThreads=" << numThreads << "   testLength=" << testLengthSeconds.count() << "s -----\n";

            std::cout << "Ops/sec = " << agg[irun] << "\n";
            std::cout << "memory_usage (Bytes) = " << mem_agg[irun] << "\n";
        }
        
        std::cout << "\n\n###### MEDIAN RESULT FOR ALL " << numRuns << " RUNS: ######" << "\n";
          std::cout << "\n----- Benchmark=" << className <<   "   numElements=" << numElements << "   numThreads=" << numThreads << "   testLength=" << testLengthSeconds.count() << "s -----\n";
        
        std::cout << "Ops/sec = " << medianops << "   delta = " << delta << "%   min = " << minops << "   max = " << maxops << "\n";
        std::cout << "memory_usage = " << mem_medianops << "   delta = " << mem_delta << "%   min = " << mem_minops << "   max = " << mem_maxops << "\n";
         return {medianops, mem_medianops};
    }


public:

    static void allThroughputTests(DsType dsType, int testLengthSeconds, int numElements, int numberOfRuns, int readPercent, int insertPercent, int deletePercent, const std::string& reclamation, int userThreadCount = -1) {
        vector<int> threadList;
        if (userThreadCount > 0) {
            threadList = { userThreadCount };
        } else {
            threadList = { 1, 16, 32, 64, 128, 256, 384 };
        }
        // threadList = { 1, 4, 8, 12, 16, 24, 32 }; // for a laptop
        const int numRuns = numberOfRuns;
        const seconds testLength = seconds(testLengthSeconds);
        vector<int> elemsList;
        long long ops[8][threadList.size()];
        long long mem[8][threadList.size()];

        if (dsType == DS_TYPE_LISTWF || dsType == DS_TYPE_LISTLF) {
            const int MHLNONE = 0;
            const int HLNONE = 1;
            const int MHLEBR = 0;
            const int HLEBR = 1;
            const int MHLHP = 0;
            const int HLHP = 1;
            const int MHLHPO = 0;
            const int HLHPO = 1;
            const int MHLHE = 0;
            const int HLHE = 1;
            const int MHLIBR = 0;
            const int HLIBR = 1;
            const int MHLHYALINE = 0;
            const int HLHYALINE = 1;

            for (int ithread = 0; ithread < threadList.size(); ithread++) {
                        auto nThreads = threadList[ithread];
                        BenchmarkLists bench(nThreads);
                        if(reclamation == "NR"){
                            auto result1 = bench.benchmark<HarrisMichaelLinkedListNR<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                            ops[MHLNONE][ithread] = result1.first;
                            mem[MHLNONE][ithread] = result1.second;
                            auto result2 = bench.benchmark<HarrisLinkedListNR<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                            ops[HLNONE][ithread] = result2.first;
                            mem[HLNONE][ithread] = result2.second;
                        } else if(reclamation == "EBR"){
                            auto result3 = bench.benchmark<HarrisMichaelLinkedListEBR<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                            ops[MHLEBR][ithread] = result3.first;
                            mem[MHLEBR][ithread] = result3.second;
                            auto result4 = bench.benchmark<HarrisLinkedListEBR<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                            ops[HLEBR][ithread] = result4.first;
                            mem[HLEBR][ithread] = result4.second;
                        } else if(reclamation == "HP"){
                            auto result5 = bench.benchmark<HarrisMichaelLinkedListHP<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                            ops[MHLHP][ithread] = result5.first;
                            mem[MHLHP][ithread] = result5.second;
                            auto result6 = (dsType == DS_TYPE_LISTLF) ? bench.benchmark<HarrisLinkedListLFHP<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation) : bench.benchmark<HarrisLinkedListWFHP<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                            ops[HLHP][ithread] = result6.first;
                            mem[HLHP][ithread] = result6.second;
                        } else if(reclamation == "HPO"){
                            auto result13 = bench.benchmark<HarrisMichaelLinkedListHPO<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                            ops[MHLHPO][ithread] = result13.first;
                            mem[MHLHPO][ithread] = result13.second;
                            auto result14 = (dsType == DS_TYPE_LISTLF) ? bench.benchmark<HarrisLinkedListLFHPO<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation) : bench.benchmark<HarrisLinkedListWFHPO<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                            ops[HLHPO][ithread] = result14.first;
                            mem[HLHPO][ithread] = result14.second;
                        } else if(reclamation == "IBR"){
                            auto result7 = bench.benchmark<HarrisMichaelLinkedListIBR<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                            ops[MHLIBR][ithread] = result7.first;
                            mem[MHLIBR][ithread] = result7.second;
                            auto result8 = (dsType == DS_TYPE_LISTLF) ? bench.benchmark<HarrisLinkedListLFIBR<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation) : bench.benchmark<HarrisLinkedListWFIBR<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                            ops[HLIBR][ithread] = result8.first;
                            mem[HLIBR][ithread] = result8.second;
                        } else if(reclamation == "HE"){
                            auto result9 = bench.benchmark<HarrisMichaelLinkedListHE<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                            ops[MHLHE][ithread] = result9.first;
                            mem[MHLHE][ithread] = result9.second;
                            auto result10 = (dsType == DS_TYPE_LISTLF) ? bench.benchmark<HarrisLinkedListLFHE<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation) : bench.benchmark<HarrisLinkedListWFHE<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                            ops[HLHE][ithread] = result10.first;
                            mem[HLHE][ithread] = result10.second;
                        } else if(reclamation == "HYALINE"){
                            auto result11 = bench.benchmark<HarrisMichaelLinkedListHyaline<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                            ops[MHLHYALINE][ithread] = result11.first;
                            mem[MHLHYALINE][ithread] = result11.second;
                            auto result12 = (dsType == DS_TYPE_LISTLF) ? bench.benchmark<HarrisLinkedListLFHyaline<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation) : bench.benchmark<HarrisLinkedListWFHyaline<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                            ops[HLHYALINE][ithread] = result12.first;
                            mem[HLHYALINE][ithread] = result12.second;
                        }
                    }
        } else {
            const int NTNONE = 0;
            const int NTEBR = 0;
            const int NTHP = 0;
            const int NTHPO = 0;
            const int NTHE = 0;
            const int NTIBR = 0;
            const int NTHYALINE = 0;

            for (int ithread = 0; ithread < threadList.size(); ithread++) {
                auto nThreads = threadList[ithread];
                BenchmarkLists bench(nThreads);

                if(reclamation == "NR"){
                    auto result1 = bench.benchmark<NatarajanMittalTreeNR<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                    ops[NTNONE][ithread] = result1.first;
                    mem[NTNONE][ithread] = result1.second;
                } else if(reclamation == "EBR") {
                    auto result2 = bench.benchmark<NatarajanMittalTreeEBR<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                    ops[NTEBR][ithread] = result2.first;
                    mem[NTEBR][ithread] = result2.second;
                } else if(reclamation == "HP"){
                    auto result3 = bench.benchmark<NatarajanMittalTreeHP<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                    ops[NTHP][ithread] = result3.first;
                    mem[NTHP][ithread] = result3.second;
                } else if(reclamation == "HPO"){
                    auto result7 = bench.benchmark<NatarajanMittalTreeHPO<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                    ops[NTHPO][ithread] = result7.first;
                    mem[NTHPO][ithread] = result7.second;
                } else if(reclamation == "IBR"){
                    auto result4 = bench.benchmark<NatarajanMittalTreeIBR<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                    ops[NTIBR][ithread] = result4.first;
                    mem[NTIBR][ithread] = result4.second;
                } else if(reclamation == "HE"){
                    auto result5 = bench.benchmark<NatarajanMittalTreeHE<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                    ops[NTHE][ithread] = result5.first;
                    mem[NTHE][ithread] = result5.second;
                } else if(reclamation == "HYALINE"){
                    auto result6 = bench.benchmark<NatarajanMittalTreeHyaline<UserData, 1>, 1>(testLength, numRuns, numElements, dsType, readPercent, insertPercent, deletePercent, reclamation);
                    ops[NTHYALINE][ithread] = result6.first;
                    mem[NTHYALINE][ithread] = result6.second;
                }
            }
        }

        std::cout<<"\n\nFINAL RESULTS (FOR CHARTS):"<<std::endl<<std::endl;
        std::cout << "\nResults in ops per second for numRuns=" << numRuns << ",  length=" << testLength.count() << "s \n";
        std::cout << "\nNumber of elements: " << numElements << "\n\n";
        int classSize;
        if (dsType == DS_TYPE_LISTWF || dsType == DS_TYPE_LISTLF) {
            classSize = 2;
            if (reclamation == "NR") {
                cout << "Threads, HarrisMichaelLinkedListNR, HarrisLinkedListNR\n";
            } else if(reclamation == "EBR"){
                cout << "Threads, HarrisMichaelLinkedListEBR, HarrisLinkedListEBR, HarrisMichaelLinkedListEBR_Memory_Usage, HarrisLinkedListEBR_Memory_Usage\n";
            } else if(reclamation == "HP"){
                cout << "Threads, HarrisMichaelLinkedListHP, HarrisLinkedListHP, HarrisMichaelLinkedListHP_Memory_Usage, HarrisLinkedListHP_Memory_Usage\n";
            } else if(reclamation == "HPO"){
                cout << "Threads, HarrisMichaelLinkedListHPO, HarrisLinkedListHPO, HarrisMichaelLinkedListHPO_Memory_Usage, HarrisLinkedListHPO_Memory_Usage\n";
            } else if(reclamation == "IBR"){
                cout << "Threads, HarrisMichaelLinkedListIBR, HarrisLinkedListIBR, HarrisMichaelLinkedListIBR_Memory_Usage, HarrisLinkedListIBR_Memory_Usage\n";
            } else if(reclamation == "HE"){
                cout << "Threads, HarrisMichaelLinkedListHE, HarrisLinkedListHE, HarrisMichaelLinkedListHE_Memory_Usage, HarrisLinkedListHE_Memory_Usage\n";
            } else if(reclamation == "HYALINE"){
                cout << "Threads, HarrisMichaelLinkedListHYALINE, HarrisLinkedListHYALINE, HarrisMichaelLinkedListHYALINE_Memory_Usage, HarrisLinkedListHYALINE_Memory_Usage\n";
            }
        } else {
            classSize = 1;
            if(reclamation == "NR"){
                cout << "Threads, NatarajanMittalTreeNR\n";
            } else if(reclamation == "EBR"){
                cout << "Threads, NatarajanMittalTreeEBR, NatarajanMittalTreeEBR_Memory_Usage\n";
            } else if(reclamation == "HP"){
                cout << "Threads, NatarajanMittalTreeHP, NatarajanMittalTreeHP_Memory_Usage\n";
            } else if(reclamation == "HPO"){
                cout << "Threads, NatarajanMittalTreeHPO, NatarajanMittalTreeHPO_Memory_Usage\n";
            } else if(reclamation == "IBR"){
                cout << "Threads, NatarajanMittalTreeIBR, NatarajanMittalTreeIBR_Memory_Usage\n";
            } else if(reclamation == "HE"){
                cout << "Threads, NatarajanMittalTreeHE, NatarajanMittalTreeHE_Memory_Usage\n";
            } else if(reclamation == "HYALINE"){
                cout << "Threads, NatarajanMittalTreeHYALINE, NatarajanMittalTreeHYALINE_Memory_Usage\n";
            }
        }
        for (int ithread = 0; ithread < threadList.size(); ithread++) {
            auto nThreads = threadList[ithread];
            cout << nThreads << ", ";
            for (int il = 0; il < classSize; il++) {
                cout << ops[il][ithread] << ", ";
            }
            
            for (int il = 0; il < classSize; il++) {
                cout << mem[il][ithread] << ", ";
            }
            cout << "\n";
        }
    }
};

#endif
