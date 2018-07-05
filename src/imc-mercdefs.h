/*
 * IMC2 - an inter-mud communications protocol
 *
 * imc-mercbase.h: integrated macro defs for Merc-derived codebases.
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

#ifndef IMC_MERCDEFS_H
#define IMC_MERCDEFS_H

#define CIRCLE

#ifdef IN_IMC
#include "types.h"

#include "internal.defs.h"
#include "characters.h"
#include "rooms.h"
#include "descriptors.h"
#include "interpreter.h"
#include "utils.h"
#include "comm.h"
#include "db.h"

#endif


#define NEED_STR_PREFIX		1

#define AFF_DETECT_HIDDEN	AFF_SENSE_LIFE

#define IS_NOCHAN(ch)	(CHN_FLAGGED(ch, Channel::NoShout))
#define IS_NOTELL(ch)	(CHN_FLAGGED(ch, Channel::NoTell))
#define IS_SILENT(ch)	((!IS_NPC(ch) && CHN_FLAGGED(ch, Channel::NoShout)) || \
						ROOM_FLAGGED(IN_ROOM(ch), ROOM_SOUNDPROOF))
#define IS_QUIET(ch)	(0)

#define IMC_DENY_FLAGS(ch) ((ch)->player->special.imc.deny)
#define IMC_ALLOW_FLAGS(ch) ((ch)->player->special.imc.allow)
#define IMC_DEAF_FLAGS(ch) ((ch)->player->special.imc.deaf)
#define IMC_RREPLY(ch) ((ch)->player->special.imc.rreply)
#define IMC_RREPLY_NAME(ch) ((ch)->player->special.imc.rreply_name)
#define ICE_LISTEN(ch) ((ch)->player->special.imc.listen)

#define page_to_char(str, ch) page_string((ch)->desc, (char*) str, 1)

#define IS_RINVIS(ch) (!IS_NPC(ch) && IS_SET(IMC_DEAF_FLAGS(ch), IMC_RINVIS))

#define CAN(ch, x, minlev)												\
	(!IS_NPC(ch) && !IS_SET(IMC_DENY_FLAGS(ch), (x)) &&					\
	((GET_LEVEL(ch) >= (minlev)) || IS_SET(IMC_ALLOW_FLAGS(ch), (x))))

#define CHECKIMC(ch)									\
	if (IS_NPC(ch)) {									\
		send_to_char("NPCs cannot use IMC.\n\r", (ch));	\
		return;											\
	}													\
	if (imc_active<IA_UP) {								\
		send_to_char("IMC is not active.\n\r", (ch));	\
		return;											\
	}

#define CHECKMUD(ch, m)															\
	if (str_cmp(m, imc_name) && !imc_find_reminfo(m,1))							\
		(ch)->Send("Warning: '%s' doesn't seem to be active on IMC.\n\r", (m));

#define CHECKMUDOF(ch,n) CHECKMUD(ch, imc_mudof(n))

#endif
