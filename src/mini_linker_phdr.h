/*
 * mini_linker_phdr.h
 *
 *  Created on: 2018年6月4日
 *      Author: loulei
 */

#ifndef MINI_LINKER_PHDR_H_
#define MINI_LINKER_PHDR_H_

#include <stdbool.h>
#include <elf.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <asm-generic/mman-common.h>
#include <errno.h>
#include "common.h"
#include "Loader.h"

#define MAYBE_MAP_FLAG(x,from,to)    (((x) & (from)) ? (to) : 0)
#define PFLAGS_TO_PROT(x)            (MAYBE_MAP_FLAG((x), PF_X, PROT_EXEC) | \
                                      MAYBE_MAP_FLAG((x), PF_R, PROT_READ) | \
                                      MAYBE_MAP_FLAG((x), PF_W, PROT_WRITE))


const char *name_;
int fd_;
Elf32_Ehdr header_;
size_t phdr_num_;

void *phdr_mmap_;
Elf32_Phdr *phdr_table_;
Elf32_Addr phdr_size_;

void *load_start_;
Elf32_Addr load_size_;
Elf32_Addr load_bias_;

const Elf32_Phdr *loaded_phdr_;


bool ReadElfHeader();
bool VerifyElfHeader();
bool ReadProgramHeader();
bool ReserveAddressSpace();
bool LoadSegments();
bool FindPhdr();
bool CheckPhdr(Elf32_Addr);


void init(char *name, int fd);
void fini();

bool Load();
//size_t phdr_count(){
//	return phdr_num_;
//}
//
//Elf32_Addr load_start(){
//	return load_start_;
//}
//
//Elf32_Addr load_size(){
//	return load_size_;
//}
//
//Elf32_Addr load_bias(){
//	return load_bias_;
//}
//
//const Elf32_Phdr *loaded_phdr(){
//	return loaded_phdr_;
//}


size_t
phdr_table_get_load_size(const Elf32_Phdr* phdr_table,
                         size_t phdr_count,
                         Elf32_Addr* min_vaddr,
                         Elf32_Addr* max_vaddr);

int
phdr_table_protect_segments(const Elf32_Phdr* phdr_table,
                            int               phdr_count,
                            Elf32_Addr        load_bias);

int
phdr_table_unprotect_segments(const Elf32_Phdr* phdr_table,
                              int               phdr_count,
                              Elf32_Addr        load_bias);

int
phdr_table_protect_gnu_relro(const Elf32_Phdr* phdr_table,
                             int               phdr_count,
                             Elf32_Addr        load_bias);

int
phdr_table_get_arm_exidx(const Elf32_Phdr* phdr_table,
                         int               phdr_count,
                         Elf32_Addr        load_bias,
                         Elf32_Addr**      arm_exidx,
                         unsigned*         arm_exidix_count);

void
phdr_table_get_dynamic_section(const Elf32_Phdr* phdr_table,
                               int               phdr_count,
                               Elf32_Addr        load_bias,
                               Elf32_Dyn**       dynamic,
                               size_t*           dynamic_count,
                               Elf32_Word*       dynamic_flags);


#endif /* MINI_LINKER_PHDR_H_ */
