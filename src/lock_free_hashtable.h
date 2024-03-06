#ifndef LOCK_FREE_HASHTABLE_H
#define LOCK_FREE_HASHTABLE_H

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <atomic>
#include <vector>

#include "lock_free_list.h"

typedef uint32_t ValueType;
typedef uint32_t KeyType;

struct TableEntry {
	NodeType* sentinel_node;
};

class HashTable {
   public:
	virtual ~HashTable() {}
	virtual bool Add(ValueType value) = 0;
	virtual bool Remove(ValueType value) = 0;
	virtual bool Contains(ValueType value) = 0;
	virtual std::string ToString() = 0;
};

class LockFreeHashTable : public HashTable {
   private:
	LockFreeList* list;
	std::atomic<std::vector<TableEntry>*> hashtable;
	const uint32_t MAX_AVERAGE_BUCKET_SIZE = 4;  // if table_size > MAX_AVERAGE_BUCKET_SIZE * size(hashtable) then we double the number of hashtable entries
	const uint32_t HIGH = 0x80000000;
	const uint32_t MASK = 0x00FFFFFF;
	const uint32_t ALLONE = 0xFFFFFFFF;
	std::atomic<uint32_t> table_size;  // number of elements in the table without sentinel nodes
	KeyType HashFunction(ValueType value);
	KeyType MakeNormalKey(ValueType value);
	KeyType MakeSentinelKey(KeyType key);
	KeyType Reverse(KeyType input);
	void DoubleTableSize();
	NodeType* GetSentinelNode(ValueType item);
	uint32_t GetNumberOfBitsUsed();
	NodeType* AddSentinelNode(ValueType value);
	std::vector<TableEntry>* GetHashtablePointer();
	void DoubleHashTableSize();

   public:
	LockFreeHashTable();
	LockFreeHashTable(const LockFreeHashTable& lock_free_hashtable);
	bool Add(ValueType value) override;
	bool Remove(ValueType value) override;
	bool Contains(ValueType value) override;
	std::string ToString() override;
	LockFreeHashTable& operator=(const LockFreeHashTable& a);  // make cppcheck happy
};

#endif
