/**
 * @file lock_free_hashtable.cpp
 * @author Josef Salzmann &	Aleksandar Hadzhiyski
 * @brief Implement a lock free hashtable based on
 * Ori Shalev, Nir Shavit: Split-ordered lists: Lock-free extensible hash tables. J. ACM 53(3): 379-405 (2006)
 * @date 2022-05-30
 */

#include "lock_free_hashtable.h"

/**
 * @brief Construct a new Lock Free Hash Table:: Lock Free Hash Table object
 * Initializing the underlying list yields one head node  with value 0
 * and one tail node with value UINT32_MAX. We just need to add another sentinel node
 * with value 1, so that we have two sentinel nodes in total. The tail node of the list
 * will be never accessed.
 */
LockFreeHashTable::LockFreeHashTable() {
	list = new LockFreeList();
	std::vector<TableEntry>* hashtable_init = new std::vector<TableEntry>(2);
	hashtable = new std::vector<TableEntry>();
	TableEntry first_htable_element = {};
	first_htable_element.sentinel_node = list->GetHead();
	(*hashtable_init)[0] = first_htable_element;
	TableEntry second_htable_element = {};
	KeyType second_sentinel_node_key = MakeSentinelKey(1);
	NodeType* second_sentinel_ptr = list->AddAndGetPointer(list->GetHead(), {second_sentinel_node_key, 1});
	second_htable_element.sentinel_node = second_sentinel_ptr;
	(*hashtable_init)[1] = second_htable_element;

	hashtable.store(hashtable_init, std::memory_order_seq_cst);

	table_size.store(0);
}

/**
 * @brief Return the unmarked pointer to the hashtable.
 *
 * @return std::vector<TableEntry>*
 */
std::vector<TableEntry>* LockFreeHashTable::GetHashtablePointer() {
	return static_cast<std::vector<TableEntry>*>(list->GetPointer(hashtable.load(std::memory_order_seq_cst)));
}

/**
 * @brief Basic hashfunction which seems to work out.
 * @param value the value that should be hashed.
 *
 * @return The hashed value.
 */
KeyType LockFreeHashTable::HashFunction(ValueType value) {
	uint32_t x = value;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = ((x >> 16) ^ x) * 0x45d9f3b;
	x = (x >> 16) ^ x;
	return x;
}

/**
 * @brief Gets the number of bits we are currently using for our sentinel nodes,
 * which is basically log2 of the number of sentinel nodes.
 *
 * @return uint32_t
 */
uint32_t LockFreeHashTable::GetNumberOfBitsUsed() {
	std::vector<TableEntry>* htable_ptr = GetHashtablePointer();
	uint32_t hashtable_size = (*htable_ptr).size();
	return (uint32_t)log2(hashtable_size);
}

/**
 * @brief Return the sentinel node a given value.
 *
 * @param value The value for which we want the sentinel node aka start node
 * @return NodeType*
 */
NodeType* LockFreeHashTable::GetSentinelNode(ValueType value) {
	uint32_t value_lower_bits = (uint32_t)value & (ALLONE >> (32 - GetNumberOfBitsUsed()));
	std::vector<TableEntry>* htable_ptr = GetHashtablePointer();
	return (*htable_ptr)[value_lower_bits].sentinel_node;
}

/**
 * @brief Add an element to the hashtable.
 * If the tablesize is bigger MAX_AVERAGE_BUCKET_SIZE * size(hashtable) we double the size of the table.
 *
 *
 * @param value Value to be added to the hashtable.
 * @return true
 * @return false
 */
bool LockFreeHashTable::Add(ValueType value) {
	NodeType* sentinel = GetSentinelNode(HashFunction(value));
	KeyType key = MakeNormalKey(value);
	bool success = list->Add(sentinel, {key, value});
	if (!success) {
		return false;
	} else {
		table_size++;  // actual table size and "table_size" are not updated atomically,
		    // but that should not be a problem since the resize regime is not that strict.
		std::vector<TableEntry>* htable_old = GetHashtablePointer();
		uint32_t permissibletablesize = MAX_AVERAGE_BUCKET_SIZE * (*htable_old).size();
		if ((uint32_t)table_size > permissibletablesize && !list->GetFlag(htable_old)) {
			std::vector<TableEntry>* marked_htable = htable_old;
			list->SetFlag((void**)&marked_htable);
			if (hashtable.compare_exchange_strong(htable_old, marked_htable)) {  // we set the mark of the hashtable pointer to
				                                                                 // 1 to indicate that we are about to double the table.
				                                                                 // So no two threads try to double the table at the same time.
				DoubleHashTableSize();
			}
		}

		return true;
	}
}

/**
 * @brief Check if a value is contained in the hashtable.
 *
 * @param value Value for which we check.
 * @return true
 * @return false
 */
bool LockFreeHashTable::Contains(ValueType value) {
	NodeType* sentinel = GetSentinelNode(HashFunction(value));
	KeyType key = MakeNormalKey(value);
	return list->Contains(sentinel, {key, value});
}

/**
 * @brief Add a sentinel node. Called while doubleing the table.
 *
 * @param value Value of the sentinel node.
 * @return NodeType* a pointer to the sentinel node.
 */
NodeType* LockFreeHashTable::AddSentinelNode(ValueType value) {
	NodeType* start = GetSentinelNode(value & ~(1 << GetNumberOfBitsUsed()));  // set MSB to zero
	KeyType key = MakeSentinelKey(value);
	auto ret = list->AddAndGetPointer(start, {key, value});
	return ret;
}

/**
 * @brief Double the hashtable by creating a new vector of table elements, copying the current
 * sentinel nodes and adding new ones. This is basically the main difference of our implementation
 * and the one presented in the paper. In the paper the sentinel nodes get initialized only when the 
 * are needed.
 */
void LockFreeHashTable::DoubleHashTableSize() {
	std::vector<TableEntry>* htable_ptr = GetHashtablePointer();
	uint32_t current_max_entry = (*htable_ptr).size();
	std::vector<TableEntry>* htable_new = new std::vector<TableEntry>(current_max_entry * 2);
	for (uint32_t i = 0; i < current_max_entry; i++) {
		(*htable_new)[i] = (*htable_ptr)[i];
	}
	for (uint32_t i = current_max_entry; i < current_max_entry * 2; i++) {
		NodeType* newSentinel = AddSentinelNode(i);
		(*htable_new)[i].sentinel_node = newSentinel;
	}
	hashtable.store(htable_new, std::memory_order_seq_cst);
}

/**
 * @brief Remove an element from the hashtable.
 *
 * @param value The value to be removed.
 * @return true
 * @return false
 */
bool LockFreeHashTable::Remove(ValueType value) {
	NodeType* sentinel = GetSentinelNode(HashFunction(value));
	KeyType key = MakeNormalKey(value);
	bool success = list->Remove(sentinel, {key, value});
	if (!success) {
		return false;
	} else {
		table_size--;  // actual table size and "table_size" are not updated atomically, but that should not be a problem since the resize regime is not that strict.
		return true;
	}
}

/**
 * @brief Make a normal key for a normal (i.e. not sentinel) value.
 * The key is basically the reversed masked hash with its LSB set to one.
 * This ensures that we are always bigger than the respective sentinel node.
 *
 * @param value The value for which we want to make a key.
 * @return KeyType
 */
KeyType LockFreeHashTable::MakeNormalKey(ValueType value) {
	KeyType key = HashFunction(value) & MASK;
	return Reverse(key | HIGH);
}

/**
 * @brief Make a sentinel key. The lowest bit of the key is never set.
 *
 * @param value
 * @return KeyType
 */
KeyType LockFreeHashTable::MakeSentinelKey(ValueType value) {
	return Reverse(value & MASK);
}

/**
 * @brief Method to reverse the bit orderd of an input value.
 *
 * @param input The value to be reversed.
 * @return KeyType
 */
KeyType LockFreeHashTable::Reverse(uint32_t input) {
	uint32_t no_bits = sizeof(input) * 8;
	uint32_t reverse_num = 0;
	for (uint32_t i = 0; i < no_bits; i++) {
		if ((input & (1 << i)))
			reverse_num |= 1 << ((no_bits - 1) - i);
	}
	return reverse_num;
}

/**
 * @brief Return the string of the list for some debugging.
 *
 * @return std::string
 */
std::string LockFreeHashTable::ToString() {
	return list->ToString();
}