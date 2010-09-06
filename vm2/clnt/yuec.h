#if !defined(__PFMC_H__)
#define __PFMC_H__

namespace pfm {
namespace clnt {
extern int init(const char *cfg);
extern void poll(unsigned long long ut);
extern void fin();

typedef void *client;
typedef void *watcher;
typedef void *errobj;
typedef void *args;
typedef void *obj;
typedef int (*watchercb)(watcher,client,errobj,char*,int);
extern client login(const char *host, const char *wid,
			const char *acc, const char *authd, int dlen,
			watchercb cb);
extern watcher call(client c, args a, watchercb cb);
extern const char *error_to_s(char *b, int l, errobj e);
extern obj get_from(client c);
extern args init_args(int len);
extern void fin_args(args a);
extern int setup_args(args a, obj o, const char *method,
			bool local_call, int n_args);
extern int args_int(args a, int i);
extern int args_bool(args a, bool b);
extern int args_nil(args a);
extern int args_bigint(args a, long long ll);
extern int args_double(args a, double d);
extern int args_string(args a, const char *str);
extern int args_array(args a, int size);
extern int args_map(args a, int size);
extern int args_blob(args a, const char *p, int l);
}
}

#endif
