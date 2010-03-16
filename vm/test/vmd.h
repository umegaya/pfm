/***************************************************************
 * vmd.h: daemon build by sfc which run with script vm
 * 2010/02/15 iyatomi : create
 *                             Copyright (C) 2008-2010 Takehiro Iyatomi
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
#if !defined(__VMD_H__)
#define __VMD_H__

#include "vm.hpp"

namespace sfc {
namespace vm {
using namespace lang;
using namespace cluster;
class vmd : public app::daemon {
public:
	/* sessions */
	class vmdmstr : public vmnode<vmdmstr> {
	public:
		typedef vmnode<vmdmstr> super;
		typedef super::mstr_base_factory factory;
		typedef super::protocol protocol;
		typedef super::world world;
		typedef super::proc_id proc_id;
		typedef super::script script;
		typedef super::UUID UUID;
	protected:
		struct account_info {
			UUID		m_uuid;
			world_id	m_login_wid;
			account_info() { memset(this, 0, sizeof(*this)); }
			bool login() const { return m_login_wid[0] != 0; }
		};
	protected:
		static map<account_info, protocol::login_id> m_lm;
	public:
		vmdmstr() : super(this) {}
		~vmdmstr() {}
		static int init_login_map(int max_user);
	public:/* receiver */
		int recv_cmd_new_object(U32 msgid,
				UUID &uuid, char *addr, size_t adrl, char *p, size_t l);
		int recv_code_new_object(querydata &q, int r, UUID &uuid, char *p, size_t l);
		int recv_cmd_login(U32 msgid, const world_id &wid, const char *acc,
				char *adata, size_t len);
		int recv_cmd_rpc(U32 msgid, UUID &uuid, proc_id &pid,
				char *p, int l, rpctype rc) { return 0; }
		int recv_code_rpc(querydata &q, char *p, size_t l, rpctype rc)
				{return 0;}
		int recv_cmd_node_ctrl(U32 msgid, const char *cmd,
				const world_id &wid, const address &a);
	};
	class vmdsvnt : public vmnode<vmdsvnt> {
	public:
		typedef vmnode<vmdsvnt> super;
		typedef super::svnt_base_factory factory;
		typedef super::loadpurpose loadpurpose;
		typedef super::protocol protocol;
		typedef super::world world;
		typedef super::world_id world_id;
		typedef super::script script;
		typedef super::UUID UUID;
	protected:
		UUID m_session_uuid;
		world_id m_wid;
	public:
		vmdsvnt() : super(this), m_session_uuid() {}
		~vmdsvnt() {}
		const UUID &verify_uuid(const UUID &uuid) {
			return GET_32(&(m_session_uuid)) ? m_session_uuid : uuid; }
		int load_or_create_object(U32 msgid,
				UUID &uuid, char *p, size_t l, loadpurpose lp,
				querydata **pq);
	public:/* vmdsvnt */
		int recv_cmd_new_object(U32 msgid, UUID &uuid,
				char *addr, size_t adrl, char *p, size_t l);
		int recv_code_new_object(querydata &q, int r, UUID &uuid, char *p, size_t l);
		int recv_cmd_login(U32 msgid, const world_id &wid, const char *acc,
				char *ath, size_t athl);
		int recv_code_login(querydata &q, int r, const world_id &wid,
				UUID &uuid, char *p, size_t l);
		int recv_notify_node_change(const char *cmd,
				const world_id &wid, const address &a);
		int recv_code_node_ctrl(querydata &q, int r, const char *cmd,
					const world_id &wid, const address &a);
	};
	class vmdclnt : public vmnode<vmdclnt> {
	public:
		typedef vmnode<vmdclnt> super;
		typedef super::clnt_base_factory factory;
		typedef super::protocol protocol;
		typedef super::script script;
		typedef super::UUID UUID;
	public:
		vmdclnt() : super(this) {}
		~vmdclnt() {}
	public:/* vmdclnt */
		int recv_code_login(querydata &q, int r, const world_id &wid, 
			UUID &uuid, char *p, size_t l);
	};
public:
	/* daemon proces */
	factory *create_factory(const char *sname);
	int	create_config(config* cl[], int size);
	int	boot(int argc, char *argv[]);
	int	initlib(CONFIG &c);
	void	shutdown();
};
}	//namespace vm;
}	//namespace sfc; 

#endif//__VMD_H__

