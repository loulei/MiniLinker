/*
 ============================================================================
 Name        : MiniLinker.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include "Parser.h"
#include "Loader.h"

#define ELF_FILE_PATH "file/libdata.so"

int main(void) {
	parseElf(ELF_FILE_PATH);

	LOGD("\nstart load lib %s\n", ELF_FILE_PATH);
	soinfo *si = do_dlopen(ELF_FILE_PATH, RTLD_NOW);
	LOGD("%s map to : %p", ELF_FILE_PATH, si);

	return EXIT_SUCCESS;
}
