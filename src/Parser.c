#include "Parser.h"

void parseElf(char *filename){
	printf("start parse elf %s\n", filename);

	FILE *fp = fopen(filename, "rb");
	if(!fp){
		printf("fopen %s fail\n", filename);
		return;
	}

	Elf32_Ehdr ehdr;
	Elf32_Shdr *sh_table;
	Elf32_Shdr sh_str;
	Elf32_Shdr sh_dynstr;
	Elf32_Shdr sh_sym;
	Elf32_Sym *sym_table;
	char *str;
	char *dynstr;
	int i;

	fread(&ehdr, sizeof(Elf32_Ehdr), 1, fp);

	printf("type=%d\n", ehdr.e_type); 		// #define ET_DYN		3
	printf("machine=%d\n", ehdr.e_machine); // #define EM_ARM		40
	printf("entry=0x%08x\n", ehdr.e_entry); // 0x0
	printf("phoff=0x%08x\n", ehdr.e_phoff); // 0x34
	printf("phnum=%d\n", ehdr.e_phnum);		// 8
	printf("phentsize=%d\n", ehdr.e_phentsize); // 32
	printf("shoff=0x%08x\n", ehdr.e_shoff); // 0x51dc
	printf("shnum=%d\n", ehdr.e_shnum);		// 27
	printf("shentsize=%d\n", ehdr.e_shentsize);	// 40
	printf("ehsize=%d\n", ehdr.e_ehsize);	// 52 == e_phoff
	printf("shstrndx=%d\n", ehdr.e_shstrndx); // 26 == (e_shnum-1)

	printf("-------------------- parse section -------------------\n");

	printf("1. find strtab\n");
	fseek(fp, ehdr.e_shoff+ehdr.e_shentsize*ehdr.e_shstrndx, SEEK_SET);
	fread(&sh_str, sizeof(Elf32_Shdr), 1, fp);
	printf("section string table: name index=%d, type=0x%08x, flag=0x%08x, addr=0x%08x, offset=0x%08x, size=%d, align=%d\n",
			sh_str.sh_name, sh_str.sh_type, sh_str.sh_flags, sh_str.sh_addr, sh_str.sh_offset, sh_str.sh_size, sh_str.sh_addralign);
	fseek(fp, sh_str.sh_offset, SEEK_SET);
	str = (char*)malloc(sh_str.sh_size);
	fread(str, sizeof(unsigned char), sh_str.sh_size, fp);

	printf("2. find dynstr\n");
	sh_table = (Elf32_Shdr*)malloc(sizeof(Elf32_Shdr) * ehdr.e_shnum);
	memset(sh_table, 0, sizeof(Elf32_Shdr) * ehdr.e_shnum);
	fseek(fp, ehdr.e_shoff, SEEK_SET);
	fread(sh_table, sizeof(Elf32_Shdr), ehdr.e_shnum, fp);
	for(i=0; i<ehdr.e_shnum; i++){
		Elf32_Shdr *pshdr = sh_table + i;
		if(!strcmp(".dynstr", str+pshdr->sh_name)){
			printf("section %d: name=%s, type=0x%08x, flag=0x%08x, addr=0x%08x, offset=0x%08x, size=%d, align=%d\n",
					i+1, str+pshdr->sh_name, pshdr->sh_type, pshdr->sh_flags, pshdr->sh_addr, pshdr->sh_offset, pshdr->sh_size, pshdr->sh_addralign);
			memcpy(&sh_dynstr, pshdr, sizeof(Elf32_Shdr));
			break;
		}
	}

	fseek(fp, sh_dynstr.sh_offset, SEEK_SET);
	dynstr = (char *)malloc(sh_dynstr.sh_size);
	memset(dynstr, 0, sh_dynstr.sh_size);
	fread(dynstr, sizeof(unsigned char), sh_dynstr.sh_size, fp);

	printf("3. parse symble section\n");
	for(i=0; i<ehdr.e_shnum; i++){
		Elf32_Shdr *pshdr = sh_table + i;
		if(!strcmp(".dynsym", str+pshdr->sh_name)){
			memcpy(&sh_sym, pshdr, sizeof(Elf32_Shdr));
			printf("section %d: name=%s, type=0x%08x, flag=0x%08x, addr=0x%08x, offset=0x%08x, size=%d, align=%d\n",
													i+1, str+pshdr->sh_name, pshdr->sh_type, pshdr->sh_flags, pshdr->sh_addr, pshdr->sh_offset, pshdr->sh_size, pshdr->sh_addralign);
			break;
		}
	}

	fseek(fp, sh_sym.sh_offset, SEEK_SET);
	int sym_num = sh_sym.sh_size / sizeof(Elf32_Sym);
	printf("\tsym num=%d\n", sym_num);
	fseek(fp, sh_sym.sh_offset, SEEK_SET);
	sym_table = (Elf32_Sym*)malloc(sh_sym.sh_size);
	memset(sym_table, 0, sh_sym.sh_size);
	fread(sym_table, sizeof(Elf32_Sym), sym_num, fp);
	for(i=0; i<sym_num; i++){
		Elf32_Sym *sym = sym_table + i;
		printf("\tsymble %d: name=%s, type=%d, shndx=%d, value(offset from section)=0x%08x\n", (i+1), dynstr + sym->st_name, ELF32_ST_TYPE(sym->st_info), sym->st_shndx, sym->st_value);
	}

	printf("4. print all section\n");
	for(i=0; i<ehdr.e_shnum; i++){
		Elf32_Shdr *pshdr = sh_table + i;
		printf("section %d: name=%s, type=0x%08x, flag=0x%08x, addr=0x%08x, offset=0x%08x, size=%d, align=%d\n",
															i+1, str+pshdr->sh_name, pshdr->sh_type, pshdr->sh_flags, pshdr->sh_addr, pshdr->sh_offset, pshdr->sh_size, pshdr->sh_addralign);
	}


	free(sym_table);
	free(dynstr);
	free(str);
	free(sh_table);
	fclose(fp);
}
