#include <assert.h>
#include <stdint.h>
#include <stdio.h>

// Define the implementation BEFORE including the header
#define SBL_ARENA_IMPLEMENTATION
#include "../sbl_arena.h"

void test_basic_and_alignment() {
	SblArena arena;
	sbl_arena_init(&arena, 1024); // 1KB arena

	// Test basic allocation
	void* ptr1 = sbl_arena_alloc(&arena, 10);
	assert(ptr1 != NULL);

	// Test alignment (force an odd offset first)
	sbl_arena_alloc(&arena, 3);

	// Request a 16-byte aligned block
	void* ptr_aligned = sbl_arena_alloc_align(&arena, 20, 16);

	// Mathematically verify the pointer is a multiple of 16
	assert(((uintptr_t)ptr_aligned % 16) == 0);
	printf("[OK] Alignment test passed.\n");

	sbl_arena_free(&arena);
}

void test_zero_allocation() {
	SblArena arena;
	sbl_arena_init(&arena, 1024);

	// Allocate 100 bytes of zeroed memory
	int size = 100;
	uint8_t* zeroes = (uint8_t*)sbl_arena_alloc_zero(&arena, size);

	// Verify every single byte is exactly 0
	for (int i = 0; i < size; i++) {
		assert(zeroes[i] == 0);
	}
	printf("[OK] Zero allocation passed.\n");

	sbl_arena_free(&arena);
}

void test_arena_growth() {
	SblArena arena;
	sbl_arena_init(&arena, 100); // Start TINY (100 bytes)

	// Fill up the first block
	sbl_arena_alloc(&arena, 80);
	assert(arena.head == arena.current); // Still in the first block

	// Ask for 200 bytes (forces the arena to grow)
	void* big_chunk = sbl_arena_alloc(&arena, 200);
	assert(big_chunk != NULL);

	// Verify it actually created a new block
	assert(arena.head != arena.current);
	assert(arena.head->next == arena.current);

	printf("[OK] Arena growth passed.\n");
	sbl_arena_free(&arena);
}

void test_bookmarks() {
	SblArena arena;
	sbl_arena_init(&arena, 1024);

	sbl_arena_alloc(&arena, 100);
	uint64_t used_before = sbl_arena_get_used(&arena);

	// Create a bookmark
	SblArenaMark mark = sbl_arena_mark(&arena);

	// Allocate some temporary garbage
	sbl_arena_alloc(&arena, 500);
	assert(sbl_arena_get_used(&arena) > used_before);

	// Rollback to the bookmark
	sbl_arena_reset_to_mark(&arena, mark);

	// Verify the used memory went exactly back to where it was
	assert(sbl_arena_get_used(&arena) == used_before);

	printf("[OK] Bookmarks & Rollback passed.\n");
	sbl_arena_free(&arena);
}

int main() {
	printf("--- Running SBL Arena Tests ---\n");

	test_basic_and_alignment();
	test_zero_allocation();
	test_arena_growth();
	test_bookmarks();

	// Test Thread Local
	SblArena* thread_arena = sbl_arena_get_thread();
	assert(thread_arena != NULL);
	sbl_arena_thread_reset();
	printf("[OK] Thread-local arena passed.\n");

	printf("--- ALL TESTS PASSED SUCCESSFULLY! ---\n");
	return 0;
}
