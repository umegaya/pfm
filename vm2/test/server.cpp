#include "common.h"
#include "testutil.h"
#include "proto.h"
#include <sys/wait.h>

using namespace pfm;

#define SVRPFX "pfm"
struct process_info {
	pid_t pid;
	int status;
};

int write_config(int node_type,
	char *fname, int len, const char *path, int n) {
	snprintf(fname, len, "%s/%u.conf", path, n);
	FILE *fp = fopen(fname, "w");
	if (!fp) { return NBR_ESYSCALL; }
	switch(node_type) {
	case servant_node:
		fprintf(fp, "svnt.host=0.0.0.0:%hu", 8200 + n);
		break;
	case client_node:
		fprintf(fp, "clnt.host=localhost:%hu", 8200 + n);
		break;
	}
	fclose(fp);
	return NBR_OK;
}

int stop_process(process_info *si)
{
	kill(si->pid, SIGTERM);
	waitpid(si->pid, &(si->status), 0);
	if (WTERMSIG(si->status) != SIGTERM) {
		return NBR_EINVAL;
	}
	return NBR_OK;
}

int server_test(int argc, char *argv[])
{
	int r, n_svnt = 1;
	char fname[256];
	char *mstr_prog = "./mstr/"SVRPFX"m";
	char *mstr_argv[] = { "", "./mstr/conf/m.conf", NULL };
	char *svnt_prog = "./svnt/"SVRPFX"s";
	char *svnt_argv[] = { "", "", NULL };
	char *clnt_prog = "./clnt/"SVRPFX"c";
	char *clnt_argv[] = { "", "", "", NULL };
	/* run mstr */
	process_info minfo;
	if ((r = app::daemon::fork(mstr_prog, mstr_argv, NULL)) < 0) {
		ASSERT(false);
		return r;
	}
	minfo.pid = (pid_t)r;
	app::daemon::log(kernel::INFO, "master: pid = %u\n", r);
	nbr_osdep_sleep(1 * 1000 * 1000 * 1000);
	/* run n_svnt svnt */
	process_info sinfo[n_svnt];
	for (int i = 0; i < n_svnt; i++) {
		if ((r = write_config(servant_node,
			fname, sizeof(fname), "./svnt/conf", i)) < 0) {
			return r;
		}
		svnt_argv[0] = svnt_prog;
		svnt_argv[1] = fname;
		if ((r = app::daemon::fork(svnt_prog, svnt_argv, NULL)) < 0) {
			return r;
		}
		sinfo[i].pid = (pid_t)r;
		app::daemon::log(kernel::INFO, "servant[%u]: pid = %u\n", i, r);
		nbr_osdep_sleep(1 * 1000 * 1000 * 1000);
	}
	/* run n_svnt clnt */
	process_info cinfo[n_svnt];
	for (int i = 0; i < n_svnt; i++) {
		if ((r = write_config(client_node,
			fname, sizeof(fname), "./clnt/conf", i)) < 0) {
			return r;
		}
		svnt_argv[0] = clnt_prog;
		svnt_argv[1] = fname;
		svnt_argv[2] = "1";
		if ((r = app::daemon::fork(clnt_prog, clnt_argv, NULL)) < 0) {
			return r;
		}
		cinfo[i].pid = (pid_t)r;
		app::daemon::log(kernel::INFO, "client[%u]: pid = %u\n", i, r);
		nbr_osdep_sleep(1 * 1000 * 1000 * 1000);
	}
	nbr_osdep_sleep(5LL * 1000 * 1000 * 1000);
	/* kill all process */
	r = NBR_OK;
	for (int i = 0; i < n_svnt; i++) {
		if ((r = stop_process(&(cinfo[i]))) < 0) {}
	}
	for (int i = 0; i < n_svnt; i++) {
		if ((r = stop_process(&(sinfo[i]))) < 0) {}
	}
	if ((r = stop_process(&minfo)) < 0) {}
	return r;
}
