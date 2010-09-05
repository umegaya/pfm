#include "nbr.h"
#include "testutil.h"
#include "ll.h"
#include <string.h>
#include <memory.h>

char *rand_string(char *p, size_t l)
{
	const char text[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	for (size_t i = 0; i < (l - 1); i++) {
		p[i] = text[nbr_rand32() % (sizeof(text) - 1)];
	}
	p[l - 1] = '\0';
	return p;
}

char *rand_buffer(char *p, size_t l)
{
	for (size_t i = 0; i < l; i++) {
		p[i] = (char )(nbr_rand32() % 256);
	}
	return p;
}

const char *get_rcpath(char *buf, size_t blen, 
		const char *exepath, const char *path)
{
	char *p = strrchr(exepath, '/');
	memcpy(buf, exepath, p - exepath);
	buf[p - exepath] = '\0';
	strcat(buf, "/");
	strcat(buf, path);
	return buf;
}

//using namespace pfm;

#if 0
int pack_rpc_reqheader(pfm::serializer &sr,
		class pfm::UUID &uuid, const char *klass,
		const char *method, pfm::world_id wid, int n_arg)
{
	int r;
	pfm::MSGID msgid = 11;
	TEST((r = pfm::rpc::ll_exec_request::pack_header(sr, msgid, 
		uuid, klass, method, strlen(method), wid, strlen(wid), false, n_arg)) < 0,
		"push_array_len fail (%d)\n", r);
	return sr.len();
}
#endif



