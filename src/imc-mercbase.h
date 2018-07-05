/*
 * IMC2 - an inter-mud communications protocol
 *
 * imc-mercbase.h: integrated interface defs for Merc-derived codebases.
 *                 Now also includes Circle defs.
 *
 * Copyright (C) 1996,1997 Oliver Jowett <oliver@jowett.manawatu.planet.co.nz>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef IMC_MERCBASE_H
#define IMC_MERCBASE_H

#include "imc-mercdefs.h"

/* Stick your mud ad in this #define. */
#define IMC_MUD_INFO "Aliens vs Predator: The MUD\n\r"

/* Color configuration. */

#define IMC_COLORCHAR '`'
#define IMC_COLORCHAR2 '^'

/* uncomment this if your mud supports color */
#define IMC_COLOR

/* end of color config */


/* default levels for rtell, rbeep */
#define IMC_LEVEL_RTELL 6
#define IMC_LEVEL_RBEEP 101


/** no user-servicable parts below here **/

#define IMC_RINFO   0x04
#define IMC_ICE     0x10

#define IMC_RINVIS  0x1000
#define IMC_RBEEP   0x2000
#define IMC_RTELL   0x4000


ACMD(do_rinfo);

ACMD(do_rtell);
ACMD(do_rreply);
ACMD(do_rwho);
ACMD(do_rwhois);
ACMD(do_rquery);
ACMD(do_rbeep);
ACMD(do_rfinger);

ACMD(do_imclist);
ACMD(do_rsockets);
ACMD(do_imc);
ACMD(do_rignore);
ACMD(do_rconnect);
ACMD(do_rdisconnect);

ACMD(do_mailqueue);
ACMD(do_istats);

ACMD(do_rchannels);
ACMD(do_rchanset);

ACMD(do_rping);

/* string checking (with SAM+hacks) */
void imc_markstrings(void (*markfn)(char *));

/* color conversion functions, mud->imc and imc->mud */
const char *color_mtoi(const char *);
const char *color_itom(const char *);

/* mail shell */
void imc_post_mail(CharData *from, const char *sender, const char *to_list,
                   const char *subject, const char *date, const char *text);

#endif
