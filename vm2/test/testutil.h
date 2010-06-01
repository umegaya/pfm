#if !defined(__TESTUTIL_H__)
#define __TESTUTIL_H__

#include "serializer.h"
#include "proto.h"
#include "object.h"

extern char *rand_string(char *p, size_t l);
extern char *rand_buffer(char *p, size_t l);
extern const char *get_rcpath(char *b, size_t blen, const char *exepath, const char *path);

extern int pack_rpc_resheader(pfm::serializer &sr, pfm::object &o);
extern int pack_rpc_reqheader(pfm::serializer &sr, pfm::object &o, 
	const char *method, pfm::world_id wid, int n_arg);

#define TTRACE(fmt,...)	TRACE("%08x:%s:%u>" fmt, 	\
		nbr_thread_get_curid(), __FILE__,__LINE__,__VA_ARGS__);
#define PUSHSTR(sr,name)	sr.push_string(#name, sizeof(#name) - 1);
#define MAKEPATH(_b,_path) get_rcpath(_b, sizeof(_b), argv[0], _path)

#define TEST(cond, ...)	\
	if (cond) {	\
		TTRACE(__VA_ARGS__);	\
		return r;	\
	}

#endif
