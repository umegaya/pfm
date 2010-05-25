#include <stdio.h>
#include <string.h>
#include "nbr.h"

#define TEST(type) extern int type##_test(int argc ,char *argv[]);	\
fprintf(stderr, #type "_test: start\n");		\
if ((argc < 2 || strcmp("any", argv[1]) == 0 || 	\
	strcmp(#type, argv[1]) == 0) && 		\
	(r = type##_test(argc, argv)) < 0) { 		\
 fprintf(stderr, #type "_test: fail (%d)\n", r); 	\
 return r;						\
}							\
fprintf(stderr, #type "_test: pass\n");

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
	nbr_fin();
	return r;
}
