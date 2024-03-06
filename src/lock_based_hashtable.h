#ifndef LOCK_BASED_HASHTABLE_H
#define LOCK_BASED_HASHTABLE_H

#include <mutex>
#include <unordered_map>

#include "lock_free_hashtable.h"
#include "lock_free_list.h"

class LockBasedHashTable : public HashTable {
   private:
	std::unordered_map<ValueType, ValueType> map;
	std::mutex mutex;

   public:
	LockBasedHashTable() : map({}){};
	bool Add(ValueType value) override;
	bool Remove(ValueType value) override;
	bool Contains(ValueType value) override;
	std::string ToString() override;
};

#endif