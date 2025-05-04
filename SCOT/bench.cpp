/*
 * main.cpp
 *
 *  Created on: Apr 23, 2016
 *      Author: pramalhe
 *
 *  Modified and adopted for SCOT by Md Amit Hasan Arovi
 */

#include <iostream>
#include <thread>
#include <string>
#include <regex>
#include "BenchmarkLists.hpp"

int main(int argc, char* argv[]) {
    if (argc < 9) {
        std::cerr << "Usage: ./bench <list|tree> <test_length_seconds> <element_size> <num_runs> <read_percent> <insert_percent> <delete_percent> <reclamation> [num_threads]\n\n"
                  << "Arguments:\n"
                  << "  <list|tree>              : The data structure to test\n"
                  << "  <test_length_seconds>    : Duration of the test in seconds (e.g., 10)\n"
                  << "  <element_size>           : Number of elements (e.g., 512)\n"
                  << "  <num_runs>               : Number of times to repeat the benchmark (e.g., 5)\n"
                  << "  <read_percent>           : Percentage of read operations (e.g., 80 or 80%)\n"
                  << "  <insert_percent>         : Percentage of insert operations (e.g., 10 or 10%)\n"
                  << "  <delete_percent>         : Percentage of delete operations (e.g., 10 or 10%)\n"
                  << "  <reclamation>            : Reclamation scheme: HP | EBR | NR | IBR | HE | HYALINE\n"
                  << "  [num_threads]            : (Optional) Number of threads to run (e.g., 64)\n\n"
                  << "Note: Sum of read, insert, and delete percentages must not exceed 100.\n"
                  << std::endl;
        return 1;
    }

    std::string ds = argv[1];
    bool isList = (ds == "list");

    int testLengthSeconds = std::stoi(argv[2]);
    int elementSize = std::stoi(argv[3]);
    int numRuns = std::stoi(argv[4]);

    auto sanitizePercent = [](const std::string& str) -> int {
        std::string clean = std::regex_replace(str, std::regex("%"), "");
        return std::stoi(clean);
    };

    int readPercent   = sanitizePercent(argv[5]);
    int insertPercent = sanitizePercent(argv[6]);
    int deletePercent = sanitizePercent(argv[7]);

    if (readPercent < 0 || insertPercent < 0 || deletePercent < 0 ||
        readPercent > 100 || insertPercent > 100 || deletePercent > 100) {
        std::cerr << "Percentages must be between 0 and 100.\n";
        return 1;
    }

    if (readPercent + insertPercent + deletePercent > 100) {
        std::cerr << "Sum of read, insert, and delete percentages must not exceed 100.\n";
        return 1;
    }

    std::string reclamation = argv[8];
    if (reclamation != "HP" && reclamation != "EBR" && reclamation != "NR" && reclamation != "IBR" && reclamation != "HE" && reclamation != "HYALINE") {
        std::cerr << "Invalid reclamation strategy. Use: HP | EBR | NR | IBR | HE | HYALINE\n";
        return 1;
    }

    int userThreadCount = -1;
    if (argc >= 10) {
        try {
            userThreadCount = std::stoi(argv[9]);
            if (userThreadCount <= 0) throw std::invalid_argument("Thread count must be positive");
        } catch (...) {
            std::cerr << "Invalid thread count provided in argument 10." << std::endl;
            return 1;
        }
    }

    BenchmarkLists::allThroughputTests(
        isList,
        testLengthSeconds,
        elementSize,
        numRuns,
        readPercent,
        insertPercent,
        deletePercent,
        reclamation,
        userThreadCount
    );

    return 0;
}
