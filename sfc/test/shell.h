/***************************************************************
 * shell.h : testing suite of session.h (cluster feature)
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
#if !defined(__SHELL_H__)
#define __SHELL_H__

#include "sfc.hpp"

namespace sfc {
using namespace app;
using namespace cluster;
class shelld : public daemon {
public:
	class protocol : public textprotocol {
	public:
		static const char cmd_list[];
		static const char cmd_copyinit[];
		static const char cmd_copychunk[];
		static const char cmd_exec[];
		static const char code_list[];
		static const char code_copyinit[];
		static const char code_copychunk[];
		static const char code_exec_start[];
		static const char code_exec_result[];
		static const char code_exec_end[];
		static char *debug_big_data;
		static const size_t debug_big_datasize = 16;//(64 * 1024);
	public:
		void recv_cmd_list(ucast_sender *u, U32 msgid) { ASSERT(false);return ; }
		void recv_cmd_copyinit(U32 msgid, const char *path, int n_chunk, int chunksz) { ASSERT(false);return ; }
		void recv_cmd_copychunk(U32 msgid, int chunkno, const char *chunk, int clen) { ASSERT(false);return ; }
		void recv_cmd_exec(U32 msgid, const char *path) { ASSERT(false);return ; }
		void recv_cmd_bcast(U32 msgid, const char *cmd, int cmdl) { ASSERT(false);return ; }
		void recv_cmd_unicast(U32 msgid, const char *addr, const char *cmd, int cmdl) { ASSERT(false);return ; }
		void recv_code_list(U32 msgid, int n_host, const char *p_hostdata[]) { ASSERT(false);return ; }
		void recv_code_copyinit(U32 msgid, const char *result) { ASSERT(false);return ; }
		void recv_code_copychunk(U32 msgid, const char *result) { ASSERT(false);return ; }
		void recv_code_exec_start(U32 msgid, const char *result) { ASSERT(false);return ; }
		void recv_code_exec_result(U32 msgid, const char *line) { ASSERT(false);return ; }
		void recv_code_exec_end(U32 msgid, const char *result) { ASSERT(false);return ; }
	public:
		static int sendping(class session &s, UTIME ut);
		static int recvping(class session &s, char *p, int l);
	};
	template <class S>
	class protocol_impl : public protocol {
	public:
		static int on_recv(S &, ucast_sender *, char *, int);
		int send_cmd_list(S &s, U32 msgid);
		int send_cmd_copyinit(S &s, U32 msgid, const char *dst, int n_chunk, int chunksz);
		int send_cmd_copychunk(S &s, U32 msgid, int chunkno, const char *chunk, int clen);
		int send_cmd_exec(S &s, U32 msgid, const char *cmd);
		int send_code_list(S &s, U32 msgid, const char *hostdata[], int n_host);
		int send_code_copyinit(S &s, U32 msgid, const char *result);
		int send_code_copychunk(S &s, U32 msgid, const char *result);
		int send_code_exec_start(S &s, U32 msgid, const char *result);
		int send_code_exec_result(S &s, U32 msgid, const char *line);
		int send_code_exec_end(S &s, U32 msgid, const char *result);
	};
	class shell_ucaster : public ucast_sender, public protocol_impl<shell_ucaster> {
	public:
		void recv_cmd_exec(U32 msgid, const char *cmd);
		int senddata(U32 msgid, const char *p, int l) {
			return ucast_sender::senddata(msgid, p, l + 1);
		}
	};
	class shell_bcaster : public bcast_sender {
	public:
		int senddata(U32 msgid, const char *p, int l) {
			return bcast_sender::senddata(msgid, p, l + 1);
		}
	};
#if 1
	class shell_node : public node {
	public:
		int m_textmode;
		int m_master_node;
	public:
		shell_node(int tm, int mn) : node(),
			m_textmode(tm), m_master_node(mn) {}
		bool master_node() const { return m_master_node != 0; }
		int senddata(U32 msgid, char *p, int l) {
			if (m_textmode) {
				return cfg().m_fns(m_sk, p, l);
			}
			else {
				return cfg().m_fns(m_sk, p, l + 1/* send last '\0' also */);
			}
		}
	};
#else
	typedef session shell_node;
#endif
	typedef shell_node servant;
	typedef shell_node master;
	class shellclient : public servant, public protocol_impl<shellclient> {
	protected:
		char	m_cmd[256];
	public:
		typedef factory_impl<shellclient,
					arraypool<shellclient> > factory;
		typedef config property;
	public:
		shellclient() : servant(1, 0) { m_cmd[0] = '\0'; }
		~shellclient() {}
	public:
		void fin()						{}
		int on_recv(char *p, int l)		{
			return protocol_impl<shellclient>::on_recv(*this, NULL, p, l);
		}
		int on_cluster_recv(ucast_sender *u, char *p, int l) {
			ASSERT(u);
			return protocol_impl<shellclient>::on_recv(*this, u, p, l);
		}
		int on_event(char *p, int l)	{ return NBR_OK; }
		int on_open(const config &cfg);
		void setcmd(const char *cmd);
		void recv_code_exec_start(U32 msgid, const char *result);
		void recv_code_exec_end(U32 msgid, const char *result);
		void recv_code_exec_result(U32 msgid, const char *line);
		pollret poll(UTIME ut, bool from_worker);
	};
	class shellserver : public master, public protocol_impl<shellserver> {
	public:
		typedef factory_impl<shellserver> factory;
		typedef config property;
	public:
		shellserver() : master(0, 1) {}
		shellserver(int tm, int mn) : master(tm, mn) {}
		~shellserver() {}
	public:
		void fin()						{}
		int on_recv(char *p, int l)		{
			TRACE("%u: time = %llu %s(%u)\n", nbr_osdep_getpid(), nbr_time(), __FILE__, __LINE__);
			return protocol_impl<shellserver>::on_recv(*this, NULL, p, l);
		}
		int on_cluster_recv(ucast_sender *u, char *p, int l) {
			ASSERT(u);
			return protocol_impl<shellserver>::on_recv(*this, u, p, l);
		}
		int on_event(char *p, int l)	{ return NBR_OK; }
		void recv_cmd_exec(U32 msgid, const char *cmd);
		void recv_cmd_list(ucast_sender *u, U32 msgid);
		void recv_cmd_bcast(U32 msgid, const char *cmd, int cmdl);
		void recv_cmd_unicast(U32 msgid, const char *addr, const char *cmd, int cmdl);
		pollret poll(UTIME ut, bool from_worker);
	};

	class shell_connector : public shell_node, public protocol {
	public:
		typedef config property;
	public:
		shell_connector() : shell_node(0, 0) {}
		int on_cluster_recv(ucast_sender *u, char *p, int l) {
			ASSERT(u);
			return protocol_impl<shell_ucaster>::on_recv(*(shell_ucaster *)u, u, p, l);
		}
	};
	class svnt_shellserver : public shellserver {
	public:
		svnt_shellserver() : shellserver(1, 0) {}
	};
	typedef master_cluster_factory_impl<shellserver, shellserver, shell_connector>
		master_shell;
	typedef servant_cluster_factory_impl<svnt_shellserver, shell_connector>
		servant_shell;
public:
	struct exec_ctx {
		char cmd[256];
		void *s;
		shell_ucaster uc;
		U32 msgid;
		int alive;
	};
protected:
	int m_server;
	static array<exec_ctx> m_el;
	static THPOOL m_job;
public:
	shelld() : daemon(), m_server(0) {}
	factory 	*create_factory(const char *sname);
	int			create_config(config* cl[], int size);
	int			boot(int argc, char *argv[]);
	int			initlib(CONFIG &c);
	void		shutdown();
public:
	static array<exec_ctx> &allocator() { return m_el; }
	template <class S> static int	addjob(exec_ctx *ctx);
	template <class S> static void	*popen_job(void *ctx);
};
}	//namespace sfc

#endif//__SHELL_H__
