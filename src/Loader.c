#include "Loader.h"
#include "mini_linker_phdr.h"

static char tmp_err_buf[768];
static char __linker_dl_err_buf[768];

char* linker_get_error_buffer() {
  return &__linker_dl_err_buf[0];
}

size_t linker_get_error_buffer_size() {
  return sizeof(__linker_dl_err_buf);
}

typedef struct soinfo_pool_t soinfo_pool_t;
#define SOINFO_PER_POOL ((PAGE_SIZE - sizeof(soinfo_pool_t*)) / sizeof(soinfo))

struct soinfo_pool_t {
	soinfo_pool_t *next;
	soinfo info[SOINFO_PER_POOL];
};

static struct soinfo_pool_t *gSoInfoPools = NULL;
static soinfo *gSoInfoFreeList = NULL;

static soinfo *solist = NULL;
static soinfo *sonext = NULL;
static soinfo *somain;

static const char *const gSoPaths[] = {
		"/vendor/lib",
		"/system/lib",
		NULL
};

void CallFunction(const char* function_name, linker_function_t function){
	if(function == NULL || function == -1){
		return;
	}
	function();
	set_soinfo_pool_protection(PROT_READ | PROT_WRITE);
}

void CallArray(const char* array_name, linker_function_t* functions, size_t count, bool reverse){
	if(functions == NULL)
		return;

	int begin = reverse ? (count - 1) : 0;
	int end = reverse ? -1 : count;
	int step = reverse ? -1 : 1;

	for(int i=0; i != end; i+=step){
		CallFunction("function", functions[i]);
	}
}

void CallConstructors(soinfo *si){
	if(constructors_called){
		return;
	}
	constructors_called = true;
	CallFunction("DT_INIT", si->init_func);
	CallArray("DT_INIT_ARRAY", si->init_array, si->init_array_count, false);
}


soinfo *do_dlopen(const char *name, int flags){
	if((flags & ~(RTLD_NOW | RTLD_LAZY | RTLD_LOCAL | RTLD_GLOBAL)) != 0){
		LOGE("invalid flags to dlopen: %x", flags);
		return NULL;
	}
	set_soinfo_pool_protection(PROT_READ | PROT_WRITE);
	soinfo *si = find_library(name);
	if(si != NULL){
		CallConstructors(si);
	}
	set_soinfo_pool_protection(PROT_READ);
	return si;
}

static void set_soinfo_pool_protection(int protection){
	for(soinfo_pool_t *p = gSoInfoPools; p != NULL; p = p->next){
		if(mprotect(p, sizeof(*p), protection) == -1){
			abort();
		}
	}
}

static soinfo* find_library(const char *name){
	soinfo *si = find_library_internal(name);
	if(si != NULL){
		si->ref_count++;
	}
	return si;
}

static soinfo* find_library_internal(const char *name){
	if(name == NULL){
		return somain;
	}

	soinfo *si = find_loaded_library(name);
	if(si != NULL){
		if(si->flags & FLAG_LINKED){
			return si;
		}
		LOGD("recusive link to %s", si->name);
		return NULL;
	}

	si = load_library(name);
	if(si == NULL){
		return NULL;
	}

	LOGD("init library base=0x%08x sz=0x%08x name=%s", si->base, si->size, si->name);

}

static bool soinfo_link_image(soinfo *si){
	Elf32_Addr base = si->load_bias;
	const Elf32_Phdr *phdr = si->phdr;
	int phnum = si->phnum;
	bool relocating_linker = (si->flags & FLAG_LINKED) != 0;
	if(!relocating_linker){
		//...
	}

	size_t dynamic_count;
	Elf32_Word dynamic_flags;

	phdr_table_get_dynamic_section(phdr, phnum, base, &si->dynamic, &dynamic_count, &dynamic_flags);
	if(si->dynamic == NULL){
		LOGD("missing PT_DYNAMIC in %s", si->name);
		return false;
	}else{

	}

	phdr_table_get_arm_exidx(phdr, phnum, base, &si->ARM_exidx, &si->ARM_exidx_count);

	uint32_t needed_count = 0;
	for(Elf32_Dyn *d = si->dynamic; d->d_tag != DT_NULL; ++d){
		LOGD("d=%p, d[0](tag)=0x%08x d[1](val)=0x%08x", d, d->d_tag, d->d_un.d_val);
		switch(d->d_tag){
		case DT_HASH:
			si->nbucket = ((unsigned*)(base+d->d_un.d_ptr))[0];
			si->nchain = ((unsigned*)(base+d->d_un.d_ptr))[1];
			si->bucket = (unsigned*)(base + d->d_un.d_ptr + 8);
			si->chain = (unsigned*)(base + d->d_un.d_ptr + 8 + si->nbucket * 4);
			break;
		case DT_STRTAB:
			si->strtab = (const char*)(base + d->d_un.d_ptr);
			break;
		case DT_SYMTAB:
			si->symtab = (Elf32_Sym *)(base + d->d_un.d_ptr);
			break;
		case DT_PLTREL:
			if(d->d_un.d_val != DT_REL){
				LOGD("unsupported DT_RELA in %s", si->name);
				return false;
			}
			break;
		case DT_JMPREL:
			si->plt_rel = (Elf32_Rel*)(base + d->d_un.d_ptr);
			break;
		case DT_PLTRELSZ:
			si->plt_rel_count = d->d_un.d_val / sizeof(Elf32_Rel);
			break;
		case DT_REL:
			si->rel = (Elf32_Rel*)(base + d->d_un.d_ptr);
			break;
		case DT_PLTGOT:
			si->plt_got = (unsigned*)(base + d->d_un.d_ptr);
			break;
		case DT_DEBUG:
			break;
		case DT_RELA:
			LOGD("unsupported DT_RELA in %s", si->name);
			return false;
		case DT_INIT:
			si->init_func = base+d->d_un.d_ptr;
			LOGD("%s constructors (DT_INIT) found at %p", si->name, si->init_func);
			break;
		case DT_FINI:
			si->fini_func = base + d->d_un.d_ptr;
			LOGD("%s destructors (DT_FINI) found at %p", si->name, si->fini_func);
			break;
		case DT_INIT_ARRAY:
			si->init_array = base + d->d_un.d_ptr;
			LOGD("%s constructors (DT_INIT_ARRAY) found at %p", si->name, si->init_array);
			break;
		case DT_INIT_ARRAYSZ:
			si->init_array_count = ((unsigned)d->d_un.d_val) / sizeof(Elf32_Addr);
			break;
		case DT_FINI_ARRAY:
			si->fini_array = base + d->d_un.d_ptr;
			LOGD("%s deconstructors (DT_FINI_ARRAY) found at %p", si->name, si->fini_array);
			break;
		case DT_FINI_ARRAYSZ:
			si->fini_array_count = ((unsigned)d->d_un.d_val) / sizeof(Elf32_Addr);
			break;
		case DT_PREINIT_ARRAY:
			si->preinit_array = base + d->d_un.d_ptr;
			LOGD("%s constructors (DT_PREINIT_ARRAY) found at %p", si->name, si->preinit_array);
			break;
		case DT_PREINIT_ARRAYSZ:
			si->preinit_array_count = ((unsigned)d->d_un.d_val) / sizeof(Elf32_Addr);
			break;
		case DT_TEXTREL:
			si->has_text_relocations = true;
			break;
		case DT_SYMBOLIC:
			si->has_DT_SYMBOLIC = true;
			break;
		case DT_NEEDED:
			++needed_count;
			break;
		}
	}

	LOGD("si->base=0x%08x, si->strtab=%p, si->symtab=%p", si->base, si->strtab, si->symtab);

	soinfo** needed = (soinfo **)alloca((1+needed_count) * sizeof(soinfo*));
	soinfo **pneeded = needed;

	for(Elf32_Dyn *d = si->dynamic; d->d_tag != DT_NULL; ++d){
		if(d->d_tag == DT_NEEDED){
			const char *library_name = si->strtab + d->d_un.d_val;
			LOGD("%s needs %s", si->name, library_name);
			soinfo *lsi = (soinfo*)dlopen(library_name, 0);
			if(lsi == NULL){
//				strlcpy(tmp_err_buf, linker_get_error_buffer(), sizeof(tmp_err_buf));
				LOGD("could not load library %s needed by %s", library_name, si->name);
				return false;
			}
			*pneeded++ = lsi;
		}
	}
	*pneeded = NULL;

	if(si->has_text_relocations){
		if(phdr_table_protect_segments(si->phdr, si->phnum, si->load_bias) < 0){
			LOGD("can not unprotect loadable segments for %s: %s", si->name, strerror(errno));
			return false;
		}
	}
}


static soinfo *find_loaded_library(const char *name){
	soinfo *si;
	const char *bname;

	bname = strrchr(name, '/');
	bname = bname ? bname + 1 : name;

	for(si = solist; si != NULL; si = si->next){
		if(!strcmp(bname, si->name)){
			return si;
		}
	}
	return NULL;
}

static soinfo* load_library(const char *name){
	int fd = open_library(name);
	if(fd == -1){
		LOGD("library %s not found", name);
		return NULL;
	}
	init(name, fd);
	if(!Load()){
		return NULL;
	}
	const char *bname = strrchr(name, "/");
	soinfo *si = soinfo_alloc(bname ? bname +1 :name);
	if(si == NULL){
		return NULL;
	}
	si->base = load_start_;
	si->size = load_size_;
	si->load_bias = load_bias_;
	si->flags = 0;
	si->dynamic = NULL;
	si->phnum = phdr_num_;
	si->phdr = loaded_phdr_;
	return si;
}


static int open_library(const char *name){
	LOGD("opening %s", name);
	int fd = open(name, O_RDONLY | O_CLOEXEC);
	if(fd != -1){
		return fd;
	}
	return -1;
}

static soinfo *soinfo_alloc(const char *name){
	if(strlen(name) > SOINFO_NAME_LEN){
		LOGD("library name %s too long", name);
		return NULL;
	}

	if(!ensure_free_list_non_empty()){
		LOGD("out of memory when loading %s", name);
		return NULL;
	}

	soinfo *si = (soinfo*)malloc(sizeof(soinfo));
	memset(si, 0, sizeof(soinfo));
	strlcpy(si->name, name, sizeof(si->name));
	LOGD("name %s:allocated soinfo @ %p", name, si);
	return si;
}

static bool ensure_free_list_non_empty(){
	if(gSoInfoFreeList != NULL){
		return NULL;
	}
	soinfo_pool_t *pool = mmap(NULL, sizeof(*pool), PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
	if(pool == MAP_FAILED){
		return false;
	}

	pool->next = gSoInfoPools;
	gSoInfoPools = pool;

	gSoInfoFreeList = &pool->info[0];
	soinfo *next = NULL;
	for(int i=SOINFO_PER_POOL-1; i >= 0; --i){
		pool->info[i].next = next;
		next = &pool->info[i];
	}
	return true;
}

static int soinfo_relocate(soinfo *si, Elf32_Rel *rel, unsigned count, soinfo *needed[]){
	Elf32_Sym *symtab = si->symtab;
	const char *strtab = si->strtab;
	Elf32_Sym *s;
	Elf32_Rel *start = rel;
	soinfo *lsi;

	for(size_t idx = 0; idx < count; ++idx, ++rel){
		unsigned type = ELF32_R_TYPE(rel->r_info);
		unsigned sym = ELF32_R_SYM(rel->r_info);
		Elf32_Addr reloc = rel->r_offset + si->load_bias;
		Elf32_Addr sym_addr = 0;
		char *sym_name = NULL;

		LOGD("processing %s relocation at index %d", si->name, idx);
		if(type == 0){
			continue;
		}
		if(sym != 0){
			sym_name = (char*)(strtab + symtab[sym].st_name);
		}
	}
}


static Elf32_Sym * soinfo_do_lookup(soinfo *si, const char *name, soinfo **lsi, soinfo *needed[]){
	unsigned elf_hash = elfhash(name);
	Elf32_Sym *s = NULL;
	if(si != NULL && somain != NULL){
		if(si == somain){
			s = soinfo_elf_lookup(si, elf_hash, name);
			if(s != NULL){
				*lsi = si;
				goto done;
			}
		}else{
			if(!si->has_DT_SYMBOLIC){

			}
		}
	}

done:
	if(s != NULL){
		LOGD("si %s sym %s s->st_value = 0x%08x, found in %s, base = 0x%08x, load bias = 0x%08x", si->name, name, s->st_value, (*lsi)->name, (*lsi)->base, (*lsi)->load_bias);
		return s;
	}
	return NULL;
}

static Elf32_Sym * soinfo_elf_lookup(soinfo *si, unsigned hash, const char *name){
	Elf32_Sym *symtab = si->symtab;
	const char *strtab = si->strtab;

	LOGD("search %s in %s@0x%08x %08x %d", name, si->name, si->base, hash, hash % si->nbucket);

	for(unsigned n = si->bucket[hash % si->nbucket]; n != 0; n=si->chain[n]){
		Elf32_Sym *s = symtab + n;
		if(strcmp(strtab + s->st_name, name)) continue;
		switch (ELF32_ST_BIND(s->st_info)) {
			case STB_GLOBAL:
			case STB_WEAK:
				if(s->st_shndx == SHN_UNDEF){
					continue;
				}
				LOGD("found %s in %s (%08x) %d", name, si->name, s->st_value, s->st_size);
				return s;
		}
	}
	return NULL;
}


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







