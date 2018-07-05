/*
 * IMC2 - an inter-mud communications protocol
 *
 * ice.h: IMC-channel-extensions common defs
 *
 * Copyright (C) 1997 Oliver Jowett <oliver@jowett.manawatu.planet.co.nz>
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

#ifndef ICE_H
#define ICE_H

/* channel policy */

#define ICE_OPEN    1
#define ICE_CLOSED  2
#define ICE_PRIVATE 3

/* defined in icec-mercbase.h, etc */


class ICELChannel {
public:
					ICELChannel(void) : name(NULL), level(0), format1(NULL), format2(NULL) { };
					~ICELChannel(void);
					
	char *			name;
	int				level;
	char *			format1;
	char *			format2;
};


class ICEChannel {
public:
					ICEChannel(void) : name(NULL), owner(NULL), operators(NULL), policy(0),
							invited(NULL), excluded(NULL), active(NULL), local(NULL) { }
					~ICEChannel(void) {
						this->FreeLocal();
						if (this->name)			free(this->name);
						if (this->owner)		free(this->owner);
						if (this->operators)	free(this->operators);
						if (this->invited)		free(this->invited);
						if (this->excluded)		free(this->excluded);
						if (this->active)		free(this->active);
					}
	
	bool			Audible(const char *who);
	void			SendMessage(const char *name, const char *text, bool emote);
	void			ShowChannel(const char *from, const char *text, bool emote);
	void			FreeLocal(void);
	void			NotifyUpdate(void);
	
	char *			name;
	char *			owner;
	char *			operators;
	
	SInt32			policy;
	char *			invited;
	char *			excluded;
	char *			active;
	ICELChannel *	local;
};

//bool ice_audible(ice_channel *c, const char *who);
const char *ice_nameof(const char *fullname);
const char *ice_mudof(const char *fullname);

#endif
