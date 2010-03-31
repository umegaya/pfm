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
	/* enum session type */
	typedef enum {
		vmd_session_master = 1,
		vmd_session_servant = 2,
		vmd_session_client = 3,
	} vmd_stype;
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
		static const int vmd_session_type = vmd_session_master;
		static U32 get_msgid(factory *f) { return f->msgid(); }
		static int init_login_map(int max_user);
		static void fin_login_map() { m_lm.fin(); }
		inline script *fetch_vm();
		static inline script *fetch_vm_from_thread(THREAD);
	public:/* receiver */
		int recv_cmd_node_register(U32 msgid, const address &a);
		int recv_cmd_login(U32 msgid, const world_id &wid, const char *acc,
				char *adata, size_t len);
		int recv_code_node_ctrl(querydata &q, int r, const char *cmd,
				const world_id &wid, char *p, size_t l);
		int recv_cmd_node_ctrl(U32 msgid, const char *cmd,
				const world_id &wid, char *p, size_t l);
		/* below dummy handler */
		int recv_cmd_rpc(U32 msgid, UUID &uuid, proc_id &pid,
				char *p, int l, rpctype rc) { return 0; }
		int recv_code_rpc(querydata &q, char *p, size_t l, rpctype rc)
				{return 0;}
	protected:
		int cmd_add_node(U32 msgid, const world_id &wid, const address &a);
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
		typedef super::vm_msg vm_msg;
		typedef super::serializer serializer;
		typedef super::UUID UUID;
		typedef super::VM VM;
	protected:
		UUID m_session_uuid;
		static map<address, UUID> m_pm;	/* player object - session mapping */
#if defined(_RPC_PROF)
		static int m_client_finish;
		static UTIME m_start;
		static bool m_first;
#endif
	public:
		vmdsvnt() : super(this), m_session_uuid() {}
		~vmdsvnt() {}
		static const int vmd_session_type = vmd_session_servant;
		static U32 get_msgid(factory *f) { return cf().msgid(); }
		static int init_player_map(int max_session);
		static void fin_player_map() { m_pm.fin(); }
		inline script *fetch_vm();
		static inline script *fetch_vm_from_thread(THREAD);
		vmdsvnt *from_object(object &o) {
			address *a = m_pm.find(o.uuid());
			return a ? sf(*this)->pool().find(*a) : NULL;
		}
		void fin() {
			m_pm.erase(m_session_uuid);
			super::fin();
		}
		const UUID &session_uuid() const { return m_session_uuid; }
		const UUID &verify_uuid(const UUID &uuid) {
			return protocol::is_valid_id(m_session_uuid) ? m_session_uuid : uuid; }
		bool trust() const { return !protocol::is_valid_id(m_session_uuid); }
		int on_open(const config &);
		int on_close(int );
	public:/* vmdsvnt */
		int recv_cmd_new_object(U32 msgid, const world_id &wid,
				UUID &uuid, char *p, size_t l);
		int recv_code_new_object(querydata &q, int r, UUID &uuid, char *p, size_t l);
		int recv_cmd_login(U32 msgid, const world_id &wid, const char *acc,
				char *ath, size_t athl);
		int recv_code_login(querydata &q, int r, const world_id &wid,
				UUID &uuid, char *p, size_t l);
		int recv_notify_node_change(const char *cmd,
				const world_id &wid, const address &a);
		int recv_cmd_node_ctrl(U32 msgid, const char *cmd,
				const world_id &wid, char *p, size_t l);
		int recv_code_node_register(querydata &q, int r) { return 0; }
	protected:/* callback */
		static void exit_init_object(vmdsvnt &sender, vmdsvnt &recver,
			VM vm, int r, U32 rmsgid, rpctype rt, char *p, size_t l);
		static void exit_load_player(vmdsvnt &sender, vmdsvnt &recver,
			VM vm, int r, U32 rmsgid, rpctype rt, char *p, size_t l);
	};
	class vmdclnt : public vmnode<vmdclnt> {
	public:
		typedef vmnode<vmdclnt> super;
		typedef super::clnt_base_factory factory;
		typedef super::protocol protocol;
		typedef super::script script;
		typedef super::vm_msg vm_msg;
		typedef super::UUID UUID;
		enum client_state {
			client_state_invalid,
			client_state_login_attempt,
			client_state_resume,
			client_state_pending,
			/* invalid -> login_attempt -> state_pending <-> state_resume */
			client_state_error,
		};
		static client_state m_cs;
	protected:
		UUID m_session_uuid;
		UTIME m_last_process;
		protocol::login_id m_account;
	public:
		vmdclnt() : super(this), m_session_uuid(protocol::invalid_id()),
			m_last_process(0LL) {}
		~vmdclnt() {}
		static const int vmd_session_type = vmd_session_client;
		static U32 get_msgid(factory *f) { return f->msgid(); }
		inline script *fetch_vm();
		static inline script *fetch_vm_from_thread(THREAD);
		vmdclnt *from_object(object &o) { return NULL; }
		static void set_state(client_state cs) { m_cs = cs; }
	public:/* vmdclnt */
		int recv_code_login(querydata &q, int r, const world_id &wid, 
			UUID &uuid, char *p, size_t l);
		session::pollret poll(UTIME ut, bool from_worker);
		UTIME last_process() const { return m_last_process; }
		void set_last_process(UTIME ut) { m_last_process = ut; }
	protected:
		static void exit_main(vmdclnt &sender, vmdclnt &recver,
			VM vm, int r, U32 rmsgid, rpctype rt, char *p, size_t l);
	};
public:
	struct worker_data {
		vmdmstr::script *m_mstr_vm;
		vmdsvnt::script *m_svnt_vm;
		vmdclnt::script *m_clnt_vm;
	} *m_wkp;
	int m_wks;
	static void on_worker_event(THREAD, THREAD, char *, size_t);
public:
	/* daemon process */
	factory *create_factory(const char *sname);
	int	create_config(config* cl[], int size);
	int	boot(int argc, char *argv[]);
	int	initlib(CONFIG &c);
	int 	on_signal(int signo);
	void	shutdown();
};

inline vmd::vmdmstr::script *vmd::vmdmstr::fetch_vm()
{
	vmd::worker_data *wd =
		(vmd::worker_data *)nbr_sock_get_worker_data_from(sk());
	return wd ? wd->m_mstr_vm : NULL;
}

inline vmd::vmdsvnt::script *vmd::vmdsvnt::fetch_vm()
{
	vmd::worker_data *wd =
		(vmd::worker_data *)nbr_sock_get_worker_data_from(sk());
	return wd ? wd->m_svnt_vm : NULL;
}

inline vmd::vmdclnt::script *vmd::vmdclnt::fetch_vm()
{
	vmd::worker_data *wd =
		(vmd::worker_data *)nbr_sock_get_worker_data_from(sk());
	return wd ? wd->m_clnt_vm : NULL;
}

inline vmd::vmdmstr::script *
vmd::vmdmstr::fetch_vm_from_thread(THREAD th)
{
	vmd::worker_data *wd =
		(vmd::worker_data *)nbr_sock_get_worker_data(th);
	return wd ? wd->m_mstr_vm : NULL;
}

inline vmd::vmdsvnt::script *
vmd::vmdsvnt::fetch_vm_from_thread(THREAD th)
{
	vmd::worker_data *wd =
		(vmd::worker_data *)nbr_sock_get_worker_data(th);
#if defined(_DEBUG)
	if (wd) {
		ASSERT(wd->m_svnt_vm->thread() == th);
	}
#endif
	return wd ? wd->m_svnt_vm : NULL;
}

inline vmd::vmdclnt::script *
vmd::vmdclnt::fetch_vm_from_thread(THREAD th)
{
	vmd::worker_data *wd =
		(vmd::worker_data *)nbr_sock_get_worker_data(th);
	return wd ? wd->m_clnt_vm : NULL;
}

}	//namespace vm;
}	//namespace sfc; 

#endif//__VMD_H__

