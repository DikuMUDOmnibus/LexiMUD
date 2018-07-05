/*
 * IMC2 - an inter-mud communications protocol
 *
 * ice.c: IMC-channel-extensions (ICE) common code
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

#include "imc.h"
#include "ice.h"

#include "utils.h"
#include "buffer.h"

//	see if someone can talk on a channel
bool ICEChannel::Audible(const char *who) {
	//	checking a mud?
	if (!strchr(who, '@')) {
		char *arg;
		const char *p;
		int invited = 0;

		if (this->policy != ICE_PRIVATE)
			return true;

		if (!str_cmp(ice_mudof(this->name), imc_name))
			return true;

		//	Private channel - can only hear if someone with the right mud name is there.
    	arg = get_buffer(IMC_DATA_LENGTH);
    	
		for (p = imc_getarg(this->invited, arg, IMC_NAME_LENGTH); *arg;
				p = imc_getarg(p, arg, IMC_NAME_LENGTH)) {
			if (!str_cmp(who, arg) || !str_cmp(who, imc_mudof(arg))) {
				invited=1;
				break;
			}
		}

		if (!invited) {
			release_buffer(arg);
			return false;
		}
		
		for (p = imc_getarg(this->excluded, arg, IMC_NAME_LENGTH); *arg;
				p = imc_getarg(p, arg, IMC_NAME_LENGTH)) {
			if (!str_cmp(who, arg) || !str_cmp(who, imc_mudof(arg))) {
				release_buffer(arg);
				return false;
			}
		}
		release_buffer(arg);

		return true;
	}

	/* owners and operators always can */
	if (!str_cmp(this->owner, who) || imc_hasname(this->operators, who))
		return true;

	/* ICE locally can use any channel */
	if (!str_cmp(imc_nameof(who), "ICE") && !str_cmp(imc_mudof(who), imc_name))
		return true;
  
	if (this->policy == ICE_OPEN) {
		//	open policy. default yes. override with excludes, then invites
    
		if ((imc_hasname(this->excluded, who) || imc_hasname(this->excluded, imc_mudof(who))) &&
				!imc_hasname(this->invited, who) && !imc_hasname(this->invited, imc_mudof(who)))
			return false;
		else
			return true;
	}

	//	closed or private. default no, override with invites, then excludes
  
	if ((imc_hasname(this->invited, who) || imc_hasname(this->invited, imc_mudof(who))) &&
			!imc_hasname(this->excluded, who) && !imc_hasname(this->excluded, imc_mudof(who)))
		return true;
	else
		return false;
}


const char *ice_mudof(const char *fullname) {
	char *buf = imc_getsbuf(IMC_PNAME_LENGTH);
	char *where = buf;
	int count = 0;

	while (*fullname && *fullname != ':' && count < IMC_PNAME_LENGTH-1) {
		*where++ = *fullname++;
		count++;
	}
	
	*where = '\0';
	imc_shrinksbuf(buf);
	return buf;
}


const char *ice_nameof(const char *fullname) {
	char *buf = imc_getsbuf(IMC_MNAME_LENGTH);
	char *where;

	where = strchr(fullname, ':');
	if (!where)	imc_sncpy(buf, fullname, IMC_MNAME_LENGTH);
	else		imc_sncpy(buf, where+1, IMC_MNAME_LENGTH);

	imc_shrinksbuf(buf);
	return buf;
}
