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

#define ELF_FILE_PATH "file/libdata.so"

int main(void) {
	parseElf(ELF_FILE_PATH);


	return EXIT_SUCCESS;
}
