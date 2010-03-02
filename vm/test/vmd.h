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
	/* vmdconfig */
	class vmdconfig : public config {
	public:
		char m_lang[16];
		char m_langopt[256];
		char m_kvs[16];
		char m_kvsopt[256];
		U32	m_max_object, m_rpc_entry, m_rpc_ongoing;
	public:
		vmdconfig() : config(), m_max_object(1000 * 1000),
			m_rpc_entry(1000 * 1000), m_rpc_ongoing(1000 * 1000) {}
		vmdconfig(BASE_CONFIG_PLIST,
				char *lang, char *lopt,
				char *kvs, char *kopt,
				int max_object, int rpc_entry, int rpc_ongoing);
		virtual int set(const char *k, const char *v);
	};
	class vmdmstr : public vmnode<vmdmstr> {
	public:
		typedef vmnode<vmdmstr> super;
		typedef super::mstr_base_factory factory;
		typedef super::protocol protocol;
		typedef super::script script;
		typedef super::UUID UUID;
	protected:
		struct account_info {
			UUID	m_uuid;
			U8		m_login, padd[3];
			account_info() { memset(this, 0, sizeof(*this)); }
		};
		typedef enum {
			load_purpose_invalid,
			load_purpose_login,
			load_purpose_create,
		} loadpurpose;
	protected:
		static CONHASH m_ch;
		static map<account_info, protocol::login_id> m_lm;
		static int add_conhash(const char *addr);
		static int del_conhash(const char *addr);
		static const CHNODE *lookup_conhash(UUID &uuid);
	public:
		vmdmstr() : super(this) {}
		~vmdmstr() {}
		querydata *senddata(session &s, U32 msgid, char *p, int l);
		void setaddr() {
			super::setaddr();
			if (add_conhash(addr()) < 0) { close(); }
		}
		int load_or_create_object(U32 msgid, UUID &uuid, loadpurpose lp);
	public:/* receiver */
		int recv_cmd_new_object(U32 msgid, const char *acc, UUID &uuid);
		int recv_code_new_object(querydata &q, int r, const char *acc,
				UUID &uuid, char *p, size_t l);
		int recv_cmd_login(U32 msgid, const char *acc, char *adata, size_t len);
	};
	class vmdsvnt : public vmnode<vmdsvnt> {
	public:
		typedef vmnode<vmdsvnt> super;
		typedef super::svnt_base_factory factory;
		typedef super::protocol protocol;
		typedef super::script script;
		typedef super::UUID UUID;
	protected:
		UUID m_session_uuid;
	public:
		vmdsvnt() : super(this), m_session_uuid() {}
		~vmdsvnt() {}
		querydata *senddata(session &s, U32 msgid, char *p, int l);
		factory::connector *backend_connect(address &a) {
			return sf(*this)->backend_connect(a); }
		factory::connector *backend_conn() {
			return sf(*this)->backend_conn(); }
	public:/* vmdsvnt */
		int recv_cmd_new_object(U32 msgid, const char *acc, UUID &uuid);
		int recv_code_new_object(querydata &q, int r, const char *acc,
				UUID &uuid, char *p, size_t l);
		int recv_cmd_login(U32 msgid, const char *acc, char *adata, size_t alen);
		int recv_code_login(querydata &q, int r, UUID &uuid, char *p, size_t l);
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
		querydata *senddata(session &s, U32 msgid, char *p, int l);
		factory::connector *backend_connect(address &a) {
			return sf(*this)->backend_connect(a); }
		factory::connector *backend_conn() {
			return sf(*this)->backend_conn(); }
		int on_open(const config &cfg);
	public:/* vmdclnt */
		int recv_code_login(querydata &q, int r, UUID &uuid, char *p, size_t l);
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

