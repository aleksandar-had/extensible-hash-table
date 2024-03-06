/**
 * @file lock_free_list.cpp
 * @author Josef Salzmann &	Aleksandar Hadzhiyski
 * @brief Lock free list as presented in the slides.
 * With the addition of a "start" node from which the Find() method will start.
 * The "start" node will of course be the respective sentinel node of an entry.
 * Also the method AddAndGetPointer() has been added to add sentinel nodes
 * and return pointers to them.
 * @date 2022-05-30
 */
#include "lock_free_list.h"

/**
 * @brief Contains method as from the slides, only that the starting node is a sentinel
 * node supplied by the hashtable
 *
 * @param start
 * @param item
 * @return true
 * @return false
 */
bool LockFreeList::Contains(NodeType* start, KeyValue item) {
	// same as lazy implementation
	// except marked flag is part of next pointer
	NodeType* n = start;
	while (n != nullptr && n->item < item) {
		n = static_cast<NodeType*>(GetPointer(n->next));
	}
	if (n == nullptr)
		return false;
	return n->item == item && !GetFlag(n->next);
}

/**
 * @brief Return the head element of the list.
 * Needed for the initializiation of the hashtable.
 *
 * @return NodeType*
 */
NodeType* LockFreeList::GetHead() {
	return head;
}

/**
 * @brief Find method from the slides only that the starting node is a sentinel 
 * node supplied by the hashtable
 *
 * @param start
 * @param item
 * @return Window
 */
Window LockFreeList::Find(NodeType* start, KeyValue item) {
	// Search for item or successor
	while (true) {
		NodeType* pred = start;
		NodeType* curr = static_cast<NodeType*>(GetPointer(pred->next));

		while (true) {
			if (curr->next == nullptr) {  // we are at the end of the list
				return {pred, curr};
			}
			NodeType* succ = static_cast<NodeType*>(GetPointer(curr->next));
			while (GetFlag(curr->next)) {
				ResetFlag((void**)&curr);
				ResetFlag((void**)&succ);
				if (pred->next.compare_exchange_strong(curr, succ))
					break;

				curr = succ;
				succ = static_cast<NodeType*>(GetPointer(succ->next));
			}
			if (curr->item >= item) {
				return {pred, curr};
			}
			pred = curr;
			curr = static_cast<NodeType*>(GetPointer(curr->next));
		}
	}
}

/**
 * @brief Add method from the slides only that the starting node is a sentinel 
 * node supplied by the hashtable
 *
 * @param start
 * @param item
 * @return true
 * @return false
 */
bool LockFreeList::Add(NodeType* start, KeyValue item) {
	Window w;

	NodeType* n = new NodeType();
	n->item = item;
	n->mark = false;
	n->next = nullptr;

	while (true) {
		w = Find(start, item);
		NodeType* pred = w.pred;
		NodeType* curr = w.curr;

		if (curr != nullptr && curr->item == item) {
			delete (n);
			return false;
		}

		n->next = curr;

		// unmark new node
		ResetFlag((void**)&n->next);
		ResetFlag((void**)&curr);

		if (pred->next.compare_exchange_strong(curr, n))
			return true;
	}
}

/**
 * @brief Basically the same method ass Add(), only that we return a pointer 
 * into the list to the inserted element. Used for adding sentinel nodes.
 *
 * @param start
 * @param item
 * @return NodeType*
 */
NodeType* LockFreeList::AddAndGetPointer(NodeType* start, KeyValue item) {
	Window w;

	NodeType* n = new NodeType();
	n->item = item;
	n->mark = false;
	n->next = nullptr;

	while (true) {
		w = Find(start, item);
		NodeType* pred = w.pred;
		NodeType* curr = w.curr;

		if (curr != nullptr && curr->item == item) {
			delete (n);
			return nullptr;
		}

		n->next = curr;

		// unmark new node
		ResetFlag((void**)&n->next);
		ResetFlag((void**)&curr);

		if (pred->next.compare_exchange_strong(curr, n))
			return n;
	}
}

/**
 * @brief Remove method from the slides only that the starting node is a sentinel 
 * node supplied by the hashtable
 *
 * @param start
 * @param item
 * @return true
 * @return false
 */
bool LockFreeList::Remove(NodeType* start, KeyValue item) {
	Window w;

	while (true) {
		w = Find(start, item);
		if (w.curr == nullptr || item != w.curr->item)
			return false;

		NodeType* succ = w.curr->next;
		NodeType* markedsucc = succ;
		// mark as deleted
		SetFlag((void**)&markedsucc);
		ResetFlag((void**)&succ);
		if (!w.curr->next.compare_exchange_strong(succ, markedsucc))
			continue;
		// attempt to unlink curr
		w.pred->next.compare_exchange_strong(w.curr, succ);
		return true;
	}
}

/**
 * @brief For marked pointers we use the LSB of the pointer as a mark.
 *
 * @param markedpointer
 * @return void*
 */
void* LockFreeList::GetPointer(void* markedpointer) {
	return (void*)(((uintptr_t)markedpointer) & ~1);
}

bool LockFreeList::GetFlag(void* markedpointer) {
	return ((uintptr_t)markedpointer) & 1;
}

void LockFreeList::SetFlag(void** markedpointer) {
	(*markedpointer) = (void*)((uintptr_t)(*markedpointer) | 1);
}

void LockFreeList::ResetFlag(void** markedpointer) {
	(*markedpointer) = (void*)((uintptr_t)(*markedpointer) & ~1);
}

std::string LockFreeList::ToString() {
	NodeType* current_node = head;
	std::stringstream ss;
	int count = 0;
	while (current_node != nullptr) {
		ss << "Node " << count << ": ";
		if ((current_node->item.key & 0x1) == 0)
			ss << "Sentinel-Node ";
		ss << "Key " << current_node->item.key
		   << ", Value " << current_node->item.value
		   << ", Mark " << current_node->mark << "\n";
		count++;
		current_node = current_node->next;
	}

	return ss.str();
}