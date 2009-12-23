#include "sfc.hpp"
#include <stdarg.h>
#include <signal.h>

U32 daemon::m_sigflag = 0;

void
daemon::sigfunc(int signo)
{
	m_sigflag |= (1 << signo);
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
daemon::init(int argc, char *argv[], config *list[], int n_list)
{
	int r;
	CONFIG libcfg;
	nbr_get_default(&libcfg);
	if ((r = initlib(libcfg)) < 0) {
		log(LOG_ERROR, "fail to library parameter (%d)\n", r);
		return r;
	}
	if ((r = nbr_init(&libcfg)) < 0) {
		log(LOG_ERROR, "fail to init library(%d)\n", r);
		return r;
	}
	if (!m_cl.init(n_list, n_list)) {
		log(LOG_ERROR, "config list too big? (%d)\n", m_cl.size());
		return NBR_EEXPIRE;
	}
	cmap::iterator c;
	for (int i = 0; i < n_list; i++) {
		config *cfg;
		if (!(cfg = create_config(list[i]->m_name))) {
			log(LOG_ERROR, "%s: fail to create config\n", list[i]->m_name);
			return NBR_EINVAL;
		}
		cfg->set(*list[i]);
		c = m_cl.insert(cfg, list[i]->m_name);
		if (c == m_cl.end()) {
			return NBR_EEXPIRE;
		}
	}
	if ((r = read_config(argc, argv)) < 0) {
		log(LOG_ERROR, "read_config fail (%d)\n", r);
		return NBR_ECONFIGURE;
	}
	if (!m_sl.init(m_cl.size(), m_cl.size())) {
		log(LOG_ERROR, "session factory list too big? (%d)\n", m_cl.size());
		return NBR_EEXPIRE;
	}
	smap::iterator s;
	for (c = m_cl.begin(); c != m_cl.end(); c = m_cl.next(c)) {
		session::factory *f;
		if (!(f = create_factory(c->m_name))) {
			log(LOG_ERROR, "%s: fail to create factory\n", c->m_name);
			return NBR_EINVAL;
		}
		if ((r = f->init(*c)) < 0) {
			log(LOG_ERROR, "%s: init error %d\n", c->m_name, r);
			return r;
		}
		s = m_sl.insert(f, c->m_name);
		if (s == m_sl.end()) {
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
	smap::iterator s;
	for (s = m_sl.begin(); s != m_sl.end(); s = m_sl.next(s)) {
		s->fin();
	}
	m_sl.fin();
	cmap::iterator c;
	for (c = m_cl.begin(); c != m_cl.end(); c = m_cl.next(c)) {
		c->fin();
	}
	m_cl.fin();
	nbr_fin();
}

int
daemon::log(int prio, const char *fmt, ...)
{
	char buff[4096];

	va_list v;
	va_start(v, fmt);
	vsnprintf(buff, sizeof(buff) - 1, fmt, v);
	va_end(v);

	fprintf(stderr, "%u:%s", prio, buff);
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
			log(LOG_INFO, "%s: cannot open\n", a_files[i]);
			continue;	/* –³‚©‚Á‚½‚çdefaultÝ’è‚Ås‚­‚Ì‚Å‚n‚j */
		}
		char work[1024], sname[config::MAX_SESSION_NAME], *buff;
		const char *line;
		while(fgets(work, sizeof(work), fp)) {
			buff = nbr_str_chop(work);
			if (config::emptyline(buff)) { continue; }
			else if (config::commentline(buff)) { continue; }
			if (!(line = nbr_str_divide_tag_and_val('.', buff, sname, sizeof(sname)))) {
				log(LOG_INFO, "%s: no sname specified\n", buff);
				continue;
			}
			config *cfg = m_cl.find(sname);
			if (!cfg) {
				log(LOG_INFO, "%s: no such sname\n", sname);
				continue;
			}
			if (cfg->load(line) < 0) {
				log(LOG_INFO, "%s: invalid config\n", line);
				continue;
			}
		}
		if (fp) { fclose(fp); }
	}
	return NBR_OK;
}

int
daemon::on_signal(int signo)
{
	if (signo == SIGTERM) {
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

config*
daemon::create_config(const char *sname)
{
	return new util::config;
}

session::factory*
daemon::create_factory(const char *sname)
{
	return new session::factory_impl<session>;
}
