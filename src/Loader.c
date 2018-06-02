#include "Loader.h"

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


soinfo *do_dlopen(const char *name, int flags){
	if((flags & ~(RTLD_NOW | RTLD_LAZY | RTLD_LOCAL | RTLD_GLOBAL)) != 0){
		LOGE("invalid flags to dlopen: %x", flags);
		return NULL;
	}

}

static void set_soinfo_pool_protection(int protection){
	for(soinfo_pool_t *p = gSoInfoPools; p != NULL; p = p->next){
		if(mprotect(p, sizeof(*p), protection) == -1){
			abort();
		}
	}
}
