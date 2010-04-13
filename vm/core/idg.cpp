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
using namespace sfc::idgen;
/*-------------------------------------------------------------*/
/* sfc::idgen                                                  */
/*-------------------------------------------------------------*/
const mac_idgen::UUID mac_idgen::UUID_INVALID;
mac_idgen::UUID mac_idgen::UUID_SEED;

int mac_idgen::init() 
{
	int r;
	/* TODO: load from kvs, init from mac address very first time */
	if (nbr_osdep_get_macaddr("eth0", UUID_SEED.mac) < 0) {
		return r;
	}
	return NBR_OK;
}

const mac_idgen::UUID &
mac_idgen::new_id()
{
	/* TODO: use atomic instruction */
	if (UUID_SEED.id2 == 0xFFFFFFFF) {
		UUID_SEED.id1++;
		UUID_SEED.id2 = 0;
		return UUID_SEED;
	}
	UUID_SEED.id2++;
	return UUID_SEED;
}

