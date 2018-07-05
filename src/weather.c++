/* ************************************************************************
*   File: weather.c                                     Part of CircleMUD *
*  Usage: functions handling time and the weather                         *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */


#include "descriptors.h"
#include "characters.h"
#include "rooms.h"
#include "utils.h"
#include "comm.h"
#include "db.h"

extern struct TimeInfoData time_info;
extern int top_of_zone_table;
extern struct zone_data *zone_table;

void weather_and_time(int mode);
void another_hour(int mode);
void weather_change(void);


void weather_and_time(int mode) {
	another_hour(mode);
	if (mode)	weather_change();
}
					/*    J   F   M   A   M   J   J   A   S   O   N   D */
int days_in_month[12] = {30, 27, 30, 29, 30, 29, 30, 30, 29, 30, 29, 30};

void another_hour(int mode) {
	time_info.hours++;

	if (mode) {
		switch (time_info.hours) {
			case 5:
				sunlight = SUN_RISE;
				send_to_outdoor("The sun rises in the east.\r\n");
				break;
			case 6:
				sunlight = SUN_LIGHT;
				send_to_outdoor("The day has begun.\r\n");
				break;
			case 21:
				sunlight = SUN_SET;
				send_to_outdoor("The sun slowly disappears in the west.\r\n");
				break;
			case 22:
				sunlight = SUN_DARK;
				send_to_outdoor("The night has begun.\r\n");
				break;
			default:
				break;
		}
	}
	if (time_info.hours > 23) {	/* Changed by HHS due to bug ??? */
		time_info.hours = 0;
		time_info.day++;

		if (time_info.day > days_in_month[time_info.month]) {
			time_info.day = 0;
			time_info.month++;

			if (time_info.month > 11) {
				time_info.month = 0;
				time_info.year++;
			}
		}
	}
}


void weather_change(void) {
	int diff, change, i;
	DescriptorData *j;
	char *buf = "<ERROR>";

	for (i = 0; i <= top_of_zone_table; i++) {
		if ((time_info.month >= 9) && (time_info.month <= 16))
			diff = (zone_table[i].pressure > 985 ? -2 : 2);
		else
			diff = (zone_table[i].pressure > 1015 ? -2 : 2);

		zone_table[i].change += (dice(1, 4) * diff + dice(2, 6) - dice(2, 6));
		zone_table[i].change = MIN(zone_table[i].change, 12);
		zone_table[i].change = MAX(zone_table[i].change, -12);

		zone_table[i].pressure += zone_table[i].change;
		zone_table[i].pressure = MIN(zone_table[i].pressure, 1040);
		zone_table[i].pressure = MAX(zone_table[i].pressure, 960);

		change = 0;

		switch (zone_table[i].sky) {
			case SKY_CLOUDLESS:
				if (zone_table[i].pressure < 990)			change = 1;
				else if (zone_table[i].pressure < 1010)
					if (dice(1, 4) == 1)					change = 1;
				break;
			case SKY_CLOUDY:
				if (zone_table[i].pressure < 970)			change = 2;
				else if (zone_table[i].pressure < 990) {
					if (dice(1, 4) == 1)					change = 2;
				} else if (zone_table[i].pressure > 1030)
					if (dice(1, 4) == 1)					change = 3;
				break;
			case SKY_RAINING:
				if (zone_table[i].pressure < 970) {
					if (dice(1, 4) == 1)					change = 4;
				} else if (zone_table[i].pressure > 1030)	change = 5;
				else if (zone_table[i].pressure > 1010)
					if (dice(1, 4) == 1)					change = 5;
				break;
			case SKY_LIGHTNING:
				if (zone_table[i].pressure > 1010)			change = 6;
				else if (zone_table[i].pressure > 990)
					if (dice(1, 4) == 1)					change = 6;
				break;
			default:
				change = 0;
				zone_table[i].sky = SKY_CLOUDLESS;
				break;
		}

		switch (change) {
			case 1:
				buf = "The sky starts to get cloudy.\r\n";
				zone_table[i].sky = SKY_CLOUDY;
				break;
			case 2:
				buf = "It starts to rain.\r\n";
				zone_table[i].sky = SKY_RAINING;
				break;
			case 3:
				buf = "The clouds disappear.\r\n";
				zone_table[i].sky = SKY_CLOUDLESS;
				break;
			case 4:
				buf = "Lightning starts to show in the sky.\r\n";
				zone_table[i].sky = SKY_LIGHTNING;
				break;
			case 5:
				buf = "The rain stops.\r\n";
				zone_table[i].sky = SKY_CLOUDY;
				break;
			case 6:
				buf = "The lightning stops.\r\n";
				zone_table[i].sky = SKY_RAINING;
				break;
			default:
				break;
		}
		if ((change >= 1) && (change <= 6)) {
			START_ITER(iter, j, DescriptorData *, descriptor_list) {
				if ((STATE(j) == CON_PLAYING) && j->character && AWAKE(j->character) && OUTSIDE(j->character) && 
						(zone_table[world[IN_ROOM(j->character)].zone].number == i))
					SEND_TO_Q(buf, j);
			} END_ITER(iter);
		}
	}
}
