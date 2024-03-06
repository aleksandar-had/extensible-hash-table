#ifndef LOCK_FREE_LIST_H
#define LOCK_FREE_LIST_H

#include <stdint.h>

#include <atomic>
#include <iostream>
#include <sstream>
#include <string>

typedef uint32_t KeyType;
typedef uint32_t ValueType;

struct KeyValue {
	KeyType key;
	ValueType value;

	bool operator==(const KeyValue& a) const {
		return key == a.key && value == a.value;
	}

	bool operator!=(const KeyValue& a) const {
		return key != a.key || value != a.value;
	}

	bool operator>=(const KeyValue& a) const {
		if (key > a.key)
			return true;
		else if (key == a.key)
			return value >= a.value;
		else
			return false;
	}

	bool operator<(const KeyValue& a) const {
		if (key < a.key)
			return true;
		else if (key == a.key)
			return value < a.value;
		else
			return false;
	}
};

struct NodeType {
	KeyValue item;
	bool mark;
	std::atomic<NodeType*> next;
};

struct Window {
	NodeType* pred;
	NodeType* curr;
};

class LockFreeList {
   private:
	std::atomic<NodeType*> head;
	Window Find(NodeType* start, KeyValue item);

   public:
	LockFreeList() : head(new NodeType()) {
		NodeType* tail_imm = new NodeType();
		tail_imm->item.key = UINT32_MAX;
		tail_imm->item.value = UINT32_MAX;  // HashFunction(UINT32_MAX) < UINT32_MAX, so we know that no element comes after this one
		tail_imm->mark = false;
		tail_imm->next.store(nullptr);
		NodeType* head_imm = new NodeType();
		head_imm->item.key = 0;
		head_imm->item.value = 0;
		head_imm->mark = false;
		head_imm->next.store(tail_imm);
		head.store(head_imm);
	};
	bool Contains(NodeType* start, KeyValue item);
	bool Add(NodeType* start, KeyValue item);
	NodeType* AddAndGetPointer(NodeType* start, KeyValue item);
	bool Remove(NodeType* start, KeyValue item);
	NodeType* GetHead();
	void* GetPointer(void* markedpointer);
	bool GetFlag(void* markedpointer);
	void SetFlag(void** markedpointer);
	void ResetFlag(void** markedpointer);
	std::string ToString();
};

#endif