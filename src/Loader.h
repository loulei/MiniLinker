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
};

extern soinfo libdl_info;

#endif /* LOADER_H_ */














