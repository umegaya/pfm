/***************************************************************
 * idg.cpp : object id generator impl collection
 * 2009/02/26 iyatomi : create
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
#include "vm.hpp"

using namespace sfc;
using namespace sfc::vm;
/*-------------------------------------------------------------*/
/* sfc::vmd::vmdconfig					       */
/*-------------------------------------------------------------*/
int
vmdconfig::set(const char *k, const char *v)
{
	if (cmp("lang", k)) {
		strncpy(m_lang, v, MAX_VALUE_STR);
		return NBR_OK;
	}
	else if (cmp("langopt", k)) {
		strncpy(m_langopt, v, MAX_VALUE_STR);
		return NBR_OK;
	}
	else if (cmp("kvs", k)) {
		strncpy(m_kvs, v, MAX_VALUE_STR);
		return NBR_OK;
	}
	else if (cmp("kvsopt", k)) {
		strncpy(m_kvsopt, v, MAX_VALUE_STR);
		return NBR_OK;
	}
	else if (cmp("rootdir", k)) {
		strncpy(m_root_dir, v, MAX_VALUE_STR);
		return NBR_OK;
	}
	else if (cmp("max_object", k)) {
		SAFETY_ATOI(v, m_max_object, U32);
	}
	else if (cmp("max_world", k)) {
		SAFETY_ATOI(v, m_max_world, U32);
	}
	else if (cmp("rpc_entry", k)) {
		SAFETY_ATOI(v, m_rpc_entry, U32);
	}
	else if (cmp("rpc_ongoing", k)) {
		SAFETY_ATOI(v, m_rpc_ongoing, U32);
	}
	return config::set(k, v);
}

vmdconfig::vmdconfig(BASE_CONFIG_PLIST,
		char *lang, char *lopt,
		char *kvs, char *kopt,
		char *root_dir,
		int max_object, int max_world,
		int rpc_entry, int rpc_ongoing,
		int max_node, int max_replica)
	: config(BASE_CONFIG_CALL),
	  m_max_object(max_object), m_max_world(max_world),
	  m_rpc_entry(rpc_entry), m_rpc_ongoing(rpc_ongoing),
	  m_max_node(max_node), m_max_replica(max_replica)
{
	strcpy(m_lang, lang);
	strcpy(m_langopt, lopt);
	strcpy(m_kvs, kvs);
	strcpy(m_kvsopt, kopt);
	strcpy(m_root_dir, root_dir);
}
