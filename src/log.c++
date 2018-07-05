#include <stdarg.h>



#include "descriptors.h"
#include "characters.h"
#include "utils.h"
#include "comm.h"

#ifndef CIRCLE_MAC
void mudlogf(SInt8 type, UInt32 level, bool file, const char *format, ...) {
	va_list args;
	time_t ct = time(0);
	char *time_s = asctime(localtime(&ct));
	DescriptorData *i;
	char *mudlog_buf = (char *)malloc(MAX_STRING_LENGTH);
	char tp;

	*(time_s + strlen(time_s) - 1) = '\0';

	if (file)	fprintf(logfile, "%-19.19s :: ", time_s);

	va_start(args, format);
	if (file)
		vfprintf(logfile, format, args);
//	if (level >= 0)
	vsprintf(mudlog_buf, format, args);
	va_end(args);

	if (file)	fprintf(logfile, "\n");
	fflush(logfile);
	
//	if (level < 0)
//		return;

	START_ITER(iter, i, DescriptorData *, descriptor_list) {
		if ((STATE(i) == CON_PLAYING) && !PLR_FLAGGED(i->character, PLR_WRITING)) {
			tp = ((PRF_FLAGGED(i->character, PRF_LOG1) ? 1 : 0) +
					(PRF_FLAGGED(i->character, PRF_LOG2) ? 2 : 0));

			if ((GET_LEVEL(i->character) >= level) && (tp >= type))
				i->character->Send("`g[ %s ]`n\r\n", mudlog_buf);
		}
	} END_ITER(iter);
	FREE(mudlog_buf);
}


void log(const char *format, ...)
{
	va_list args;
	time_t ct = time(0);
	char *time_s = asctime(localtime(&ct));

	time_s[strlen(time_s) - 1] = '\0';

	fprintf(logfile, "%-19.19s :: ", time_s);

	va_start(args, format);
	vfprintf(logfile, format, args);
	va_end(args);

	fprintf(logfile, "\n");
	fflush(logfile);
}
#else
char	logBuffer[SMALL_BUFSIZE];

void mudlogf(SInt8 type, UInt32 level, bool file, const char *format, ...)
{
  va_list args;
  time_t ct = time(0);
  char *time_s = asctime(localtime(&ct));
  DescriptorData *i;
//  char *mudlog_buf = malloc(MAX_STRING_LENGTH);
  char tp;

  *(time_s + strlen(time_s) - 1) = '\0';

  if (file)
    fprintf(logfile, "%-15.15s :: ", time_s + 4);

  va_start(args, format);
  vsprintf(logBuffer, format, args);
  va_end(args);

  if (file) {
    fprintf(logfile, logBuffer);
    fprintf(logfile, "\r\n");
  }
  
  if (level < 0)
    return;

	START_ITER(iter, i, DescriptorData *, descriptor_list) {
		if ((STATE(i) == CON_PLAYING) && !PLR_FLAGGED(i->character, PLR_WRITING)) {
			tp = ((PRF_FLAGGED(i->character, PRF_LOG1) ? 1 : 0) + (PRF_FLAGGED(i->character, PRF_LOG2) ? 2 : 0));

			if ((GET_LEVEL(i->character) >= level) && (tp >= type))
				i->character->Send("`g[ %s ]`n\r\n", logBuffer);
		}
	} END_ITER(iter);
//  FREE(mudlog_buf);
}


void log(const char *format, ...)
{
	va_list args;
	time_t ct = time(0);
	char *time_s = asctime(localtime(&ct));

	*(time_s + strlen(time_s) - 1) = '\0';

	fprintf(logfile, "%-15.15s :: ", time_s + 4);

	va_start(args, format);
	vsprintf(logBuffer, format, args);
	va_end(args);

    strcat(logBuffer, "\r\n");
	fprintf(logfile, logBuffer);
}
#endif