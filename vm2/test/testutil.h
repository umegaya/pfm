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


template <class T>
class thread : public T {
public:
	typedef int (*mainfn)(T &, ...);
	THREAD m_thrd;
	THPOOL m_thp;
	int (*m_fn)(T &, ...);
	int m_argc, m_result;
	char **m_argv;
	void *m_extra;
public:
	thread() : T() {}
	int run(THPOOL thp, int argc, char **argv,
		int (*fn)(T &, ...),
		bool wait, void *extra) {
		m_argc = argc;
		m_argv = argv;
		m_fn = fn;
		m_thp = thp;
		m_extra = extra;
		if (!(m_thrd = nbr_thread_create(thp, this, main))) {
			return NBR_EPTHREAD;
		}
		return wait ? join() : NBR_OK;
	}
	template <class A1> static mainfn 
		get_cb(int (*fn)(T&, A1)) { return (mainfn)fn; }
	template <class A1, class A2> static mainfn 
		get_cb(int (*fn)(T&, A1, A2)) { return (mainfn)fn; }
	template <class A1, class A2, class A3> static mainfn
		get_cb(int (*fn)(T&, A1, A2, A3)) { return (mainfn)fn; }
	int result() const { return m_result; }
	int join() { 
		int r = nbr_thread_join(m_thrd, 0, NULL); 
		if (r < 0) { return r; }
		return nbr_thread_destroy(m_thp, m_thrd);
	}
	static void *main(THREAD th) {
		class thread *p = (class thread *)nbr_thread_get_data(th);
		p->m_result = p->m_fn(*p, p->m_argc, p->m_argv, p->m_extra);
		return NULL;
	}
};

#define EXEC_THREAD(__thp, __fn, __data, __wait, ...) \
		__data.run(__thp, argc, argv, __data.get_cb(__fn), __wait, __VA_ARGS__)

#endif
