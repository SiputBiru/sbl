/* ============================================================================
 * SBL ARENA ALLOCATOR (Siput Biru Library)
 * ============================================================================
 *
 * A fast, STB-style single-header arena allocator for C/C++.
 *
 * HOW TO USE THIS LIBRARY:
 *
 * 1. In EXACTLY ONE C or C++ source file, define SBL_ARENA_IMPLEMENTATION
 * before including this header to create the actual implementation:
 *
 * #define SBL_ARENA_IMPLEMENTATION
 * #include "sbl_arena.h"
 *
 * 2. In all other files, just include the header normally:
 *
 * #include "sbl_arena.h"
 *
 * BASIC USAGE EXAMPLE:
 *
 * SblArena my_arena;
 * // Initialize with a 1 Megabyte initial block
 * sbl_arena_init(&my_arena, 1024 * 1024);
 *
 * // Allocate using standard functions
 * void* raw_mem = sbl_arena_alloc(&my_arena, 256);
 *
 * // Allocate using helpful macros (type-safe, automatically calculates size)
 * Player* p = SBL_ARENA_PUSH_STRUCT(&my_arena, Player);
 * Vector3* positions = SBL_ARENA_PUSH_ARRAY_ZERO(&my_arena, Vector3, 100);
 * char* name_copy = SBL_ARENA_PUSH_STRING(&my_arena, "Level Boss");
 *
 * // ... do work ...
 *
 * // Reset the arena (instantly "frees" all allocations, keeps memory for reuse)
 * sbl_arena_reset(&my_arena);
 *
 * // Free the underlying OS memory only when completely shutting down
 * sbl_arena_free(&my_arena);
 *
 * THREAD-LOCAL USAGE:
 *
 * // Automatically initializes a 1MB arena for the current thread
 * SblArena* ta = sbl_arena_get_thread();
 * void* temp_data = sbl_arena_alloc(ta, 512);
 *
 * // Call this at the end of your thread's frame/loop to wipe temporary data
 * sbl_arena_thread_reset();
 *
 * CONFIGURATION MACROS:
 *
 * Define any of these BEFORE including the implementation to override defaults:
 *
 * #define SBL_ARENA_MALLOC(size)         my_custom_malloc(size)
 * #define SBL_ARENA_FREE(ptr)            my_custom_free(ptr)
 * #define SBL_ARENA_ASSERT(x)            my_custom_assert(x)
 * #define SBL_ARENA_MEMSET(ptr, val, sz) my_custom_memset(ptr, val, sz)
 * #define SBL_ARENA_MEMCPY(dst, src, sz) my_custom_memcpy(dst, src, sz)
 * #define SBL_ARENA_STATIC               // Make implementation private to the file
 *
 * LICENSE:
 * See end of file for license information
 * ============================================================================
 */

#ifndef SBL_ARENA_H
#define SBL_ARENA_H

#include <stdint.h>

// ----------------------------------------------------------------------------
// STB-Style Configuration Macros
// ----------------------------------------------------------------------------

#ifdef SBL_ARENA_STATIC
#define SBL_ARENA_DEF static
#else
#define SBL_ARENA_DEF extern
#endif

#if defined(__cplusplus)
extern "C" {
#endif

// Allow to override the base OS allocator
#ifndef SBL_ARENA_MALLOC
#include <stdlib.h>
#define SBL_ARENA_MALLOC(sz) malloc(sz)
#define SBL_ARENA_FREE(p) free(p)
#endif

// Allow to override assert
#ifndef SBL_ARENA_ASSERT
#include <assert.h>
#define SBL_ARENA_ASSERT(x) assert(x)
#endif

// Allow to override memset/memcpy
#ifndef SBL_ARENA_MEMSET
#include <string.h>
#define SBL_ARENA_MEMSET(ptr, val, sz) memset(ptr, val, sz)
#define SBL_ARENA_MEMCPY(dst, src, sz) memcpy(dst, src, sz)
#endif

// ----------------------------------------------------------------------------
// API Macros & Types
// ----------------------------------------------------------------------------

#define SBL_ARENA_PUSH_STRUCT(arena, type) ((type*)sbl_arena_alloc((arena), sizeof(type)))
#define SBL_ARENA_PUSH_STRUCT_ZERO(arena, type) ((type*)sbl_arena_alloc_zero((arena), sizeof(type)))
#define SBL_ARENA_PUSH_ARRAY(arena, type, count)                                                   \
	((type*)sbl_arena_alloc((arena), sizeof(type) * (count)))
#define SBL_ARENA_PUSH_ARRAY_ZERO(arena, type, count)                                              \
	((type*)sbl_arena_alloc_zero((arena), sizeof(type) * (count)))

#define SBL_ARENA_PUSH_STRING(arena, str)                                                          \
	((char*)SBL_ARENA_MEMCPY(sbl_arena_alloc((arena), strlen(str) + 1), (str), strlen(str) + 1))

typedef struct SblArenaBlock {
	uint8_t* memory;
	uint64_t size;
	uint64_t offset;
	struct SblArenaBlock* next;
} SblArenaBlock;

typedef struct {
	SblArenaBlock* head;
	SblArenaBlock* current;
} SblArena;

typedef struct {
	SblArenaBlock* block;
	uint64_t offset;
} SblArenaMark;

// Thread-local arena (extern in header unless static)
#ifndef SBL_ARENA_STATIC
#if __STDC_VERSION__ >= 201112L
extern _Thread_local SblArena sbl_thread_arena;
extern _Thread_local int sbl_thread_arena_initialized;
#else
extern __thread SblArena sbl_thread_arena;
extern __thread int sbl_thread_arena_initialized;
#endif
#endif

// ----------------------------------------------------------------------------
// Function Declarations
// ----------------------------------------------------------------------------

SBL_ARENA_DEF void sbl_arena_init(SblArena* arena, uint64_t initial_size);
SBL_ARENA_DEF void sbl_arena_free(SblArena* arena);

SBL_ARENA_DEF void* sbl_arena_alloc_align(SblArena* arena, uint64_t size, uint64_t align);
SBL_ARENA_DEF void* sbl_arena_alloc(SblArena* arena, uint64_t size);
SBL_ARENA_DEF void* sbl_arena_alloc_zero(SblArena* arena, uint64_t size);

SBL_ARENA_DEF SblArenaMark sbl_arena_mark(SblArena* arena);
SBL_ARENA_DEF void sbl_arena_reset_to_mark(SblArena* arena, SblArenaMark mark);
SBL_ARENA_DEF void sbl_arena_reset(SblArena* arena);
SBL_ARENA_DEF uint64_t sbl_arena_get_used(SblArena* arena);

SBL_ARENA_DEF SblArena* sbl_arena_get_thread(void);
SBL_ARENA_DEF void sbl_arena_thread_reset(void);

#if defined(__cplusplus)
}
#endif

#endif // SBL_ARENA_H

// ============================================================
// IMPLEMENTATION
// ============================================================

#ifdef SBL_ARENA_IMPLEMENTATION

#if defined(__cplusplus)
extern "C" {
#endif

// Thread-local definitions
#if __STDC_VERSION__ >= 201112L
_Thread_local SblArena sbl_thread_arena;
_Thread_local int sbl_thread_arena_initialized = 0;
#else
__thread SblArena sbl_thread_arena;
__thread int sbl_thread_arena_initialized = 0;
#endif

// Internal helpers (namespaced with double underscore)
static uintptr_t sbl_arena__align_forward(uintptr_t ptr, uint64_t align) {
	SBL_ARENA_ASSERT(align && ((align & (align - 1)) == 0));
	return (ptr + align - 1) & ~((uintptr_t)align - 1);
}

static SblArenaBlock* sbl_arena__block_create(uint64_t size) {
	SblArenaBlock* block = (SblArenaBlock*)SBL_ARENA_MALLOC(sizeof(SblArenaBlock) + size);
	SBL_ARENA_ASSERT(block);

	block->memory = (uint8_t*)(block + 1);
	block->size = size;
	block->offset = 0;
	block->next = NULL;

	return block;
}

SBL_ARENA_DEF void sbl_arena_init(SblArena* arena, uint64_t initial_size) {
	arena->head = sbl_arena__block_create(initial_size);
	arena->current = arena->head;
}

SBL_ARENA_DEF void sbl_arena_free(SblArena* arena) {
	SblArenaBlock* block = arena->head;
	while (block) {
		SblArenaBlock* next = block->next;
		SBL_ARENA_FREE(block);
		block = next;
	}
	arena->head = NULL;
	arena->current = NULL;
}

SBL_ARENA_DEF uint64_t sbl_arena_get_used(SblArena* arena) {
	if (!arena || !arena->head)
		return 0;

	uint64_t total_used = 0;
	SblArenaBlock* block = arena->head;

	while (block) {
		total_used += block->offset;
		if (block == arena->current)
			break;
		block = block->next;
	}
	return total_used;
}

SBL_ARENA_DEF void* sbl_arena_alloc_align(SblArena* arena, uint64_t size, uint64_t align) {
	SBL_ARENA_ASSERT(size > 0);

	SblArenaBlock* block = arena->current;

	while (block) {
		uintptr_t current_ptr = (uintptr_t)(block->memory + block->offset);
		uintptr_t aligned_ptr = sbl_arena__align_forward(current_ptr, align);
		uint64_t adjustment = (uint64_t)(aligned_ptr - current_ptr);

		if ((block->offset + adjustment + size) <= block->size) {
			block->offset += adjustment;
			void* result = block->memory + block->offset;
			block->offset += size;

			arena->current = block;
			return result;
		}

		if (!block->next)
			break;
		block = block->next;
	}

	uint64_t new_size = block->size * 2;
	if (new_size < size + align) {
		new_size = size + align;
	}

	block->next = sbl_arena__block_create(new_size);
	block = block->next;

	uintptr_t ptr = (uintptr_t)(block->memory);
	uintptr_t aligned_ptr = sbl_arena__align_forward(ptr, align);
	uint64_t adjustment = (uint64_t)(aligned_ptr - ptr);

	block->offset = adjustment + size;
	arena->current = block;

	return (void*)(block->memory + adjustment);
}

SBL_ARENA_DEF void* sbl_arena_alloc(SblArena* arena, uint64_t size) {
	return sbl_arena_alloc_align(arena, size, 16);
}

SBL_ARENA_DEF void* sbl_arena_alloc_zero(SblArena* arena, uint64_t size) {
	void* ptr = sbl_arena_alloc(arena, size);
	SBL_ARENA_MEMSET(ptr, 0, size);
	return ptr;
}

SBL_ARENA_DEF SblArenaMark sbl_arena_mark(SblArena* arena) {
	SblArenaMark mark;
	mark.block = arena->current;
	mark.offset = arena->current->offset;
	return mark;
}

SBL_ARENA_DEF void sbl_arena_reset_to_mark(SblArena* arena, SblArenaMark mark) {
	mark.block->offset = mark.offset;
	SblArenaBlock* block = mark.block->next;
	while (block) {
		block->offset = 0;
		block = block->next;
	}
	arena->current = mark.block;
}

SBL_ARENA_DEF void sbl_arena_reset(SblArena* arena) {
	if (!arena->head)
		return;
	SblArenaBlock* block = arena->head;
	while (block) {
		block->offset = 0;
		block = block->next;
	}
	arena->current = arena->head;
}

SBL_ARENA_DEF SblArena* sbl_arena_get_thread(void) {
	if (!sbl_thread_arena_initialized) {
		sbl_arena_init(&sbl_thread_arena, 1024 * 1024);
		sbl_thread_arena_initialized = 1;
	}
	return &sbl_thread_arena;
}

SBL_ARENA_DEF void sbl_arena_thread_reset(void) {
	if (sbl_thread_arena_initialized) {
		sbl_arena_reset(&sbl_thread_arena);
	}
}

#if defined(__cplusplus)
}
#endif

#endif // SBL_ARENA_IMPLEMENTATION

/*
 * ------------------------------------------------------------------------------
 * This software is available under 2 licenses -- choose whichever you prefer.
 * ------------------------------------------------------------------------------
 * ALTERNATIVE A - Zlib license
 * Copyright (c) 2026 Raditya Mahatma (SiputBiru)
 *
 * This software is provided "as-is", without any express or implied warranty. In no event
 * will the authors be held liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not claim that you
 * wrote the original software. If you use this software in a product, an acknowledgment
 * in the product documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be misrepresented
 * as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 * ------------------------------------------------------------------------------
 * ALTERNATIVE B - Public Domain (www.unlicense.org)
 * This is free and unencumbered software released into the public domain.
 * Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
 * software, either in source code form or as a compiled binary, for any purpose,
 * commercial or non-commercial, and by any means.
 * In jurisdictions that recognize copyright laws, the author or authors of this
 * software dedicate any and all copyright interest in the software to the public
 * domain. We make this dedication for the benefit of the public at large and to
 * the detriment of our heirs and successors. We intend this dedication to be an
 * overt act of relinquishment in perpetuity of all present and future rights to
 * this software under copyright law.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ------------------------------------------------------------------------------
 */
