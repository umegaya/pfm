/****************************************************************
 * err.h : storing error
 * 2008/07/14 iyatomi : create
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
#if !defined(__ERR_H__)
#define __ERR_H__

void	nbr_err_init();
void	nbr_err_fin();
void	nbr_err_set(const char *file, const char *func, 
			int line, int level, int err, const char *fmt, ...);
void 	nbr_err_sig(const char *msg);

#define NBR_ERROUT(lv,err,...)		\
	nbr_err_set(__FILE__,__FUNC__,__LINE__,ELV_##lv,NBR_E##err,__VA_ARGS__)
#define LASTERR				nbr_err_get()

#define	INVALID_FD	(-1)



enum {
	ELV_VERBOSE,
	ELV_INFO,
	ELV_ERROR,
	ELV_EMERG,
};

#endif
