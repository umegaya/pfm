#include "clnt.h"
#include "nbr.h"
#include "common.h"
#include "serializer.h"
#include "cp.h"

const char *
find_config_path(int argc, char *argv[]) {
	for (int i = 0; i < argc; i++) {
		if (nbr_str_cmp_tail(argv[i], ".conf", 5, 256) == 0) {
			return argv[i];
		}
	}
	return NULL;
}

static int g_client = 1;
static int g_require_callnum_per_cli = 1;
static int g_callnum = 0;
NBR_STLS pfm::clnt::args g_args = NULL;
static bool g_finish() { return (g_callnum >= ((g_require_callnum_per_cli * g_client) + g_client)); }
int callback(pfm::clnt::watcher w, pfm::clnt::client c,
		pfm::clnt::errobj e, char *p, int l) {
	if (!g_args) { g_args = pfm::clnt::init_args(65536); }
	if (e) {
		char b[256];
		TRACE("RPC error: <%s>\n", pfm::clnt::error_to_s(b, sizeof(b), e));
		return NBR_OK;
	}
	pfm::clnt::obj o = pfm::clnt::get_from(c);
	pfm::clnt::setup_args(g_args, o, "test", true, 0);
	pfm::clnt::watcher w2 = pfm::clnt::call(c, g_args, callback);
	if (w2) { 
		__sync_add_and_fetch(&g_callnum, 1);
		if (g_finish()) { return NBR_OK; }
		return NBR_OK;
	}
	return NBR_EEXPIRE;
}

int main(int argc, char *argv[])
{
	int r;
	if ((r = pfm::clnt::init(find_config_path(argc, argv)) < 0)) {
		return r;
	}
	pfm::clnt::watcher wlist[1024];
	char acc[256];
	if (argc > 2) {
		SAFETY_ATOI(argv[2], g_client, int);
		if (g_client > 1024) { return NBR_ESHORT; }
	}
	for (int i = 0; i < g_client; i++) {
		U32 hash = (g_client << 16) + i;
		snprintf(acc, sizeof(acc) - 1, "rtko%08x", hash);
		if (!(wlist[i] = pfm::clnt::login(NULL,
			"test", acc, (char *)&hash, sizeof(hash), 
			callback))) {
			return NBR_EEXPIRE;
		}
		usleep(50 * 1000);
	}
	UTIME g_start = nbr_time();
	while (1) {
		pfm::clnt::poll(nbr_time());
		if (g_finish()) { break; }
	}
	UTIME g_tot = (nbr_time() - g_start);
	fprintf(stdout, "%lluus for %uquery (%lfqps)\n", 
		g_tot, g_client * g_require_callnum_per_cli, 
		(float)(((float)1000 * 1000) * (float)g_callnum / (float)g_tot));
	pfm::clnt::fin();
	return NBR_OK;
}
