/***************************************************************
 * httpd.h : testing suite of http.h/cpp
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
#if !defined(__ECHOD_H__)
#define __ECHOD_H__

#include "sfc.hpp"

namespace sfc {
using namespace app;
using namespace base;
class echo_sv : public session, public binprotocol {
public:
	typedef factory_impl<echo_sv> factory;
	echo_sv() : session() {}
	~echo_sv() {}
	int on_recv(char *p, int l) {
		return send(p, l);
	}
	int on_close(int r) {
		daemon::stop();
		return NBR_OK;
	}
};

class echo_cl : public session, public binprotocol {
protected:
	U32 m_cnt;
	UTIME m_start;
public:
	typedef factory_impl<echo_cl, arraypool<echo_cl> > factory;
	echo_cl() : session(), m_cnt(0) {}
	~echo_cl() {}
	int cnt() const { return m_cnt; }
public:
	int on_recv(char *p, int l) {
		m_cnt++;
		if ((nbr_time() - m_start) >= 10 * 1000 * 1000) {
			daemon::stop();
			return NBR_OK;
		}
		return send(p, l);
	}
	int on_open(const config &cfg) {
		char buf[256];
		m_start = nbr_time();
		return send(buf, sizeof(buf));
	}
};

class echod : public daemon {
	echo_cl *m_c;
	int m_server;
public:
	echod() : daemon(), m_server(0) {}
	factory *create_factory(const char *sname) {
		if (strcmp(sname, "cl") == 0) {
			return new echo_cl::factory;
		}
		if (strcmp(sname, "sv") == 0) {
			return new echo_sv::factory;
		}
		return NULL;
	}
	int		create_config(config* cl[], int size) {
		if (size <= 1) {
			return NBR_ESHORT;
		}
		cl[0] = new config (
				"cl",
				"localhost:7000",
				1,
				60, opt_not_set,
				1024, 1024,
				0, 0,	/* no ping */
				-1,0,	/* no query buffer */
				"TCP", "eth0",
				1 * 10 * 1000/* 100msec task span */,
				1/* after 10ms, again try to connect */,
				kernel::INFO,
				nbr_sock_rparser_bin16,
				nbr_sock_send_bin16,
				config::cfg_flag_not_set
				);
		cl[1] = new config (
				"sv",
				"0.0.0.0:7000",
				1,
				60, opt_not_set,
				1024, 1024,
				0, 0,	/* no ping */
				-1,0,	/* no query buffer */
				"TCP", "eth0",
				1 * 10 * 1000/* 100msec task span */,
				0/* never wait ld recovery */,
				kernel::INFO,
				nbr_sock_rparser_bin16,
				nbr_sock_send_bin16,
				config::cfg_flag_server
				);
		return 2;
	}
	int		boot(int argc, char *argv[]) {
		int r;
		echo_cl::factory *f = find_factory<echo_cl::factory>("cl");
		if (!f) { return NBR_OK; }
		if (!(m_c = f->pool().alloc())) { return NBR_EEXPIRE; }
		if ((r = f->connect(m_c, f->cfg().m_host)) < 0) { return r; }
		return r;
	}
	void	shutdown() {
		if (m_c) {
			printf("finish!\nprocess : %u times\n", m_c->cnt());
		}
	}
};

}

#endif //__ECHOD_H__
