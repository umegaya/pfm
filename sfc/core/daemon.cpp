/***************************************************************
 * daemon.cpp : application class
 * 2009/12/23 iyatomi : create
 *                             Copyright (C) 2008-2009 Takehiro Iyatomi
 * This file is part of libnbr.
 * libnbr is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either
 * version 2.1 of the License or any later version.
 * libnbr is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * You should have received a copy of
 * the GNU Lesser General Public License along with libnbr;
 * if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 ****************************************************************/
#include "sfc.hpp"
#include <stdarg.h>
#include <signal.h>

using namespace sfc;
using namespace sfc::app;

U32 daemon::m_sigflag = 0;

void
daemon::sigfunc(int signo)
{
	m_sigflag |= (1 << signo);
}

void
daemon::stop()
{
	daemon::log(INFO, "application terminate\n");
	daemon::sigfunc(SIGTERM);
}

int
daemon::alive()
{
	if (m_sigflag == 0) { return 1; }
	for (int i = 0; i < (int)(sizeof(m_sigflag) * 8); i++) {
		if ((m_sigflag & (1 << i)) != 0) {
			if (on_signal(i) < 0) {
				return 0;
			}
		}
	}
	m_sigflag = 0;
	return 1;
}

int
daemon::run()
{
	while(alive()) {
		nbr_poll();
		UTIME ut = nbr_clock();
		smap::iterator p = m_sl.begin();
		for (; p != m_sl.end(); p = m_sl.next(p)) {
			if ((ut - p->m_last_poll) > p->cfg().m_taskspan) {
				p->m_last_poll = ut;
				p->poll(ut);
			}
		}
	}
	fin();
	return NBR_OK;
}

int
daemon::init(int argc, char *argv[])
{
	int r, n_list;
	config *list[MAX_CONFIG_LIST];
	CONFIG nbrcfg;
	nbr_get_default(&nbrcfg);
	if ((r = initlib(nbrcfg)) < 0) {
		log(ERROR, "fail to library parameter (%d)\n", r);
		return r;
	}
	if ((r = nbr_init(&nbrcfg)) < 0) {
		log(ERROR, "fail to init library(%d)\n", r);
		return r;
	}
	if (!(n_list = create_config(list, MAX_CONFIG_LIST))) {
		log(ERROR, "fail to create config (%d)\n", n_list);
		return n_list;
	}
	if (!m_cl.init(n_list, n_list)) {
		log(ERROR, "config list too big? (%d)\n", m_cl.size());
		return NBR_EEXPIRE;
	}
	for (int i = 0; i < n_list; i++) {
		if (m_cl.insert(list[i], list[i]->m_name) == m_cl.end()) {
			return NBR_EEXPIRE;
		}
	}
	if ((r = read_config(argc, argv)) < 0) {
		log(ERROR, "read_config fail (%d)\n", r);
		return NBR_ECONFIGURE;
	}
	if (!m_sl.init(m_cl.size(), m_cl.size())) {
		log(ERROR, "session factory list too big? (%d)\n", m_cl.size());
		return NBR_EEXPIRE;
	}
	cmap::iterator c;
	for (c = m_cl.begin(); c != m_cl.end(); c = m_cl.next(c)) {
		factory *f;
		if (c->disabled()) {
			continue;	/* skip to create factory */
		}
		if (!(f = create_factory(c->m_name))) {
			log(ERROR, "%s: fail to create factory\n", c->m_name);
			return NBR_EINVAL;
		}
		if ((r = f->base_init()) < 0) {
			log(ERROR, "%s: base_init error %d\n", c->m_name, r);
			return r;
		}
		if ((r = f->init(*c)) < 0) {
			log(ERROR, "%s: init error %d\n", c->m_name, r);
			return r;
		}
		if (m_sl.insert(f, c->m_name) == m_sl.end()) {
			return NBR_EEXPIRE;
		}
	}
	nbr_sig_set_intr_handler(sigfunc);
	nbr_sig_set_fault_handler(sigfunc);

	return boot(argc, argv);
}

void
daemon::fin()
{
	/* app defined finalization */
	shutdown();
	/* stop network IO */
	nbr_stop_sock_io();
	/* cleanup related memory resource */
	smap::iterator s;
	for (s = m_sl.begin(); s != m_sl.end(); s = m_sl.next(s)) {
		s->fin();
		s->base_fin();
	}
	m_sl.fin();
	cmap::iterator c;
	for (c = m_cl.begin(); c != m_cl.end(); c = m_cl.next(c)) {
		c->fin();
	}
	m_cl.fin();
	/* clean up library */
	nbr_fin();
}

int
daemon::log(loglevel prio, const char *fmt, ...)
{
	char buff[4096];

	va_list v;
	va_start(v, fmt);
	vsnprintf(buff, sizeof(buff) - 1, fmt, v);
	va_end(v);

	fprintf(stdout, "%u:%u:%s", nbr_osdep_getpid(), prio, buff);
	return NBR_OK;
}


int
daemon::read_config(int argc, char *argv[])
{
	char *a_files[256];
	int n_files = 0, i;
	for (i = 0; i < argc; i++) {
		if (nbr_str_cmp_tail(argv[i], ".conf", 5, 256) == 0) {
			a_files[n_files++] = argv[i];
		}
	}
	for (i = 0; i < n_files; i++) {
		FILE *fp = fopen(a_files[i], "r");
		if (fp == NULL) {
			log(INFO, "%s: cannot open\n", a_files[i]);
			continue;	/* –³‚©‚Á‚½‚çdefaultÝ’è‚Ås‚­‚Ì‚Å‚n‚j */
		}
		char work[1024], sname[config::MAX_SESSION_NAME], *buff;
		const char *line;
		while(fgets(work, sizeof(work), fp)) {
			buff = nbr_str_chop(work);
			if (config::emptyline(buff)) { continue; }
			else if (config::commentline(buff)) { continue; }
			if (!(line = nbr_str_divide_tag_and_val('.', buff, sname, sizeof(sname)))) {
				log(INFO, "%s: no sname specified\n", buff);
				continue;
			}
			config *cfg = m_cl.find(sname);
			if (!cfg) {
				log(INFO, "%s: no such sname\n", sname);
				continue;
			}
			if (cfg->load(line) < 0) {
				log(INFO, "%s: invalid config\n", line);
				continue;
			}
			log(INFO, "apply config<%s.%s>\n", sname, line);
		}
		if (fp) { fclose(fp); }
	}
	return NBR_OK;
}

int
daemon::on_signal(int signo)
{
	if (signo == SIGTERM || signo == SIGINT) {
		return -1;
	}
	return NBR_OK;
}

int
daemon::boot(int argc, char *argv[])
{
	return NBR_OK;
}

int
daemon::initlib(CONFIG &c)
{
	return NBR_OK;
}

int
daemon::create_config(config *cl[], int size)
{
	if (size <= 0) {
		return NBR_ESHORT;
	}
	cl[0] = new basic_property;
	return 1;
}

factory*
daemon::create_factory(const char *sname)
{
	return NULL;
}
