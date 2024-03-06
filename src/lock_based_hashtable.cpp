/**
 * @file lock_based_hashtable.cpp
 * @author Josef Salzmann &	Aleksandar Hadzhiyski
 * @brief Implement hashtable with locks as baseline.
 * @date 2022-05-30
 */
#include "lock_based_hashtable.h"

bool LockBasedHashTable::Add(ValueType value) {
	mutex.lock();
	auto ret = map.insert({value, 0});
	mutex.unlock();
	return ret.second;
}

bool LockBasedHashTable::Remove(ValueType value) {
	mutex.lock();
	auto ret = map.erase(value);
	mutex.unlock();
	return ret == 1;
}

bool LockBasedHashTable::Contains(ValueType value) {
	mutex.lock();
	bool ret = map.find(value) != map.end();
	mutex.unlock();
	return ret;
}

std::string LockBasedHashTable::ToString() {
	return "";  // we do not care
}