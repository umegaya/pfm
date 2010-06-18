#include <stdio.h>
#include <string.h>
#include "nbr.h"

#define TEST(type) extern int type##_test(int argc ,char *argv[]);	\
if (argc < 2 || strcmp("any", argv[1]) == 0 || 	\
	strcmp(#type, argv[1]) == 0) { 		\
	fprintf(stderr, #type "_test: start\n");		\
	if ((r = type##_test(argc, argv)) < 0) { 		\
		fprintf(stderr, #type "_test: fail (%d)\n", r); 	\
		return r;					\
	} 	\
	else {	\
		fprintf(stderr, #type "_test: pass\n"); \
	}	\
}							\

int main(int argc, char *argv[])
{
	int r;
	CONFIG c;
	nbr_get_default(&c);
	nbr_init(&c);
	TEST(dbm);
	TEST(serializer);
	TEST(uuid);
	TEST(ll);
	TEST(object);
	TEST(world);
	TEST(fiber);
	extern int server_test(int argc, char *argv[]);
	if (strcmp("server", argv[1]) == 0) {
		if ((r = server_test(argc, argv)) < 0) {
			fprintf(stderr, "server_test: fail (%d)\n", r);
			return r;
		}
	}
	nbr_fin();
	return r;
}
