/*************************************************************************
*   File: scripts.cp                 Part of Aliens vs Predator: The MUD *
*  Usage: Primary code for scripts and triggers                          *
*************************************************************************/



#include "scripts.h"
#include "buffer.h"
#include "utils.h"
#include "files.h"
#include "event.h"

// Internal functions
void free_varlist(LList<TrigVarData *> &vd);


LList<TrigData *>	trig_list;
LList<TrigData *>	purged_trigs;		   /* list of mtrigs to be deleted  */
LList<ScriptData *>	purged_scripts; /* list of mob scripts to be del */
IndexData *trig_index;
UInt32 top_of_trigt = 0;


/* release memory allocated for a variable list */
void free_varlist(LList<TrigVarData *> &vd) {
	TrigVarData *i;
	
	LListIterator<TrigVarData *>	iter(vd);
	while ((i = iter.Next())) {
		vd.Remove(i);
		delete i;
	}
}


CmdlistElement::CmdlistElement() {
	this->Init();
}

CmdlistElement::CmdlistElement(char *string) {
	this->Init();
	this->cmd = str_dup(string);
}


void CmdlistElement::Init(void) {
	this->cmd = NULL;
	this->original = NULL;
	this->next = NULL;
}


CmdlistElement::~CmdlistElement(void) {
	if (this->cmd)	FREE(this->cmd);
}

TrigVarData::TrigVarData(void) {
	this->name = NULL;
	this->value = NULL;
}


TrigVarData::~TrigVarData(void) {
	if (this->name)		FREE(this->name);
	if (this->value)	FREE(this->value);
}


ScriptData::ScriptData(void) : purged(false), types(0) {
}



/* release memory allocated for a script */
ScriptData::~ScriptData(void) {
	TrigData *t1; //, *t2;

	START_ITER(iter, t1, TrigData *, this->trig_list) {
		delete t1;
	} END_ITER(iter);
	trig_list.Clear();

	free_varlist(this->global_vars);
}


void ScriptData::extract(void) {
	TrigData *trig; //, *next_trig;

	if (PURGED(this)) 
		return;

	PURGED(this) = true;

	purged_scripts.Add(this);
	
	/* extract all the triggers too */
	START_ITER(iter, trig, TrigData *, this->trig_list) {
		trig->extract();
	} END_ITER(iter);
	trig_list.Clear();
}


TrigData::TrigData(void) {
	this->Init();
}


TrigData::TrigData(const TrigData *trig) {
	this->Init();
	*this = *trig;
	
//	this->nr		= trig->nr;
//	this->data_type	= trig->data_type;
	this->name		= SSShare(trig->name);
//	this->trigger_type	= trig->trigger_type;
//	this->cmdlist	= trig->cmdlist;
//	this->narg		= trig->narg;
	this->arglist	= SSShare(trig->arglist);
}


TrigData::~TrigData(void) {
//	UInt32 i;

	SSFree(this->name);
	SSFree(this->arglist);

	free_varlist(this->var_list);

    /*
     * The command list is a memory leak right now!
     *
    if (cmdlist != trigg->cmdlist || proto)
	for (i = cmdlist; i;) {
	    j = i;
	    i = i->next;
	    FREE(j->cmd);
	    FREE(j);
	}
	*/
}


void TrigData::Init(void) {
	memset(this, 0, sizeof(TrigData));
	
//	this->purged = false;
	this->nr			= -1;
/*	this->data_type		= 0;
	this->name			= NULL;
	this->arglist		= NULL;
	this->trigger_type	= 0;
	this->cmdlist		= NULL;
	this->curr_state	= NULL;
	this->narg			= 0;
	this->depth			= 0;
	this->wait_event	= NULL;
	this->var_list		= NULL;

	this->next			= NULL;
	this->next_master	= NULL;
*/
//	trig_list = this;
}


void TrigData::extract(void) {
//	TrigData *temp;
	if (PURGED(this)) 
		return;

	PURGED(this) = true;

	if (GET_TRIG_WAIT(this))
		GET_TRIG_WAIT(this)->Cancel();
	GET_TRIG_WAIT(this) = NULL;
	
//	REMOVE_FROM_LIST(this, trig_list, next_master);
	trig_list.Remove(this);
	purged_trigs.Add(this);
	
	if (GET_TRIG_RNUM(this) >= 0)
		trig_index[GET_TRIG_RNUM(this)].number--;

//	this->next = purged_trigs;
//	purged_trigs = this;
}


RNum real_trigger(VNum vnum) {
	int bot, top, mid, nr, found = -1;

	/* First binary search. */
	bot = 0;
	top = top_of_trigt;

	for (;;) {
		mid = (bot + top) / 2;

		if ((trig_index + mid)->vnum == vnum)
			return mid;
		if (bot >= top)
			break;
		if ((trig_index + mid)->vnum > vnum)
			top = mid - 1;
		else
			bot = mid + 1;
	}

	/* If not found - use linear on the "new" parts. */
	for(nr = 0; nr <= top_of_trigt; nr++) {
		if(trig_index[nr].vnum == vnum) {
			found = nr;
			break;
		}
	}
	return(found);
}


/*
 * create a new trigger from a prototype.
 * nr is the real number of the trigger.
 */
TrigData *read_trigger(VNum nr, UInt8 type) {
	int i;
	TrigData *trig;

	if (type == VIRTUAL) {
		if ((i = real_trigger(nr)) < 0) {
			log("Trigger (V) %d does not exist in database.", nr);
			return NULL;
		}
	} else
		i = nr;

	trig = new TrigData((TrigData *)trig_index[i].proto);
	
//	trig->next_master = trig_list;
//	trig_list = trig;
	trig_list.Add(trig);
		
	trig_index->number++;

	return trig;
}


void parse_trigger(FILE *trig_f, RNum nr, char *filename) {
	static int i = 0;
	int t[2], k;
	char	*line = get_buffer(256),
			*cmds, *s,
			*flags = get_buffer(256),
			*buf = get_buffer(256);
	CmdlistElement *cle;
	TrigData *trig;

	trig_index[i].vnum = nr;
	trig_index[i].number = 0;
	trig_index[i].func = NULL;

	trig = new TrigData;
	trig_index[i].proto = trig;
	
	sprintf(buf, "trig vnum %d", nr);

	trig->nr = i;

	if ((trig->name = SSfread(trig_f, buf, filename)) == NULL) {
		log("SYSERR: Null obj name or format error at or near %s", buf);
		tar_restore_file(filename);
		exit(1);
	}
	
	if (!get_line(trig_f, line)) {
		log("SYSERR: Format error in first numeric line %s", buf);
		tar_restore_file(filename);
		exit(1);
	}
	
	k = sscanf(line, "%s %d %d", flags, t, t+1);
	trig->trigger_type = asciiflag_conv(flags);
	trig->narg = ((k >= 2) ? t[0] : 0);
	trig->data_type = ((k == 3) ? t[1] : 0);

	GET_TRIG_ARG(trig) = SSfread(trig_f, buf, filename);
	
	if ((cmds = s = fread_string(trig_f, buf, filename)) == NULL) {
		log("SYSERR: Format error reading commands %s", buf);
		tar_restore_file(filename);
		exit(1);
	}

	trig->cmdlist = new CmdlistElement(strtok(s, "\n\r"));
	cle = trig->cmdlist;

	while ((s = strtok(NULL, "\n\r"))) {
		cle->next = new CmdlistElement(s);
		cle = cle->next;
	}

	top_of_trigt = i++;
	
	FREE(cmds);
	release_buffer(line);
	release_buffer(flags);
	release_buffer(buf);
}


/* recursively prints a list of triggers to a file */
void fprint_triglist(FILE *stream, struct int_list *list) {
	if (!list)
		return;
	fprint_triglist(stream, list->next);
	fprintf(stream, "%d\n", list->i);
}

