#include "mini_linker_phdr.h"

void init(char *name, int fd){
	name_ = name;
	fd_ = fd;
	phdr_num_ = 0;
	phdr_mmap_ = NULL;
	phdr_table_ = NULL;
	phdr_size_ = 0;
	load_start_ = NULL;
	load_size_ = 0;
	load_bias_ = 0;
	loaded_phdr_ = NULL;
}

void fini(){
	if(fd_ != -1){
		close(fd_);
	}

	if(phdr_mmap_ != NULL){
		munmap(phdr_mmap_, phdr_size_);
	}
}

bool Load(){
	return ReadElfHeader() &&
			VerifyElfHeader() &&
			ReadProgramHeader() &&
			ReserveAddressSpace() &&
			LoadSegments() &&
			FindPhdr();
}

bool ReadElfHeader(){
	ssize_t rc = read(fd_, &header_, sizeof(header_));
	if(rc < 0){
		LOGD("can not read file %s", name_, strerror(errno));
		return false;
	}
	if(rc != sizeof(header_)){
		LOGD("%s is too small to be an elf executable", name_);
		return false;
	}
	return true;
}

bool VerifyElfHeader(){
	if (header_.e_ident[EI_MAG0] != ELFMAG0 ||
	      header_.e_ident[EI_MAG1] != ELFMAG1 ||
	      header_.e_ident[EI_MAG2] != ELFMAG2 ||
	      header_.e_ident[EI_MAG3] != ELFMAG3){
		LOGD("%s has bad elf magic", name_);
	}

	if (header_.e_ident[EI_CLASS] != ELFCLASS32) {
		LOGD("%s not 32-bit: %d", name_, header_.e_ident[EI_CLASS]);
	    return false;
	}

	if (header_.e_ident[EI_DATA] != ELFDATA2LSB) {
		LOGD("%s not little-endian: %d", name_, header_.e_ident[EI_DATA]);
	    return false;
	}

	if (header_.e_type != ET_DYN) {
		LOGD("%s has unexpected e_type: %d", name_, header_.e_type);
		return false;
	}

	if (header_.e_version != EV_CURRENT) {
		LOGD("%s has unexpected e_version: %d", name_, header_.e_version);
		return false;
	}

	return true;
}

bool ReadProgramHeader(){
	phdr_num_ = header_.e_phnum;
	if(phdr_num_ < 1 || phdr_num_ > 65536/sizeof(Elf32_Phdr)){
		LOGD("%s has invalid e_phnum: %d", name_, phdr_num_);
		return false;
	}

	Elf32_Addr page_min = PAGE_START(header_.e_phoff);
	Elf32_Addr page_max = PAGE_END(header_.e_phoff + (phdr_num_ * sizeof(Elf32_Phdr)));
	Elf32_Addr page_offset = PAGE_OFFSET(header_.e_phoff);

	phdr_size_ = page_max - page_min;

	LOGD("page_min=0x%08x, page_max=0x%08x, page_offset=0x%08x", page_min, page_max, page_offset);

	void *mmap_result = mmap(NULL, phdr_size_, PROT_READ, MAP_PRIVATE, fd_, page_min);
	if(mmap_result == MAP_FAILED){
		LOGD("%s phdr mmap failed: %s", name_, strerror(errno));
		return false;
	}

	phdr_mmap_ = mmap_result;
	phdr_table_ = mmap_result + page_offset;
	return true;
}

size_t
phdr_table_get_load_size(const Elf32_Phdr* phdr_table,
                         size_t phdr_count,
                         Elf32_Addr* out_min_vaddr,
                         Elf32_Addr* out_max_vaddr){
	Elf32_Addr min_vaddr = 0xFFFFFFFFU;
	Elf32_Addr max_vaddr = 0x00000000U;

	bool found_pt_load = false;
	for(size_t i=0; i<phdr_count; i++){
		const Elf32_Phdr *phdr = &phdr_table_[i];
		if(phdr->p_type != PT_LOAD){
			continue;
		}
		found_pt_load = true;
		if(phdr->p_vaddr < min_vaddr){
			min_vaddr = phdr->p_vaddr;
		}
		if(phdr->p_paddr + phdr->p_memsz > max_vaddr){
			max_vaddr = phdr->p_paddr + phdr->p_memsz;
		}
	}
	if(!found_pt_load){
		min_vaddr = 0x00000000U;
	}
	min_vaddr = PAGE_START(min_vaddr);
	max_vaddr = PAGE_END(max_vaddr);

	if(out_min_vaddr != NULL){
		*out_min_vaddr = min_vaddr;
	}
	if(out_max_vaddr != NULL){
		*out_max_vaddr = max_vaddr;
	}
	return max_vaddr - min_vaddr;
}

bool ReserveAddressSpace(){
	Elf32_Addr min_vaddr;
	load_size_ = phdr_table_get_load_size(phdr_table_, phdr_num_, &min_vaddr, NULL);
	if(load_size_ == 0){
		LOGD("%s has no loadable segments", name_);
		return false;
	}
	uint8_t *addr = min_vaddr;
	int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;
	void *start = mmap(addr, load_size_, PROT_NONE, mmap_flags, -1, 0);
	if(start == MAP_FAILED){
		LOGD("could not reserve %d bytes of address space for %s", load_size_, name_);
		return false;
	}
	load_start_ = start;
	load_bias_ = (uint8_t*)start - addr;
	LOGD("load_start=0x%08x, load_bias=0x%08x", load_start_, load_bias_);
	return true;
}

bool LoadSegments(){
	for(size_t i=0; i<phdr_num_; i++){
		const Elf32_Phdr *phdr = &phdr_table_[i];
		if(phdr->p_type != PT_LOAD){
			continue;
		}
		Elf32_Addr seg_start = phdr->p_vaddr + load_bias_;
		Elf32_Addr seg_end = seg_start + phdr->p_memsz;

		Elf32_Addr seg_page_start = PAGE_START(seg_start);
		Elf32_Addr seg_page_end = PAGE_END(seg_end);

		Elf32_Addr seg_file_end = seg_start + phdr->p_filesz;

		Elf32_Addr file_start = phdr->p_offset;
		Elf32_Addr file_end = file_start + phdr->p_filesz;

		Elf32_Addr file_page_start = PAGE_START(file_start);
		Elf32_Addr file_length = file_end - file_page_start;

		if(file_length != 0){
			void *seg_addr = mmap((void*)seg_page_start, file_length, PFLAGS_TO_PROT(phdr->p_flags), MAP_FIXED|MAP_PRIVATE, fd_, file_page_start);
			if(seg_addr == MAP_FAILED){
				LOGD("could not map %s segmen %d: %s", name_, i, strerror(errno));
				return false;
			}
		}

		if((phdr->p_flags & PF_W) != 0 && PAGE_OFFSET(seg_file_end) > 0){
			memset((void*)seg_file_end, 0, PAGE_SIZE - PAGE_OFFSET(seg_file_end));
		}

		seg_file_end = PAGE_END(seg_file_end);

		if(seg_page_end > seg_file_end){
			void* zeromap = mmap((void*)seg_file_end,
			                           seg_page_end - seg_file_end,
			                           PFLAGS_TO_PROT(phdr->p_flags),
			                           MAP_FIXED|MAP_ANONYMOUS|MAP_PRIVATE,
			                           -1,
			                           0);
			if (zeromap == MAP_FAILED) {
				LOGD("couldn't zero fill %s gap: %s", name_, strerror(errno));
				return false;
			}
		}
	}
	return true;
}

bool CheckPhdr(Elf32_Addr loaded){
	const Elf32_Phdr *phdr_limit = phdr_table_ + phdr_num_;
	Elf32_Addr loaded_end = loaded + (phdr_num_ * sizeof(Elf32_Phdr));
	for(Elf32_Phdr *phdr = phdr_table_; phdr < phdr_limit; phdr++){
		if(phdr->p_type != PT_LOAD){
			continue;
		}
		Elf32_Addr seg_start = phdr->p_vaddr + load_bias_;
		Elf32_Addr seg_end = phdr->p_filesz + seg_start;
		if(seg_start <= loaded && loaded_end <= seg_end){
			loaded_phdr_ = loaded;
			return true;
		}
	}
	LOGD("%s loaded phdr %x not in loadable segment", name_, loaded);
	return false;
}

bool FindPhdr(){
	const Elf32_Phdr *phdr_limit = phdr_table_ + phdr_num_;
	for(const Elf32_Phdr *phdr = phdr_table_; phdr < phdr_limit; ++phdr){
		if(phdr->p_type == PT_PHDR){
			return CheckPhdr(load_bias_ + phdr->p_vaddr);
		}
	}

	for(const Elf32_Phdr *phdr = phdr_table_; phdr < phdr_limit; ++phdr){
		if(phdr->p_type == PT_LOAD){
			if(phdr->p_offset == 0){
				Elf32_Addr elf_addr = load_bias_ + phdr->p_vaddr;
				const Elf32_Ehdr *ehdr = (const Elf32_Ehdr*)(void*)elf_addr;
				Elf32_Addr offset = ehdr->e_phoff;
				return CheckPhdr((Elf32_Addr)ehdr + offset);
			}
			break;
		}
	}
	LOGD("can not find loaded phdr for %s", name_);
	return false;
}

void
phdr_table_get_dynamic_section(const Elf32_Phdr* phdr_table,
                               int               phdr_count,
                               Elf32_Addr        load_bias,
                               Elf32_Dyn**       dynamic,
                               size_t*           dynamic_count,
                               Elf32_Word*       dynamic_flags){
	const Elf32_Phdr *phdr = phdr_table;
	const Elf32_Phdr *phdr_limit = phdr + phdr_count;
	for(phdr = phdr_table; phdr < phdr_limit; phdr++){
		if(phdr->p_type != PT_DYNAMIC){
			continue;
		}
		*dynamic = load_bias + phdr->p_vaddr;
		if(dynamic_count){
			*dynamic_count = (unsigned)(phdr->p_memsz / 8);
		}
		if(dynamic_flags){
			*dynamic_flags = phdr->p_flags;
		}
		return;
	}
	*dynamic = NULL;
	if(dynamic_count){
		*dynamic_count = 0;
	}
}

int
phdr_table_get_arm_exidx(const Elf32_Phdr* phdr_table,
                         int               phdr_count,
                         Elf32_Addr        load_bias,
                         Elf32_Addr**      arm_exidx,
                         unsigned*         arm_exidix_count){
	const Elf32_Phdr *phdr = phdr_table;
	const Elf32_Phdr *phdr_limit = phdr_table + phdr_count;
	for(phdr = phdr_table; phdr < phdr_limit; phdr++){
		if(phdr->p_type != PT_ARM_EXIDX)
			continue;

		*arm_exidx = (Elf32_Addr*)(load_bias + phdr->p_vaddr);
		*arm_exidix_count = (unsigned)(phdr->p_memsz / 8);
		return 0;
	}
	*arm_exidix_count = 0;
	*arm_exidx = NULL;
	return -1;
}












