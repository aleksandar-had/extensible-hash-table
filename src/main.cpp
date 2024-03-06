#include <assert.h>
#include <getopt.h>
#include <omp.h>
#include <stdlib.h>
#include <time.h>

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

#include "lock_based_hashtable.h"
#include "lock_free_hashtable.h"

#define FIXED_DOUBLE(x) std::fixed << std::setprecision(2) << (x)

void Usage(const std::string& prog_name) {
	std::cout << prog_name << " [Options]" << std::endl
	          << "Options:" << std::endl
	          << "-i	Number of iterations (default: 30)" << std::endl
	          << "-t	Number of threads (default: 8)" << std::endl
	          << "-s	Timelimit in milliseconds (default: 1000)" << std::endl
	          << "-c	Test correctness instead of throughput (default: false)" << std::endl
	          << "-r	Record and save speedup in a file (default: false)" << std::endl
	          << "-g	Test throughput with one global region instead of thread local regions (default: false)" << std::endl
	          << "-v	Test throughput with variyng load factor" << std::endl
	          << "-h	Print this message" << std::endl;
}

/**
 * @brief Apparently thread safe random number generator from stackoverlfow :P
 *
 * @param min
 * @param max
 * @return int
 */
int intRand(const int& min, const int& max) {
	static thread_local std::mt19937 generator;
	std::uniform_int_distribution<int> distribution(min, max);
	return distribution(generator);
}

/**
 * @brief Test Correctness via Adding/Removing a specified amount of elements per thread.
 * Each thread has its own region of elements so that we can check each return value of
 * Add/Remove/Contains methods and assert a known value.
 * This function should be called with massive overloading, such that a lot different
 * interleavings occur.
 *
 * @param n_per_thread
 * @param myHashTable
 * @param n_threads
 */
void TestCorrectness(uint32_t n_per_thread, HashTable* myHashTable, int n_threads) {
	srand(time(NULL));
	uint32_t random_offset = (uint32_t)rand();

	omp_set_dynamic(0);
	omp_set_num_threads(n_threads);
#pragma omp parallel
	{
		int t = omp_get_thread_num();
#pragma omp barrier
		bool ret_val;
		for (uint32_t i = 0; i < n_per_thread; i++) {
			uint32_t number = i + t * n_per_thread + random_offset;
			ret_val = myHashTable->Contains(number);
			assert(!ret_val);
			ret_val = myHashTable->Add(number);
			assert(ret_val);
			ret_val = myHashTable->Contains(number);
			assert(ret_val);
			ret_val = myHashTable->Add(number);
			assert(!ret_val);
		}
		for (uint32_t i = 0; i < n_per_thread; i++) {
			uint32_t number = i + t * n_per_thread + random_offset;
			ret_val = myHashTable->Contains(number);
			assert(ret_val);
			ret_val = myHashTable->Remove(number);
			assert(ret_val);
			ret_val = myHashTable->Contains(number);
			assert(!ret_val);
			ret_val = myHashTable->Remove(number);
			assert(!ret_val);
		}
	}
	std::cout << "No assertion violation observed" << std::endl;
}

/**
 * @brief Test throughput, similar to TestCorrectness() every thread has its own regions
 * of elements. Which means that the retry_counters are zero most of the time or a very low value otherwise.
 *
 * @param time_limit
 * @param myHashTable
 * @param n_threads
 * @return int number of operations of all threads
 */
int TestThroughputLocalRegions(double time_limit, HashTable* myHashTable, int n_threads) {
	srand(time(NULL));
	uint32_t random_offset = (uint32_t)rand();

	omp_set_dynamic(0);
	omp_set_num_threads(n_threads);
	uint32_t thread_region_width = UINT32_MAX / n_threads;
	int operation_count[n_threads];
#pragma omp parallel
	{
		int t = omp_get_thread_num();
		uint32_t local_thread_offset = thread_region_width * t + random_offset;
		uint32_t local_number = local_thread_offset;
		int local_operation_count = 0;
		double start, now;
#pragma omp barrier
		start = omp_get_wtime();
		now = omp_get_wtime();
		while ((now - start) < time_limit / 2) {
			myHashTable->Contains(local_number);
			myHashTable->Add(local_number);
			local_number++;
			local_operation_count += 2;
			now = omp_get_wtime();
		}

		local_number = local_thread_offset;
		start = omp_get_wtime();
		now = omp_get_wtime();
		while ((now - start) < time_limit / 2) {
			myHashTable->Contains(local_number);
			myHashTable->Remove(local_number);
			local_number++;
			local_operation_count += 2;
			now = omp_get_wtime();
		}
#pragma omp barrier
		operation_count[t] = local_operation_count;
	}
	int ret = 0;
	for (int i = 0; i < n_threads; i++) {
		ret += operation_count[i];
	}
	std::cout << std::to_string(ret) << " operations" << std::endl;
	return ret;
}

int TestThroughputSameRegion(double time_limit, HashTable* myHashTable, int n_threads) {
	srand(time(NULL));
	uint32_t random_offset = (uint32_t)rand();

	omp_set_dynamic(0);
	omp_set_num_threads(n_threads);
	int operation_count[n_threads];
#pragma omp parallel
	{
		int t = omp_get_thread_num();
		uint32_t local_number = 0;
		int RANDOM_MIN = random_offset;
		int RANDOM_MAX = random_offset + 100000;
		int local_operation_count = 0;
		double start, now;
#pragma omp barrier
		start = omp_get_wtime();
		now = omp_get_wtime();
		while ((now - start) < time_limit) {
			local_number = intRand(RANDOM_MIN, RANDOM_MAX);
			myHashTable->Contains(local_number);
			int add_remove_rand = intRand(0, 1);
			if (add_remove_rand == 0)
				myHashTable->Add(local_number);
			else
				myHashTable->Remove(local_number);
			local_operation_count += 2;
			now = omp_get_wtime();
		}
#pragma omp barrier
		operation_count[t] = local_operation_count;
	}
	int ret = 0;
	for (int i = 0; i < n_threads; i++) {
		ret += operation_count[i];
	}
	std::cout << std::to_string(ret) << " operations" << std::endl;
	return ret;
}

std::vector<uint64_t> TestVarLoadFactor(double time_limit_per_load_fact, HashTable* myHashTable, int n_threads) {
	srand(time(NULL));

	double load_factors[8][3] = {
	    {0, 0, 1}, {0.2, 0, 0.8}, {0.4, 0, 0.6}, {0.6, 0, 0.4}, {0.8, 0, 0.2}, {0.9, 0.1, 0}, {0.7, 0.3, 0}, {0.2, 0.05, 0.75}};
	std::vector<uint64_t> ret = {0, 0, 0, 0, 0, 0, 0, 0};

	std::random_device rd;                           // Will be used to obtain a seed for the random number engine
	std::mt19937 gen(rd());                          // Standard mersenne_twister_engine seeded with rd()
	std::uniform_real_distribution<> dis(0.0, 1.0);  // uniform distribution [0, 1)

	uint32_t random_offset = (uint32_t)rand();
	omp_set_dynamic(0);
	omp_set_num_threads(n_threads);
	double operation_count[n_threads];
	uint32_t load_factors_len = sizeof(load_factors) / sizeof(*load_factors);
    int RANDOM_MIN = random_offset;
	int RANDOM_MAX = random_offset + 10000;
    int random_number;

    bool prefill_table = true;
    // prefill table with "half" of what theoretically could be considered as working borders
    if (prefill_table) {
        for (int j = 0; j < 5000; j++) {
            random_number = intRand(RANDOM_MIN, RANDOM_MAX);
            myHashTable->Add(random_number);
        }
    }

#pragma omp parallel
	{
		int t = omp_get_thread_num();
		uint32_t local_number = 0;
		double local_operation_count = 0;
		double start, now;
#pragma omp barrier
		for (uint32_t i = 0; i < load_factors_len; i++) {
			start = omp_get_wtime();
			now = omp_get_wtime();
			while ((now - start) < time_limit_per_load_fact) {
				local_number = intRand(RANDOM_MIN, RANDOM_MAX);
				// Use dis to transform the random unsigned int generated by gen into a
				// double in [0, 1). Each call to dis(gen) generates a new random double
				double op_type = dis(gen);
				// ADD operation
				if (op_type >= 0 && op_type < load_factors[i][0]) {
					myHashTable->Add(local_number);
				}
				// REMOVE operation
				else if (op_type >= load_factors[i][0] && op_type < load_factors[i][0] + load_factors[i][1]) {
					myHashTable->Remove(local_number);
				}
				// CONTAINS operation
				else if (op_type >= load_factors[i][0] + load_factors[i][1] && op_type < 1) {
					myHashTable->Contains(local_number);
				}
				local_operation_count += 1;
				now = omp_get_wtime();
			}
#pragma omp barrier
			operation_count[t] = local_operation_count;
			for (int thr = 0; thr < n_threads; thr++) {
				ret[i] += operation_count[thr];
			}
			local_operation_count = 0;
		}
	}

	for (uint32_t i = 0; i < load_factors_len; i++) {
		std::cout << "add/rem/cont: " << FIXED_DOUBLE(load_factors[i][0]) << "/" << FIXED_DOUBLE(load_factors[i][1]) << "/" << FIXED_DOUBLE(load_factors[i][2]) << " " << std::to_string(ret[i]) << " operations" << std::endl;
	}

	return ret;
}

int main(int argc, char* argv[]) {
	int n_iterations = 30;
	int n_threads = 8;
	double time_limit_seconds = 1;
	bool test_correctness = false;
	bool record_times = false;
	bool all_same_region = false;
	bool var_load_factor = false;

	while (true) {
		switch (getopt(argc, argv, "grvci:t:s:h")) {
		case 'i':
			n_iterations = std::stoi(optarg);
			continue;
		case 't':
			n_threads = std::stoi(optarg);
			continue;
		case 's':
			time_limit_seconds = ((double)std::stoi(optarg)) / 1000.0;
			continue;
		case 'c':
			test_correctness = true;
			continue;
		case 'r':
			record_times = true;
			continue;
		case 'g':
			all_same_region = true;
			continue;
		case 'v':
			var_load_factor = true;
			continue;
		case '?':
		case 'h':
		default:
			Usage(std::string(argv[0]));
			return 0;
		case -1:
			break;
		}
		break;
	}

	std::ofstream outputfile;
	if (record_times) {
		std::stringstream ss;
		auto t = std::time(nullptr);
		auto tm = *std::localtime(&t);
		ss << "TimeData_" << std::to_string(n_threads)
		   << "_threads_" << std::to_string((int)(time_limit_seconds * 1000)) << "_mseconds_"
		   << std::put_time(&tm, "%d%m%Y%H%M%S") << ".csv";

		outputfile = std::ofstream(ss.str());
		if (all_same_region)
			outputfile << "-g,";
		outputfile << "n_threads, seconds, lock-free operations iteration 0, lock-based operations iteration 0, lock-free operations iteration 1, lock-based operations iteration 1, ..." << std::endl;
		outputfile << std::to_string(n_threads) << "," << std::to_string(time_limit_seconds) << ",";
		if (!outputfile.is_open()) {
			std::cout << "File: " << ss.str() << " could not be opened" << std::endl;
			return 0;
		}
	}

	std::cout << "Number of iterations: " << std::to_string(n_iterations) << std::endl;
	std::cout << "Number of threads: " << std::to_string(n_threads) << std::endl;
	std::cout << "Number of seconds: " << std::to_string(time_limit_seconds) << std::endl;
	if (test_correctness)
		std::cout << "Testing for correctness" << std::endl;
	else
		std::cout << "Testing throughput" << std::endl;

	auto ThroughputFunction = &TestThroughputLocalRegions;
	auto VarThroughputFunction = &TestVarLoadFactor;
	if (all_same_region)
		ThroughputFunction = &TestThroughputSameRegion;

	for (int i = 0; i < n_iterations; i++) {
		std::cout << "\n\tIteration " << i << std::endl;
		LockFreeHashTable* myLockFreeHashTable = new LockFreeHashTable();
		std::cout << "Lock Free Hashtable:  ";

		int num_operations_lock_free;
		std::vector<uint64_t> num_var_operations_lock_free;

		if (test_correctness)
			TestCorrectness(5000, myLockFreeHashTable, n_threads);
		else if (var_load_factor)
			num_var_operations_lock_free = VarThroughputFunction((double)time_limit_seconds, myLockFreeHashTable, n_threads);
		else
			num_operations_lock_free = ThroughputFunction((double)time_limit_seconds, myLockFreeHashTable, n_threads);

		int num_operations_lock_based;
		std::vector<uint64_t> num_var_operations_lock_based;

		if (!test_correctness) {
			LockBasedHashTable* myLockBasedHashTable = new LockBasedHashTable();
			std::cout << "Lock Based Hashtable: ";
			if (var_load_factor) {
				num_var_operations_lock_based = VarThroughputFunction((double)time_limit_seconds, myLockBasedHashTable, n_threads);
			} else {
				num_operations_lock_based = ThroughputFunction((double)time_limit_seconds, myLockBasedHashTable, n_threads);
			}
		}

		if (record_times) {
			outputfile << std::to_string(num_operations_lock_free) << "," << std::to_string(num_operations_lock_based) << ",";
			outputfile.flush();
		}
	}
	if (record_times)
		outputfile.close();
	return 0;
}