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

using namespace pfm;


int pack_rpc_reqheader(serializer &sr, class object &o, 
		const char *method, world_id wid, int n_arg)
{
	int r;
	MSGID msgid = 11;
	TEST((r = rpc::ll_exec_request::pack_header(sr, msgid, 
		o, method, strlen(method), wid, strlen(wid), n_arg)) < 0, 
		"push_array_len fail (%d)\n", r);
	return sr.len();
}




