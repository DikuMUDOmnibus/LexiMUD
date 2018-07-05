/*************************************************************************
*   File: ban.c++                    Part of Aliens vs Predator: The MUD *
*  Usage: Primary code for bans                                          *
*************************************************************************/


#include "characters.h"
#include "descriptors.h"
#include "ban.h"
#include "utils.h"
#include "buffer.h"
#include "comm.h"
#include "interpreter.h"
#include "db.h"

LList<BanElement *>ban_list;

/* prototypes */
void load_banned(void);
void _write_one_node(FILE * fp, BanElement * node);
void write_ban_list(void);
int Valid_Name(char *newname);
void Read_Invalid_List(void);
ACMD(do_ban);
ACMD(do_unban);

char *ban_types[] = {
  "no",
  "new",
  "select",
  "all",
  "ERROR"
};


void load_banned(void) {
	FILE *fl;
	int i, date;
	char *site_name, *ban_type, *name;
	BanElement *next_node;

	if (!(fl = fopen(BAN_FILE, "r"))) {
		log("SYSERR: Unable to open banfile");
		perror(BAN_FILE);
		return;
	}
	
	site_name = get_buffer(BANNED_SITE_LENGTH + 1);
	ban_type = get_buffer(100);
	name = get_buffer(MAX_NAME_LENGTH + 1);

	while (fscanf(fl, " %s %s %d %s ", ban_type, site_name, &date, name) == 4) {
//		CREATE(next_node, struct ban_list_element, 1);
		next_node = new BanElement;
		strncpy(next_node->site, site_name, BANNED_SITE_LENGTH);
		next_node->site[BANNED_SITE_LENGTH] = '\0';
		strncpy(next_node->name, name, MAX_NAME_LENGTH);
		next_node->name[MAX_NAME_LENGTH] = '\0';
		next_node->date = date;

		for (i = BAN_NOT; i <= BAN_ALL; i++)
			if (!strcmp(ban_type, ban_types[i]))
				next_node->type = i;

//		next_node->next = ban_list;
//		ban_list = next_node;
		ban_list.Add(next_node);
	}

	release_buffer(site_name);
	release_buffer(ban_type);
	release_buffer(name);
	fclose(fl);
}


int isbanned(char *hostname) {
	int i;
	BanElement *banned_node;
	char *nextchar;

	if (!hostname || !*hostname)
		return 0;

	i = 0;
	for (nextchar = hostname; *nextchar; nextchar++)
		*nextchar = LOWER(*nextchar);

//	for (banned_node = ban_list; banned_node; banned_node = banned_node->next)
//		if (strstr(hostname, banned_node->site))	/* if hostname is a substring */
//			i = MAX(i, banned_node->type);
	START_ITER(iter, banned_node, BanElement *, ban_list) {
		if (strstr(hostname, banned_node->site))
			i = MAX(i, banned_node->type);
	} END_ITER(iter);

	return i;
}


/*
void _write_one_node(FILE * fp, BanElement *node) {
	if (node) {
		_write_one_node(fp, node->next);
		fprintf(fp, "%s %s %ld %s\n", ban_types[node->type], node->site, node->date, node->name);
	}
}
*/

void write_ban_list(void) {
	FILE *fl;
	BanElement *node;
	
	if (!(fl = fopen(BAN_FILE, "w"))) {
		perror("SYSERR: write_ban_list");
		return;
	}
//	_write_one_node(fl, ban_list);/* recursively write from end to start */
	START_ITER(iter, node, BanElement *, ban_list) {
		fprintf(fl, "%s %s %ld %s\n", ban_types[node->type], node->site, node->date, node->name);
	} END_ITER(iter);
	fclose(fl);
	return;
}


ACMD(do_ban) {
	char	*flag, 
			*site,
			*nextchar,
			*timestr;
	int i;
	BanElement *ban_node;
	char *	format = "%-25.25s  %-8.8s  %-10.10s  %-16.16s\r\n";

	if (!*argument) {
		if (!ban_list.Count()) {
			send_to_char("No sites are banned.\r\n", ch);
			return;
		}
		ch->Send(format,
				"Banned Site Name",
				"Ban Type",
				"Banned On",
				"Banned By");
		ch->Send(format,
				"---------------------------------",
				"---------------------------------",
				"---------------------------------",
				"---------------------------------");

/*		for (ban_node = ban_list; ban_node; ban_node = ban_node->next) {
			if (ban_node->date) {
				timestr = asctime(localtime(&(ban_node->date)));
				*(timestr + 10) = 0;
				site = timestr;
			} else
				site = "Unknown";
			ch->Send(format, ban_node->site, ban_types[ban_node->type], site, ban_node->name);
		}
*/
		START_ITER(iter, ban_node, BanElement *, ban_list) {
			if (ban_node->date) {
				timestr = asctime(localtime(&(ban_node->date)));
				*(timestr + 10) = 0;
				site = timestr;
			} else
				site = "Unknown";
			ch->Send(format, ban_node->site, ban_types[ban_node->type], site, ban_node->name);
		} END_ITER(iter);
		return;
	}
	
	site = get_buffer(MAX_INPUT_LENGTH);
	flag = get_buffer(MAX_INPUT_LENGTH);

	two_arguments(argument, flag, site);
	
	if (!*site || !*flag)
		send_to_char("Usage: ban all|select|new <site>\r\n", ch);
	else if (!(!str_cmp(flag, "select") || !str_cmp(flag, "all") || !str_cmp(flag, "new")))
		send_to_char("Flag must be ALL, SELECT, or NEW.\r\n", ch);
	else {
//		for (ban_node = ban_list; ban_node; ban_node = ban_node->next)
		START_ITER(iter, ban_node, BanElement *, ban_list) {
			if (!str_cmp(ban_node->site, site)) {
				send_to_char("That site has already been banned -- unban it to change the ban type.\r\n", ch);
				break;
			}
		} END_ITER(iter);
		if (!ban_node) {
			ban_node = new BanElement;
			strncpy(ban_node->site, site, BANNED_SITE_LENGTH);
			ban_node->site[BANNED_SITE_LENGTH] = '\0';
			for (nextchar = ban_node->site; *nextchar; nextchar++)
				*nextchar = LOWER(*nextchar);
			strncpy(ban_node->name, ch->RealName(), MAX_NAME_LENGTH);
			ban_node->name[MAX_NAME_LENGTH] = '\0';
			ban_node->date = time(0);

			for (i = BAN_NEW; i <= BAN_ALL; i++)
				if (!str_cmp(flag, ban_types[i]))
					ban_node->type = i;
	
//			ban_node->next = ban_list;
//			ban_list = ban_node;
			ban_list.Add(ban_node);

			mudlogf(NRM, LVL_STAFF, TRUE,
					"%s has banned %s for %s players.", ch->RealName(), site,
					ban_types[ban_node->type]);
			send_to_char("Site banned.\r\n", ch);
			write_ban_list();
		}
	}
	release_buffer(flag);
	release_buffer(site);
}


ACMD(do_unban) {
	BanElement *ban_node;
	char *site = get_buffer(MAX_INPUT_LENGTH);

	one_argument(argument, site);
	
	if (!*site)
		send_to_char("A site to unban might help.\r\n", ch);
	else {
		START_ITER(iter, ban_node, BanElement *, ban_list) {
			if (!str_cmp(ban_node->site, site))
				break;
		} END_ITER(iter);

		if (ban_node) {
			ban_list.Remove(ban_node);
			send_to_char("Site unbanned.\r\n", ch);
			mudlogf(NRM, LVL_STAFF, TRUE,
					"%s removed the %s-player ban on %s.",
					ch->RealName(), ban_types[ban_node->type], ban_node->site);
			delete ban_node;
			write_ban_list();
		} else
			send_to_char("That site is not currently banned.\r\n", ch);
	}
	release_buffer(site);
}


/**************************************************************************
 *  Code to check for invalid names (i.e., profanity, etc.)		  *
 *  Written by Sharon P. Goza						  *
 **************************************************************************/

#define MAX_INVALID_NAMES	200
char *invalid_list[MAX_INVALID_NAMES];

int num_invalid = 0;

int Valid_Name(char *newname) {
	int i;
	char *tempname;
	const char *	name;
	DescriptorData *dt;

	/*
	 * Make sure someone isn't trying to create this same name.  We want to
	 * do a 'str_cmp' so people can't do 'Bob' and 'BoB'.  This check is done
	 * here because this will prevent multiple creations of the same name.
	 * Note that the creating login will not have a character name yet. -gg
	 */
	LListIterator<DescriptorData *>	iter(descriptor_list);
	while ((dt = iter.Next())) {
		if (dt->character && (name = dt->character->RealName()) && !str_cmp(name, newname)) {
			return 0;
		}
	}
	
	/* return valid if list doesn't exist */
	if (!invalid_list || num_invalid < 1)
		return 1;
	
	tempname = get_buffer(MAX_INPUT_LENGTH);
	
	/* change to lowercase */
	strcpy(tempname, newname);
	for (i = 0; tempname[i]; i++)
		tempname[i] = LOWER(tempname[i]);

	/* Does the desired name contain a string in the invalid list? */
	for (i = 0; i < num_invalid; i++)
		if (strstr(tempname, invalid_list[i])) {
			release_buffer(tempname);
			return 0;
		}
	
	release_buffer(tempname);
	
	return 1;
}


void Read_Invalid_List(void) {
	FILE *fp;
	char *temp;

	if (!(fp = fopen(XNAME_FILE, "r"))) {
		perror("SYSERR: Unable to open invalid name file");
		return;
	}
	
	temp = get_buffer(MAX_INPUT_LENGTH);
	num_invalid = 0;
	while(get_line(fp, temp) && num_invalid < MAX_INVALID_NAMES)
		invalid_list[num_invalid++] = str_dup(temp);
	release_buffer(temp);
	
	if (num_invalid > MAX_INVALID_NAMES) {
		log("SYSERR: Too many invalid names; change MAX_INVALID_NAMES in ban.c");
		exit(1);		
	}

	fclose(fp);
}
