/****************************************************************
 * err.c
 * 2008/06/26 iyatomi : create
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
#include "common.h"
#include "err.h"



/*-------------------------------------------------------------*/
/* internal types											   */
/*-------------------------------------------------------------*/
typedef struct error {
	const char	*file, *func;
	int			line, error;
	int			pid, tid;
} error_t;



/*-------------------------------------------------------------*/
/* internal values											   */
/*-------------------------------------------------------------*/
#define MAX_ERROR_HISTORY (16)
static error_t	g_err[MAX_ERROR_HISTORY];
static int		g_index = 0;
static int		g_lv	= ELV_ERROR;
static FILE		*g_outfp = NULL;



/*-------------------------------------------------------------*/
/* external methods											   */
/*-------------------------------------------------------------*/
void	
nbr_err_init()
{
	nbr_err_fin();
	g_outfp = stderr;
}

void	
nbr_err_fin()
{
	nbr_mem_zero(g_err, sizeof(g_err));
	g_index = 0;
	g_outfp = NULL;
}

void	
nbr_err_set_fp(int lv, FILE *fp)
{
	if (lv <= ELV_ERROR) {
		g_lv = lv;
	}
	else {
		NBR_ERROUT(ERROR,INVAL,"invalid priority %d", lv);
	}
	g_outfp = fp;
}

int
nbr_err_get()
{
	if (g_index > 0) {
		return g_err[g_index - 1].error;
	}
	else {
		return g_err[MAX_ERROR_HISTORY - 1].error;
	}
}

void			
nbr_err_out_stack()
{
	int i;
	error_t *e;
	for (i = g_index, e = g_err + i; i >= 0; i--,e--) {
		if (e->file == NULL) { continue; }
		fprintf(g_outfp ? g_outfp : stderr, "%s(%u):%s:%d\n", 
			e->file, e->line, e->func, e->error);
	}
	for (i = MAX_ERROR_HISTORY, e = g_err + i; i > g_index; i--,e--) {
		if (e->file == NULL) { continue; }
		fprintf(g_outfp ? g_outfp : stderr, "%s(%u):%s:%d\n", 
			e->file, e->line, e->func, e->error);
	}
}

void	
nbr_err_set(const char *file, const char *func, 
			int line, int lv, int err, const char *fmt, ...)
{
	va_list v;

	if (g_lv > lv) {
		return;
	}
	g_err[g_index].file = file;
	g_err[g_index].func = func;
	g_err[g_index].line = line;
	g_err[g_index].error = err;
	g_index++;
	if (g_index >= MAX_ERROR_HISTORY) {
		g_index = 0;
	}
	va_start(v, fmt);
	fprintf(g_outfp, "%s:%u ", file, line);
	vfprintf(g_outfp ? g_outfp : stderr, fmt, v);
	fprintf(g_outfp, "%s", "\n");
	va_end(v);
}
