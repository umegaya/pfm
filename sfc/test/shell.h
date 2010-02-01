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

class shelld : public daemon {
public:
	typedef session servant;
	typedef session master;
	class protocol : public session::textprotocol {
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
		static const size_t debug_big_datasize = (1024 * 1024);
	public:
		void recv_cmd_list(U32 msgid) { ASSERT(false);return ; }
		void recv_cmd_copyinit(U32 msgid, const char *path, int n_chunk, int chunksz) { ASSERT(false);return ; }
		void recv_cmd_copychunk(U32 msgid, int chunkno, const char *chunk, int clen) { ASSERT(false);return ; }
		void recv_cmd_exec(U32 msgid, const char *path) { ASSERT(false);return ; }
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
		static int on_recv(S &, char *, int);
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
	class shellclient : public servant, public protocol_impl<shellclient> {
	protected:
		char	m_cmd[256];
	public:
		typedef shellclient::factory_impl<shellclient,
					arraypool<shellclient> > factory;
	public:
		shellclient() : servant() { m_cmd[0] = '\0'; }
		~shellclient() {}
	public:
		void fin()						{}
		int on_recv(char *p, int l)		{
			return protocol_impl<shellclient>::on_recv(*this, p, l);
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
		struct exec_ctx {
			char cmd[256];
			shellserver *s;
			U32 msgid;
			int alive;
		};
	protected:
		exec_ctx m_ctx;
	public:
		typedef shellserver::factory_impl<shellserver> factory;
	public:
		shellserver() : master() {}
		~shellserver() {}
	public:
		void fin()						{}
		int on_recv(char *p, int l)		{
			TRACE("%u: time = %llu %s(%u)\n", nbr_osdep_getpid(), nbr_time(), __FILE__, __LINE__);
			return protocol_impl<shellserver>::on_recv(*this, p, l);
		}
		int on_event(char *p, int l)	{ return NBR_OK; }
		void recv_cmd_exec(U32 msgid, const char *cmd);
		pollret poll(UTIME ut, bool from_worker);
	};
protected:
	int m_server;
	static THPOOL m_job;
public:
	shelld() : daemon(), m_server(0) {}
	session::factory 	*create_factory(const char *sname);
	int					create_config(config* cl[], int size);
	int					boot(int argc, char *argv[]);
	int				 	initlib(CONFIG &c);
	void				shutdown();
	static int			addjob(shellserver::exec_ctx *ctx);
protected:
	static void			*popen_job(void *ctx);
};

}


#endif//__SHELL_H__
