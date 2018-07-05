/* ************************************************************************
*   File: comm.h                                        Part of CircleMUD *
*  Usage: header file: prototypes of public communication functions       *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define NUM_RESERVED_DESCS	8

#define COPYOVER_FILE "copyover.dat"


/* comm.c */
/*
 * Backward compatibility macros.
 */
/*
#define send_to_all(txt)			send_to_allf(txt)
#define send_to_char(txt, ch)		send_to_charf(ch, txt)
#define send_to_room(txt, ch)		send_to_roomf(ch, txt)
#define send_to_outdoor(txt)		send_to_outdoorf(txt)
#define send_to_players(ch, txt)	send_to_playersf(ch, txt)
*/
void	send_to_outdoorf(const char *messg, ...) __attribute__ ((format (printf, 1, 2)));
void	send_to_playersf(CharData *ch, const char *messg, ...) __attribute__ ((format (printf, 2, 3)));
void	send_to_all(const char *messg);
void	send_to_char(const char *messg, CharData *ch);
//void	send_to_room(const char *messg, int room);
void	send_to_outdoor(const char *messg);
void	send_to_players(CharData *ch, const char *messg);
void	send_to_zone(char *messg, int zone_rnum);

void	close_socket(DescriptorData *d);
void	echo_off(DescriptorData *d);
void	echo_on(DescriptorData *d);


#define ACT_DEBUG 1

#ifdef ACT_DEBUG
void	the_act(const char *str, int hide_invisible, CharData *ch, ObjData *obj, Ptr vict_obj, int type,
		const char *who, UInt16 line);
#define	act(str, hide_invisible, ch, obj, vict_obj, type) \
	the_act(str, hide_invisible, ch, obj, vict_obj, type, __FUNCTION__, __LINE__)
#else
void	act(const char *str, int hide_invisible, CharData *ch, ObjData *obj, Ptr vict_obj, int type);
#endif

void sub_write(char *arg, CharData *ch, bool find_invis, int targets);

#define TO_ROOM		(1 << 0)
#define TO_VICT		(1 << 1)
#define TO_NOTVICT	(1 << 2)
#define TO_CHAR		(1 << 3)
#define TO_ZONE		(1 << 4)
#define TO_GAME		(1 << 5)
#define TO_TRIG		(1 << 7)
#define TO_SLEEP	(1 << 8)	/* to char, even if sleeping */

int		write_to_descriptor(socket_t desc, char *txt);
void	write_to_q(char *txt, struct txt_q *queue, int aliased);
void	page_string(DescriptorData *d, char *str, int keep_internal);

#define USING_SMALL(d)	((d)->output == (d)->small_outbuf)
#define USING_LARGE(d)  (!USING_SMALL(d))

typedef RETSIGTYPE sigfunc(int);

extern FILE *logfile;	/* Where to send messages from log() */