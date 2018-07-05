#define MAX_HOUSES	100
#define MAX_GUESTS	10

#define HOUSE_PRIVATE	0


struct house_control_rec {
	VNum	vnum;			/* vnum of this house		*/
	VNum	atrium;		/* vnum of atrium		*/
	SInt8	exit_num;		/* direction of house's exit	*/
	time_t	built_on;		/* date this house was built	*/
	SInt8	mode;			/* mode of ownership		*/
	SInt32	owner;			/* idnum of house's owner	*/
	UInt8	num_of_guests;		/* how many guests for house	*/
	UInt32	guests[MAX_GUESTS];	/* idnums of house's guests	*/
	time_t last_payment;		/* date of last house payment   */
	long spare0;
	long spare1;
	long spare2;
	long spare3;
	long spare4;
	long spare5;
	long spare6;
	long spare7;
};



   
#define TOROOM(room, dir) (world[room].dir_option[dir] ?  world[room].dir_option[dir]->to_room : NOWHERE)

void	House_listrent(CharData *ch, VNum vnum);
void	House_boot(void);
void	House_save_all(void);
bool	House_can_enter(CharData *ch, VNum house);
void	House_crashsave(VNum vnum);
