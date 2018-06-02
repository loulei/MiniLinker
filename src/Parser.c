#include "Parser.h"

static unsigned elfhash(const char* _name) {
    const unsigned char* name = (const unsigned char*) _name;
    unsigned h = 0, g;

    while(*name) {
        h = (h << 4) + *name++;
        g = h & 0xf0000000;
        h ^= g;
        h ^= g >> 24;
    }
    return h;
}

void parseElf(char *filename){
	LOGD("start parse elf %s\n", filename);

	FILE *fp = fopen(filename, "rb");
	if(!fp){
		LOGD("fopen %s fail\n", filename);
		return;
	}

	Elf32_Ehdr ehdr;
	Elf32_Shdr *sh_table;
	Elf32_Shdr sh_str;
	Elf32_Shdr sh_dynstr;
	Elf32_Shdr sh_sym;
	Elf32_Sym *sym_table;
	Elf32_Shdr sh_hash;
	char *str;
	char *dynstr;
	Elf32_Word nbucket;
	Elf32_Word nchain;
	Elf32_Word *buchets;
	Elf32_Word *chains;


	Elf32_Phdr *ph_table;
	Elf32_Phdr dynamic;
	Elf32_Dyn *dyn_table;

	int i;

	fread(&ehdr, sizeof(Elf32_Ehdr), 1, fp);

	LOGD("type=%d\n", ehdr.e_type); 		// #define ET_DYN		3
	LOGD("machine=%d\n", ehdr.e_machine); // #define EM_ARM		40
	LOGD("entry=0x%08x\n", ehdr.e_entry); // 0x0
	LOGD("phoff=0x%08x\n", ehdr.e_phoff); // 0x34
	LOGD("phnum=%d\n", ehdr.e_phnum);		// 8
	LOGD("phentsize=%d\n", ehdr.e_phentsize); // 32
	LOGD("shoff=0x%08x\n", ehdr.e_shoff); // 0x51dc
	LOGD("shnum=%d\n", ehdr.e_shnum);		// 27
	LOGD("shentsize=%d\n", ehdr.e_shentsize);	// 40
	LOGD("ehsize=%d\n", ehdr.e_ehsize);	// 52 == e_phoff
	LOGD("shstrndx=%d\n", ehdr.e_shstrndx); // 26 == (e_shnum-1)

	LOGD("-------------------- parse section -------------------\n");

	LOGD("1. find strtab\n");
	fseek(fp, ehdr.e_shoff+ehdr.e_shentsize*ehdr.e_shstrndx, SEEK_SET);
	fread(&sh_str, sizeof(Elf32_Shdr), 1, fp);
	LOGD("section string table: name index=%d, type=0x%08x, flag=0x%08x, addr=0x%08x, offset=0x%08x, size=%d, align=%d\n",
			sh_str.sh_name, sh_str.sh_type, sh_str.sh_flags, sh_str.sh_addr, sh_str.sh_offset, sh_str.sh_size, sh_str.sh_addralign);
	fseek(fp, sh_str.sh_offset, SEEK_SET);
	str = (char*)malloc(sh_str.sh_size);
	fread(str, sizeof(unsigned char), sh_str.sh_size, fp);

	LOGD("2. find dynstr\n");
	sh_table = (Elf32_Shdr*)malloc(sizeof(Elf32_Shdr) * ehdr.e_shnum);
	memset(sh_table, 0, sizeof(Elf32_Shdr) * ehdr.e_shnum);
	fseek(fp, ehdr.e_shoff, SEEK_SET);
	fread(sh_table, sizeof(Elf32_Shdr), ehdr.e_shnum, fp);
	for(i=0; i<ehdr.e_shnum; i++){
		Elf32_Shdr *pshdr = sh_table + i;
		if(!strcmp(".dynstr", str+pshdr->sh_name)){
			LOGD("section %d: name=%s, type=0x%08x, flag=0x%08x, addr=0x%08x, offset=0x%08x, size=%d, align=%d\n",
					i+1, str+pshdr->sh_name, pshdr->sh_type, pshdr->sh_flags, pshdr->sh_addr, pshdr->sh_offset, pshdr->sh_size, pshdr->sh_addralign);
			memcpy(&sh_dynstr, pshdr, sizeof(Elf32_Shdr));
			break;
		}
	}

	fseek(fp, sh_dynstr.sh_offset, SEEK_SET);
	dynstr = (char *)malloc(sh_dynstr.sh_size);
	memset(dynstr, 0, sh_dynstr.sh_size);
	fread(dynstr, sizeof(unsigned char), sh_dynstr.sh_size, fp);

	LOGD("3. parse symble section\n");
	for(i=0; i<ehdr.e_shnum; i++){
		Elf32_Shdr *pshdr = sh_table + i;
		if(!strcmp(".dynsym", str+pshdr->sh_name)){
			memcpy(&sh_sym, pshdr, sizeof(Elf32_Shdr));
			LOGD("section %d: name=%s, type=0x%08x, flag=0x%08x, addr=0x%08x, offset=0x%08x, size=%d, align=%d\n",
													i+1, str+pshdr->sh_name, pshdr->sh_type, pshdr->sh_flags, pshdr->sh_addr, pshdr->sh_offset, pshdr->sh_size, pshdr->sh_addralign);
			break;
		}
	}

	fseek(fp, sh_sym.sh_offset, SEEK_SET);
	int sym_num = sh_sym.sh_size / sizeof(Elf32_Sym);
	LOGD("\tsym num=%d\n", sym_num);
	fseek(fp, sh_sym.sh_offset, SEEK_SET);
	sym_table = (Elf32_Sym*)malloc(sh_sym.sh_size);
	memset(sym_table, 0, sh_sym.sh_size);
	fread(sym_table, sizeof(Elf32_Sym), sym_num, fp);
	for(i=0; i<sym_num; i++){
		Elf32_Sym *sym = sym_table + i;
		if(sym->st_name == STN_UNDEF){
			continue;
		}
		if(sym->st_shndx > 0 && ELF32_ST_TYPE(sym->st_info) != STT_NOTYPE){
			Elf32_Shdr *section = sh_table + sym->st_shndx;
			LOGD("\tsymble %d: name=%s, type=%d, shndx=%d, shname=%s, value(offset from section)=0x%08x\n",
							(i+1), dynstr + sym->st_name, ELF32_ST_TYPE(sym->st_info), sym->st_shndx, str + section->sh_name, sym->st_value);
		}else{
			LOGD("\tsymble %d: name=%s, type=%d, shndx=%d, value(offset from section)=0x%08x\n",
										(i+1), dynstr + sym->st_name, ELF32_ST_TYPE(sym->st_info), sym->st_shndx, sym->st_value);
		}



	}

	LOGD("4. find hash section\n");

	for(i=0; i<ehdr.e_shnum; i++){
		Elf32_Shdr *pshdr = sh_table + i;
		if(!strcmp(".hash", str+pshdr->sh_name)){
			memcpy(&sh_hash, pshdr, sizeof(Elf32_Shdr));
			LOGD("section %d: name=%s, type=0x%08x, flag=0x%08x, addr=0x%08x, offset=0x%08x, size=%d, align=%d\n",
													i+1, str+pshdr->sh_name, pshdr->sh_type, pshdr->sh_flags, pshdr->sh_addr, pshdr->sh_offset, pshdr->sh_size, pshdr->sh_addralign);
		}
	}

	fseek(fp, sh_hash.sh_offset, SEEK_SET);
	fread(&nbucket, sizeof(Elf32_Word), 1, fp);
	fread(&nchain, sizeof(Elf32_Word), 1, fp);
	LOGD("nbucket=%d, nchain=%d\n", nbucket, nchain); // symble.size == nchain

	buchets = (Elf32_Word*)malloc(sizeof(Elf32_Word) * nbucket);
	chains = (Elf32_Word*)malloc(sizeof(Elf32_Word) * nchain);
	memset(buchets, 0, sizeof(Elf32_Word) * nbucket);
	memset(chains, 0, sizeof(Elf32_Word) * nchain);
	fread(buchets, sizeof(Elf32_Word), nbucket, fp);
	fread(chains, sizeof(Elf32_Word), nchain, fp);

	LOGD("test symble\n");
	for(i=0; i<sym_num; i++){
		Elf32_Sym *sym = sym_table + i;
		char *symname = dynstr + sym->st_name;
		unsigned hash = elfhash(symname);
		Elf32_Word j;
		for(j=buchets[hash % nbucket]; j; j=chains[j]){
			if(!strcmp(symname, dynstr + sym_table[i].st_name)){
				LOGD("find %d symble from hash section, symname=%s, result=%s\n", (i+1), symname, dynstr + sym_table[i].st_name);
			}
		}
	}


	LOGD("5. print all section\n");
	for(i=0; i<ehdr.e_shnum; i++){
		Elf32_Shdr *pshdr = sh_table + i;
		LOGD("section %d: name=%s, type=0x%08x, flag=0x%08x, addr=0x%08x, offset=0x%08x, size=%d, align=%d\n",
															i+1, str+pshdr->sh_name, pshdr->sh_type, pshdr->sh_flags, pshdr->sh_addr, pshdr->sh_offset, pshdr->sh_size, pshdr->sh_addralign);
	}


	LOGD("-------------------- parse section end -------------------\n\n");

	LOGD("-------------------- parse segment -------------------\n");
	ph_table = (Elf32_Phdr *)malloc(sizeof(Elf32_Phdr) * ehdr.e_phnum);
	memset(ph_table, 0, sizeof(Elf32_Phdr) * ehdr.e_phnum);
	fseek(fp, ehdr.e_phoff, SEEK_SET);
	fread(ph_table, sizeof(Elf32_Phdr), ehdr.e_phnum, fp);

	LOGD("1. find dynamic segment\n");
	for(i=0; i<ehdr.e_phnum; i++){
		Elf32_Phdr *pphdr = ph_table + i;
		if(PT_DYNAMIC == pphdr->p_type){
			LOGD("segment %d: type=0x%x, offset=0x%08x, vaddr=0x%08x, filesz=%d, memsz=%d, align=%d\n",
					(i+1), pphdr->p_type, pphdr->p_offset, pphdr->p_vaddr, pphdr->p_filesz, pphdr->p_memsz, pphdr->p_align);
			memcpy(&dynamic, pphdr, sizeof(Elf32_Phdr));
			break;
		}
	}

	int dyn_num =dynamic.p_memsz / sizeof(Elf32_Dyn); // memsz >= filesz
	LOGD("\tdyn num=%d\n", dyn_num);

	dyn_table = (Elf32_Dyn*)malloc(sizeof(Elf32_Dyn) * dyn_num);
	memset(dyn_table, 0, dynamic.p_memsz);
	fseek(fp, dynamic.p_offset, SEEK_SET);
	fread(dyn_table, sizeof(Elf32_Dyn), dyn_num, fp);

	for(i=0; i<dyn_num; i++){
		Elf32_Dyn *pdyn = dyn_table + i;
		if(DT_NEEDED == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_NEEDED, val=%s\n", (i+1), dynstr + pdyn->d_un.d_val); // liblog.so   libc.so  ...
		}else if(DT_STRTAB == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_STRTAB, ptr=0x%08x\n", (i+1), pdyn->d_un.d_ptr); // .dynstr
		}else if(DT_HASH == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_HASH, ptr=0x%08x\n", (i+1), pdyn->d_un.d_ptr); // .hash
		}else if(DT_PLTRELSZ == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_PLTRELSZ, val=%d\n", (i+1), pdyn->d_un.d_val); // sizeof(.rel.plt)
		}else if(DT_PLTGOT == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_PLTGOT, ptr=0x%08x\n", (i+1), pdyn->d_un.d_ptr);
		}else if(DT_SYMTAB == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_SYMTAB, ptr=0x%08x\n", (i+1), pdyn->d_un.d_ptr); // .dynsym
		}else if(DT_SYMENT == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_SYMENT, val=%d\n", (i+1), pdyn->d_un.d_val); // 16 sizeof(Elf32_Sym)
		}else if(DT_STRSZ == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_STRSZ, val=%d\n", (i+1), pdyn->d_un.d_val); // 1682 sizeof(.dynstr)
		}else if(DT_SONAME == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_SONAME, val=%s\n", (i+1), dynstr + pdyn->d_un.d_val); // libhooktest.so
		}else if(DT_REL == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_REL, ptr=0x%08x\n", (i+1), pdyn->d_un.d_ptr); // .rel.dyn
		}else if(DT_RELSZ == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_RELSZ, val=%d\n", (i+1), pdyn->d_un.d_val); // 200 sizeof(.rel.dyn)
		}else if(DT_RELENT == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_RELENT, val=%d\n", (i+1), pdyn->d_un.d_val); // 8  sizeof(DT_RELENT)
		}else if(DT_PLTREL == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_PLTREL, val=%d\n", (i+1), pdyn->d_un.d_val); // R_MICROBLAZE_JUMP_SLOT
		}else if(DT_JMPREL == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_JMPREL, ptr=0x%08x\n", (i+1), pdyn->d_un.d_ptr); // .rel.plt
		}else if(DT_INIT_ARRAY == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_INIT_ARRAY, ptr=0x%08x\n", (i+1), pdyn->d_un.d_ptr); // .init_array
		}else if(DT_FINI_ARRAY == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_FINI_ARRAY, ptr=0x%08x\n", (i+1), pdyn->d_un.d_ptr); // .fini_array
		}else if(DT_INIT_ARRAYSZ == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_INIT_ARRAYSZ, val=%d\n", (i+1), pdyn->d_un.d_val); // sizeof(.init_array)
		}else if(DT_FINI_ARRAYSZ == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_FINI_ARRAYSZ, val=%d\n", (i+1), pdyn->d_un.d_val); // sizeof(.fini_array)
		}else if(DT_RELCOUNT == pdyn->d_tag){
			LOGD("\tdyn %d: tag=DT_RELCOUNT, val=%d\n", (i+1), pdyn->d_un.d_val);
		}else if(DT_NULL == pdyn->d_tag){
			continue;
		}else{
			LOGD("\tdyn %d: tag=0x%08x\n", (i+1), pdyn->d_tag);
		}
	}

	LOGD("2. find loadable segment\n");
	for(i=0; i<ehdr.e_phnum; i++){
		Elf32_Phdr *pphdr = ph_table + i;
		if(PT_LOAD == pphdr->p_type){
			LOGD("segment %d: type=0x%x, offset=0x%08x, vaddr=0x%08x, filesz=%d, memsz=%d, align=%d\n",
										(i+1), pphdr->p_type, pphdr->p_offset, pphdr->p_vaddr, pphdr->p_filesz, pphdr->p_memsz, pphdr->p_align);
		}
	}


	free(ph_table);
	free(dyn_table);

	free(buchets);
	free(chains);
	free(sym_table);
	free(dynstr);
	free(str);
	free(sh_table);
	fclose(fp);
}
