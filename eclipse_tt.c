/* eclipse_tt.c: Eclipse console terminal simulator

   Copyright (c) 1993-1997,
   Robert M Supnik, Digital Equipment Corporation
   Commercial use prohibited

   tti		terminal input
   tto		terminal output
*/

#include "nova_defs.h"

#define	UNIT_V_DASHER	(UNIT_V_UF + 0)			/* Dasher mode */
#define UNIT_DASHER	(1 << UNIT_V_DASHER)
extern int32 int_req, dev_busy, dev_done, dev_disable;
t_stat tti_svc (UNIT *uptr);
t_stat tto_svc (UNIT *uptr);
t_stat tti_reset (DEVICE *dptr);
t_stat tto_reset (DEVICE *dptr);
t_stat ttx_setmod (UNIT *uptr, int32 value);
extern t_stat sim_activate (UNIT *uptr, int32 delay);
extern t_stat sim_cancel (UNIT *uptr);
extern t_stat sim_poll_kbd (void);
extern t_stat sim_putchar (int32 out);

/* TTI data structures

   tti_dev	TTI device descriptor
   tti_unit	TTI unit descriptor
   tti_reg	TTI register list
   ttx_mod	TTI/TTO modifiers list
*/

UNIT tti_unit = { UDATA (&tti_svc, 0, 0), KBD_POLL_WAIT };

REG tti_reg[] = {
	{ ORDATA (BUF, tti_unit.buf, 8) },
	{ FLDATA (BUSY, dev_busy, INT_V_TTI) },
	{ FLDATA (DONE, dev_done, INT_V_TTI) },
	{ FLDATA (DISABLE, dev_disable, INT_V_TTI) },
	{ FLDATA (INT, int_req, INT_V_TTI) },
	{ DRDATA (POS, tti_unit.pos, 31), PV_LEFT },
	{ DRDATA (TIME, tti_unit.wait, 24), REG_NZ + PV_LEFT },
	{ FLDATA (MODE, tti_unit.flags, UNIT_V_DASHER), REG_HRO },
	{ NULL }  };

MTAB ttx_mod[] = {
	{ UNIT_DASHER, 0, "ANSI", "ANSI", &ttx_setmod },
	{ UNIT_DASHER, UNIT_DASHER, "Dasher", "DASHER", &ttx_setmod },
	{ 0 }  };

DEVICE tti_dev = {
	"TTI", &tti_unit, tti_reg, ttx_mod,
	1, 10, 31, 1, 8, 8,
	NULL, NULL, &tti_reset,
	NULL, NULL, NULL };

/* TTO data structures

   tto_dev	TTO device descriptor
   tto_unit	TTO unit descriptor
   tto_reg	TTO register list
*/

UNIT tto_unit = { UDATA (&tto_svc, 0, 0), SERIAL_OUT_WAIT };

REG tto_reg[] = {
	{ ORDATA (BUF, tto_unit.buf, 8) },
	{ FLDATA (BUSY, dev_busy, INT_V_TTO) },
	{ FLDATA (DONE, dev_done, INT_V_TTO) },
	{ FLDATA (DISABLE, dev_disable, INT_V_TTO) },
	{ FLDATA (INT, int_req, INT_V_TTO) },
	{ DRDATA (POS, tto_unit.pos, 31), PV_LEFT },
	{ DRDATA (TIME, tto_unit.wait, 24), PV_LEFT },
	{ FLDATA (MODE, tto_unit.flags, UNIT_V_DASHER), REG_HRO },
	{ NULL }  };

DEVICE tto_dev = {
	"TTO", &tto_unit, tto_reg, ttx_mod,
	1, 10, 31, 1, 8, 8,
	NULL, NULL, &tto_reset,
	NULL, NULL, NULL };
	
	

/* Terminal input: IOT routine */

int32 tti (int32 pulse, int32 code, int32 AC)
{
int32 iodata;

iodata = (code == ioDIA)? tti_unit.buf & 0377: 0;
switch (pulse) {					/* decode IR<8:9> */
case iopS: 						/* start */
	dev_busy = dev_busy | INT_TTI;			/* set busy */
	dev_done = dev_done & ~INT_TTI;			/* clear done, int */
	int_req = int_req & ~INT_TTI;
	break;
case iopC:						/* clear */
	dev_busy = dev_busy & ~INT_TTI;			/* clear busy */
	dev_done = dev_done & ~INT_TTI;			/* clear done, int */
	int_req = int_req & ~INT_TTI;
	break;  }					/* end switch */
return iodata;
}

/* Unit service */

t_stat tti_svc (UNIT *uptr)
{
int32 temp;

sim_activate (&tti_unit, tti_unit.wait);		/* continue poll */
if ((temp = sim_poll_kbd ()) < SCPE_KFLAG) return temp;	/* no char or error? */
tti_unit.buf = temp & 0177;
/* --- BEGIN MODIFIED CODE --- */
if (tti_unit.flags & UNIT_DASHER)			/* translate input */
	translate_in();
/* ---  END  MODIFIED CODE --- */	 				
dev_busy = dev_busy & ~INT_TTI;				/* clear busy */
dev_done = dev_done | INT_TTI;				/* set done */
int_req = (int_req & ~INT_DEV) | (dev_done & ~dev_disable);
tti_unit.pos = tti_unit.pos + 1;
return SCPE_OK;
}

/* -------------------- BEGIN INSERTION -----------------------*/

int curpos = 0;		/* used by translate_out() */
int row = 0, col = 0;	/* ditto - for cursor positioning */
int spec200 = 0;	/* signals next char is 'special' */

/* Translation: Vt100 input to D200 keycodes. */

int32 translate_in()
{
    char rev = 0;
    
    if (tti_unit.buf == '\r')
        rev = '\n';	
    if (tti_unit.buf == '\n')
        rev = '\r';	
    if (rev)
        tti_unit.buf = rev;	
}

/* --------------------  END  INSERTION -----------------------*/

/* Reset routine */

t_stat tti_reset (DEVICE *dptr)
{
tti_unit.buf = 0;
dev_busy = dev_busy & ~INT_TTI;				/* clear busy */
dev_done = dev_done & ~INT_TTI;				/* clear done, int */
int_req = int_req & ~INT_TTI;
sim_activate (&tti_unit, tti_unit.wait);		/* activate unit */
return SCPE_OK;
}

/* Terminal output: IOT routine */

int32 tto (int32 pulse, int32 code, int32 AC)
{
if (code == ioDOA) tto_unit.buf = AC & 0377;
switch (pulse) {					/* decode IR<8:9> */
case iopS: 						/* start */
	dev_busy = dev_busy | INT_TTO;			/* set busy */
	dev_done = dev_done & ~INT_TTO;			/* clear done, int */
	int_req = int_req & ~INT_TTO;
	sim_activate (&tto_unit, tto_unit.wait);	/* activate unit */
	break;
case iopC:						/* clear */
	dev_busy = dev_busy & ~INT_TTO;			/* clear busy */
	dev_done = dev_done & ~INT_TTO;			/* clear done, int */
	int_req = int_req & ~INT_TTO;
	sim_cancel (&tto_unit);				/* deactivate unit */
	break;  }					/* end switch */
return 0;
}

/* Unit service */

t_stat tto_svc (UNIT *uptr)
{
int32 c, temp;

dev_busy = dev_busy & ~INT_TTO;				/* clear busy */
dev_done = dev_done | INT_TTO;				/* set done */
int_req = (int_req & ~INT_DEV) | (dev_done & ~dev_disable);
c = tto_unit.buf & 0177;
/* --- BEGIN MODIFIED CODE --- */
if (tto_unit.flags & UNIT_DASHER) {
	if ((temp = translate_out(c)) != SCPE_OK) return temp;
} else {	
	if ((temp = sim_putchar (c)) != SCPE_OK) return temp;
	tto_unit.pos = tto_unit.pos + 1;
}	
/* ---  END  MODIFIED CODE --- */	
return SCPE_OK;
}

/* -------------------- BEGIN INSERTION -----------------------*/

/* Translation routine - D200 screen controls to VT-100 controls. */

int32 translate_out(int32 c)
{
	int32 temp;
	char outstr[32];
	
	if (spec200 == 1) {		/* Special terminal control seq */
		spec200 = 0;
		switch (c) {
			case 'C':	/* read model ID */
				return SCPE_OK;
			case 'E':	/* Reverse video off */
				return SCPE_OK;
			case 'D':	/* Reverse video on */
				return SCPE_OK;
			default:
				return SCPE_OK;			
		}
	}
	if (curpos == 1) {		/* 2nd char of cursor position */
		col = c & 0x7f;
		curpos++;
		return (SCPE_OK);
	}	
	if (curpos == 2) {		/* 3rd char of cursor position */
		row = c & 0x7f;
		curpos = 0;
		sprintf(outstr, "\033[%d;%dH", row+1, col+1);
		if ((temp = putseq(outstr)) != SCPE_OK) return temp;
		return (SCPE_OK);
	}	
	switch (c) {			/* Single-char command or data */
		case 003:		/* Blink enable */
			break;
		case 004:		/* Blink disable */
			break;
		case 005:		/* Read cursor address */
			break;	
		case 010:		/* Cursor home */
			sprintf(outstr, "\033[1;1H");
			if ((temp = putseq(outstr)) != SCPE_OK) return temp;
			row = col = 0;
			return (SCPE_OK);
		case 012:		/* Newline */
			if ((temp = sim_putchar('\r')) != SCPE_OK) return temp;
			tto_unit.pos += 1;
			if ((temp = sim_putchar(c)) != SCPE_OK) return temp;
			tto_unit.pos += 1;
			col = 1;
			row++;
			if (row > 24) row = 1;
			return (SCPE_OK);
		case 013:		/* Erase EOL */
			sprintf(outstr, "\033[K");
			if ((temp = putseq(outstr)) != SCPE_OK) return temp;
			return (SCPE_OK);
		case 014:		/* Erase screen */
			sprintf(outstr, "\033[1;1H\033[2J");
			if ((temp = putseq(outstr)) != SCPE_OK) return temp;
			row = col = 0;
			return (SCPE_OK);
		case 015:		/* CR */
			if ((temp = sim_putchar(c)) != SCPE_OK) return temp;
			tto_unit.pos += 1;
			col = 1;
			return (SCPE_OK);
		case 016:		/* Blink On */
			sprintf(outstr, "\033[5m");
			if ((temp = putseq(outstr)) != SCPE_OK) return temp;
			return (SCPE_OK);
		case 017:		/* Blink off */
			sprintf(outstr, "\033[25m");
			if ((temp = putseq(outstr)) != SCPE_OK) return temp;
			return (SCPE_OK);
		case 020:		/* Write cursor address */
			curpos = 1;
			return SCPE_OK;
		case 024:		/* underscore on */
			sprintf(outstr, "\033[4m");
			if ((temp = putseq(outstr)) != SCPE_OK) return temp;
			return (SCPE_OK);
		case 025:		/* underscore off */
			sprintf(outstr, "\033[24m");
			if ((temp = putseq(outstr)) != SCPE_OK) return temp;
			return (SCPE_OK);
			break;
		case 027:		/* cursor up */
			sprintf(outstr, "\033[A");
			if ((temp = putseq(outstr)) != SCPE_OK) return temp;
			row--;
			if (row < 1) row = 24;
			return (SCPE_OK);
		case 030:		/* cursor right */
			sprintf(outstr, "\033[C");
			if ((temp = putseq(outstr)) != SCPE_OK) return temp;
			col++;
			if (col > 80) {
				col = 1;
				row++;
				if (row > 24) row = 1;
			}	
			return (SCPE_OK);
		case 031:		/* Cursor left */
			sprintf(outstr, "\033[D");
			if ((temp = putseq(outstr)) != SCPE_OK) return temp;
			tto_unit.pos += 1;
			col--;
			if (col < 1) {
				col = 80;
				row--;
				if (row < 1) row = 24;
			}	
			return (SCPE_OK);
		case 032:		/* Cursor down */
			sprintf(outstr, "\033[B");
			if ((temp = putseq(outstr)) != SCPE_OK) return temp;
			row++;
			if (row > 24) row = 1;
			return (SCPE_OK);
		case 034:		/* Dim on */
			sprintf(outstr, "\033[22m");
			if ((temp = putseq(outstr)) != SCPE_OK) return temp;
			return (SCPE_OK);
		case 035:		/* Dim off */
			sprintf(outstr, "\033[1m");
			if ((temp = putseq(outstr)) != SCPE_OK) return temp;
			return (SCPE_OK);
		case 036:		/* Special sequence */
			spec200 = 1;
			return SCPE_OK;				
		default:		/* ..A character of data */
			if ((temp = sim_putchar(c)) != SCPE_OK) return temp;
			tto_unit.pos += 1;
			col++;
			if (col > 80) {
				col = 1;
				row++;
				if (row > 24) row = 24;
			}	
			return (SCPE_OK);
	}
	return SCPE_OK;
}

int32 putseq(char *seq)
{
	int i, len, temp;
	
	len = strlen(seq);
	for (i = 0; i < len; i++) {
		if ((temp = sim_putchar(seq[i])) != SCPE_OK) 
		     return temp;
		tto_unit.pos += 1;
	}
	return SCPE_OK; 
}

/* --------------------  END  INSERTION -----------------------*/

/* Reset routine */

t_stat tto_reset (DEVICE *dptr)
{
tto_unit.buf = 0;
dev_busy = dev_busy & ~INT_TTO;				/* clear busy */
dev_done = dev_done & ~INT_TTO;				/* clear done, int */
int_req = int_req & ~INT_TTO;
sim_cancel (&tto_unit);					/* deactivate unit */
return SCPE_OK;
}

t_stat ttx_setmod (UNIT *uptr, int32 value)
{
tti_unit.flags = (tti_unit.flags & ~UNIT_DASHER) | value;
tto_unit.flags = (tto_unit.flags & ~UNIT_DASHER) | value;
return SCPE_OK;
}