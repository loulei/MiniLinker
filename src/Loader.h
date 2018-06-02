/*
 * Loader.h
 *
 *  Created on: 2018年5月29日
 *      Author: loulei
 */

#ifndef LOADER_H_
#define LOADER_H_

#include <unistd.h>
#include <sys/types.h>
#include <elf.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include "common.h"



#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define PAGE_MASK (~(PAGE_SIZE-1))
// Returns the address of the page containing address 'x'.
#define PAGE_START(x)  ((x) & PAGE_MASK)

// Returns the offset of address 'x' in its page.
#define PAGE_OFFSET(x) ((x) & ~PAGE_MASK)

// Returns the address of the next page after address 'x', unless 'x' is
// itself at the start of a page.
#define PAGE_END(x)    PAGE_START((x) + (PAGE_SIZE-1))

#define SOINFO_NAME_LEN 128

typedef struct _soinfo soinfo;

typedef void (*linker_function_t)();

typedef struct _link_map_t link_map_t;

struct _link_map_t {
	uintptr_t l_addr;
	char *l_name;
	uintptr_t l_ld;
	link_map_t *l_next;
	link_map_t *l_prev;
};

struct _soinfo{
	char name[SOINFO_NAME_LEN];
	const Elf32_Phdr *phdr;
	size_t phnum;
	Elf32_Addr entry;
	Elf32_Addr base;
	unsigned size;

	uint32_t unused1;

	Elf32_Dyn *dynamic;

	uint32_t unused2;
	uint32_t unused3;

	soinfo *next;
	unsigned flags;

	const char *strtab;
	Elf32_Sym *symtab;

	size_t nbucket;
	size_t nchain;

	unsigned *bucket;
	unsigned *chain;

	unsigned *plt_got;

	Elf32_Rel *plt_rel;
	size_t plt_rel_count;

	Elf32_Rel *rel;
	size_t rel_count;

	linker_function_t *preinit_array;
	size_t preinit_array_count;

	linker_function_t *init_array;
	size_t init_array_count;
	linker_function_t *fini_array;
	size_t fini_array_count;

	linker_function_t init_func;
	linker_function_t fini_func;

	unsigned *ARM_exidx;
	size_t ARM_exidx_count;

	size_t ref_count;
	link_map_t link_map;

	bool constructors_called;

	// When you read a virtual address from the ELF file, add this
	// value to get the corresponding address in the process' address space.
	Elf32_Addr load_bias;

	bool has_text_relocations;
	bool has_DT_SYMBOLIC;

	void CallConstructors();
	void CallDestructors();
	void CallPreInitConstructors();

	void CallArray(const char* array_name, linker_function_t* functions, size_t count, bool reverse);
	void CallFunction(const char* function_name, linker_function_t function);
};

extern soinfo libdl_info;

#endif /* LOADER_H_ */














