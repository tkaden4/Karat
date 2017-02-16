#include<locale.h>
#include<wchar.h>
#include<SDL2/SDL.h>

#include<log.h>
#include<alloc.h>
#include<vm/kprog.h>
#include<parse/parse.h>

#define usage() \
	printf("Usage: karat < file.kt >"); exit(0);

int main(int argc, const char *argv[])
{
	setlocale(LC_ALL, "");

	const char *prog = NULL;
	switch(argc){
	case 2:
		prog = argv[1];
		break;
	default:
		usage();
	};

	FILE *test = fopen(prog, "r");
	err_on(!test, "could not open %s", prog);

	struct kprog *rprog = kprog_create();
	if(!parse_file(test, rprog)){
		debug("Result program: %lu bytes used", rprog->__size);
#define ITER_TYPE u16
		for(size_t i = 0; i <= rprog->prog_size - sizeof(ITER_TYPE); i += sizeof(ITER_TYPE)){
			printf("0x%lX\n", (u64)*(ITER_TYPE *)&rprog->program[i]);
		}
		kprog_destroy(rprog);
	}else{
		puts("unable to parse file");
	}

	fclose(test);
	return 0;
}
