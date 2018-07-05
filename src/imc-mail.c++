/*
 * IMC2 - an inter-mud communications protocol
 *
 * imc-mail.c: IMC mailer functions
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


#include "imc.h"
#include "utils.h"
#include "buffer.h"

/*
 *  general stuff
 */

/* escape a string for writing to a file */

static const char *escape(const char *data)
{
  char *buf=imc_getsbuf(IMC_DATA_LENGTH);
  char *p;

  for (p=buf; *data && (p-buf < IMC_DATA_LENGTH-1); data++, p++)
  {
    if (*data == '\n')
    {
      *p++='\\';
      *p='n';
    }
    else if (*data == '\r')
    {
      *p++='\\';
      *p='r';
    }
    else if (*data == '\\')
    {
      *p++='\\';
      *p='\\';
    }
    else if (*data == '"')
    {
      *p++='\\';
      *p='"';
    }
    else
      *p=*data;
  }

  *p=0;

  imc_shrinksbuf(buf);
  return buf;
}

/* unescape: reverse escape */
static const char *unescape(const char *data)
{
  char *buf=imc_getsbuf(IMC_DATA_LENGTH);
  char *p;
  char ch;

  for (p=buf; *data && (p-buf < IMC_DATA_LENGTH-1); data++, p++)
  {
    if (*data == '\\')
    {
      ch = *(++data);
      switch (ch)
      {
      case 'n':
	*p='\n';
	break;
      case 'r':
	*p='\r';
	break;
      case '\\':
	*p='\\';
	break;
      default:
	*p=ch;
	break;
      }
    }
    else
      *p=*data;
  }

  *p=0;

  imc_shrinksbuf(buf);
  return buf;
}



/*
 *  mail
 */

/* new_mail: create a new mail structure */
IMCMail::IMCMail(void) : from(NULL), to(NULL), text(NULL), date(NULL), subject(NULL), id(NULL),
		received(0), usage(0) {
}


/* free_mail: free a mail structure */
IMCMail::~IMCMail(void) {
	if (this->from)		FREE(this->from);
	if (this->to)		FREE(this->to);
	if (this->id)		FREE(this->id);
	if (this->text)		FREE(this->text);
	if (this->subject)	FREE(this->subject);
	if (this->date)		FREE(this->date);

	imc_cancel_event(NULL, this);
}


//	IMCMail::Write: write a single mail to a file
void IMCMail::Write(FILE *out) {
	fprintf(out, "From %s\n"
				 "To %s\n"
				 "Subject %s\n"
				 "Date %s\n"
				 "Text %s\n"
				 "ID %s\n"
				 "Received %ld\n",
			escape(this->from),
			escape(this->to),
			escape(this->subject),
			escape(this->date),
			escape(this->text),
			escape(this->id),
			this->received);
}


//	read_mail: read a single mail from a file, NULL on EOF
static IMCMail *read_mail(FILE *in) {
	IMCMail *p;
	char line[IMC_DATA_LENGTH];
	char temp[IMC_DATA_LENGTH];

	fgets(line, IMC_DATA_LENGTH, in);
	if (ferror(in) || feof(in))
		return NULL;

	p = new IMCMail();

	sscanf(line, "From %[^\n]", temp);
	p->from = str_dup(unescape(temp));
	fgets(line, IMC_DATA_LENGTH, in);
	sscanf(line, "To %[^\n]", temp);
	p->to = str_dup(unescape(temp));
	fgets(line, IMC_DATA_LENGTH, in);
	sscanf(line, "Subject %[^\n]", temp);
	p->subject = str_dup(unescape(temp));
	fgets(line, IMC_DATA_LENGTH, in);
	sscanf(line, "Date %[^\n]", temp);
	p->date = str_dup(unescape(temp));
	fgets(line, IMC_DATA_LENGTH, in);
	sscanf(line, "Text %[^\n]", temp);
	p->text = str_dup(unescape(temp));
	fgets(line, IMC_DATA_LENGTH, in);
	sscanf(line, "ID %[^\n]", temp);
	p->id = str_dup(unescape(temp));
	fgets(line, IMC_DATA_LENGTH, in);
	sscanf(line, "Received %ld", &p->received);

	p->usage = 0;

	return p;
}


/*
 *  maillist - a list of imc_mail entries
 */

LList<IMCMail *>	imc_ml_head;

/* init_ml: init the maillist */
static void init_ml(void) {
	IMCMail *mail		= new IMCMail();
	mail->to			= str_dup("");
	mail->from			= str_dup("");
	mail->date			= str_dup("");
	mail->subject		= str_dup("");
	mail->text			= str_dup("");
	mail->id			= str_dup("");
	imc_ml_head.Add(mail);
}


/* free_ml: free the maillist */

static void free_ml(void) {
	IMCMail *p;

	while ((p = imc_ml_head.Top())) {
		imc_ml_head.Remove(p);
		delete p;
	}
}


//	add_ml: add an item to the maillist
static void add_ml(IMCMail *p) {
	if (!p) {
		imc_logerror("BUG: add_ml: adding NULL pointer");
		return;
	}

	imc_ml_head.Add(p);
}


//	find_ml: find a given ID in the mail list
static IMCMail *find_ml(const char *id) {
	IMCMail *p;

	if (!id) {
		imc_logerror("BUG: find_ml: NULL id");
		return NULL;
	}

	LListIterator<IMCMail *>	iter(imc_ml_head);
	while ((p = iter.Next()))
		if (!str_cmp(p->id, id))
			return p;

	return NULL;
}

/* delete_ml: delete a given node in the mail list */

static void delete_ml(IMCMail *node) {
	if (!node)
		imc_logerror("BUG: delete_ml: NULL node");
	else if (imc_ml_head.InList(node)) {
		imc_ml_head.Remove(node);
		if (!node->usage)
			delete node;
	} else
		imc_logerror("BUG: delete_ml: node at %p not on list", node);
}

/* save_ml: save maillist */
static void save_ml(void) {
	FILE *out;
	IMCMail *p;
	char *name = get_buffer(200);

	imc_sncpy(name, imc_prefix, 190);
	strcat(name, "mail-list");

	out = fopen(name, "w");
	
	release_buffer(name);

	if (!out) {
		imc_lerror("save_ml: fopen");
		return;
	}

	LListIterator<IMCMail *>	iter(imc_ml_head);
	while ((p = iter.Next()))
		p->Write(out);

	fclose(out);
}

/* load_ml: load maillist, assumes init_ml done */
static void load_ml(void) {
	FILE *in;
	IMCMail *p;
	char *name = get_buffer(200);

	imc_sncpy(name, imc_prefix, 190);
	strcat(name, "mail-list");

	in = fopen(name, "r");
	release_buffer(name);
	if (!in)
		return;

	p = read_mail(in);
	while (p) {
		add_ml(p);
		p = read_mail(in);
	}

	fclose(in);
}




/*
 *  qnode - an entry in the 'mail to send' queue, referencing a particular
 *          piece of mail, and the mud that this entry needs to send to
 */

/* new_qnode: get a new qnode */
IMCQNode::IMCQNode(void) : data(NULL), tomud(NULL), next(NULL) {
}


/* free_qnode: free a qnode */
IMCQNode::~IMCQNode(void) {
	if (this->tomud)	FREE(this->tomud);

	if (this->data && !--this->data->usage)
		delete_ml(this->data);

	imc_cancel_event(NULL, this);
}


//	write_qnode: write a qnode to a file
void IMCQNode::Write(FILE *out) {
	fprintf(out, "%s %s\n", this->data->id, this->tomud);
}


//	read_qnode: read a qnode from a file
static IMCQNode *read_qnode(FILE *in) {
	IMCQNode *p;
	IMCMail *m;
	char line[IMC_DATA_LENGTH];
#if defined(macintosh)
	static
#endif
	char temp1[IMC_DATA_LENGTH], temp2[IMC_DATA_LENGTH];

	fgets(line, IMC_DATA_LENGTH, in);
	if (ferror(in) || feof(in))
		return NULL;

	sscanf(line, "%[^ ] %[^\n]", temp1, temp2);
	m = find_ml(temp1);
	if (!m) {
		imc_logerror("read_qnode: ID %s not in mail queue", temp1);
		return NULL;
	}

	p = new IMCQNode();

	m->usage++;
	p->data		= m;
	p->tomud	= str_dup(temp2);

	return p;
}


/*
 *  mailqueue - a list of active qnodes
 */

IMCQNode *imc_mq_head, *imc_mq_tail;


//	init_mq: init mailqueue
static void init_mq(void)
{
  imc_mq_head       = new IMCQNode();
  imc_mq_tail       = imc_mq_head;
  imc_mq_head->data = NULL;
  imc_mq_head->next = NULL;
}


//	free_mq: delete mailqueue
static void free_mq(void) {
	IMCQNode *p, *p_next;

	for (p = imc_mq_head; p; p = p_next) {
		p_next = p->next;
		delete p;
	}

	imc_mq_head = imc_mq_tail = NULL;
}

/* add_mq: add a queue of items to the tail of the mq */
static void add_mq(IMCQNode *p) {
	imc_mq_tail->next = p;
	while (p->next)
		p = p->next;
	imc_mq_tail = p;
}


#if 0
/* get_mq: extract the head of the mail queue */
static imc_qnode *get_mq(void)
{
  imc_qnode *p;

  if (imc_mq_head == imc_mq_tail)	/* empty queue */
    return NULL;

  p=imc_mq_head->next;
  imc_mq_head->next=p->next;
  if (p == imc_mq_tail)
    imc_mq_tail=imc_mq_head;

  p->next=NULL;	                        /* Just In Case */

  return p;
}
#endif

/* find_mq: find the item with the given ID/tomud values */
static IMCQNode *find_mq(const char *id, const char *tomud) {
	IMCQNode *p = NULL;

	for (p = imc_mq_head->next; p; p = p->next)
		if (!strcmp(id, p->data->id) && !str_cmp(tomud, p->tomud))
			break;

	return p;
}

/* delete_mq: delete the item with the given ID/tomud values */
static void delete_mq(const char *id, const char *tomud) {
	IMCQNode *p, *last;

	for (last = imc_mq_head, p = last->next; p; last = p, p = p->next) {
		if (!strcmp(id, p->data->id) && !str_cmp(tomud, p->tomud)) {
			last->next = p->next;
			if (p == imc_mq_tail)
			imc_mq_tail = last;
			delete p;
			return;
		}
	}
}

/* save mailqueue */
static void save_mq(void)
{
  FILE *out;
  IMCQNode *p;
  char name[200];

  imc_sncpy(name, imc_prefix, 189);
  strcat(name, "mail-queue");

  out=fopen(name, "w");
  if (!out)
  {
    imc_lerror("save_mq: fopen");
    return;
  }

  for (p=imc_mq_head->next; p; p=p->next)
    p->Write(out);

  fclose(out);
}

/* load mailqueue, assumes init_mq done */
static void load_mq(void)
{
  FILE *in;
  IMCQNode *p;
  char name[200];
  int when=10;

  imc_sncpy(name, imc_prefix, 189);
  strcat(name, "mail-queue");

  in=fopen(name, "r");
  if (!in)
    return;

  p=read_qnode(in);
  while (!feof(in) && !ferror(in))
  {
    if (p)
    {
      add_mq(p);
      imc_add_event(when, ev_qnode_send, p, 1);
      when += rand()%30+30;
    }
    p=read_qnode(in);
  }

  fclose(in);
}




/*
 *  mailid - a single mail ID that has been received
 */

//	IMCMailID::IMCMailID: get a new mailid
IMCMailID::IMCMailID(void) : id(NULL), received(0) {
}


//	IMCMailID::~IMCMailID: free a mailid
IMCMailID::~IMCMailID(void) {
	if (this->id)		free(this->id);

	imc_cancel_event(NULL, this);
}

/* generate_mailid: generate a new mailid (string) */
static char *generate_mailid(void)
{
  char *buffer=imc_getsbuf(200);

  sprintf(buffer, "%d-%d@%s", rand(), imc_sequencenumber++, imc_name);
  imc_shrinksbuf(buffer);
  return buffer;
}

/* write_mailid: write a mailid to a file */
void IMCMailID::Write(FILE * out)
{
  fprintf(out, "%s %ld\n", this->id, this->received);
}

/* read_mailid: read a mailid from a file, NULL on EOF */
static IMCMailID *read_mailid(FILE *in)
{
  IMCMailID *p;
  char line[IMC_DATA_LENGTH];
  char temp[IMC_DATA_LENGTH];
  time_t r;

  fgets(line, IMC_DATA_LENGTH, in);
  if (ferror(in) || feof(in))
    return NULL;

  sscanf(line, "%[^ ] %ld", temp, &r);

  p= new IMCMailID();

  p->id       = str_dup(temp);
  p->received = r;

  return p;
}



/*
 *  idlist - a list of mail IDs received over the last 24 hours
 */

LList<IMCMailID *>	imc_idlist;

/* init_idlist: init the ID list */
static void init_idlist(void) {
	imc_idlist.Add(new IMCMailID());
}

/* free_idlist: free the idlist */
static void free_idlist(void) {
	IMCMailID *p;

	while((p = imc_idlist.Top())) {
		imc_idlist.Remove(p);
		delete p;
	}
}

/* add_idlist: add an ID to the idlist */

static void add_idlist(IMCMailID *p) {
	imc_idlist.Add(p);
}

/* find_id: check if an ID is in the ID list */

static IMCMailID *find_id(const char *id) {
	IMCMailID *p;

	LListIterator<IMCMailID *>	iter(imc_idlist);
	while((p = iter.Next()))
		if (!strcmp(p->id, id))
			return p;

	return NULL;
}

/* flush_idlist: flush old entries from the mailseen list */
static void flush_idlist(time_t at) {
	IMCMailID *p;

	LListIterator<IMCMailID *>	iter(imc_idlist);
	while ((p = iter.Next())) {
		if (p->received < at) {
			imc_idlist.Remove(p);
			delete p;
		}
	}
}

/* save_idlist: save idlist */
static void save_idlist(void) {
	FILE *out;
	IMCMailID *p;
	char *name = get_buffer(200);

	imc_sncpy(name, imc_prefix, 191);
	strcat(name, "mail-ids");

	out=fopen(name, "w");
	
	release_buffer(name);
	
	if (!out) {
		imc_lerror("save_idlist: fopen");
		return;
	}

	LListIterator<IMCMailID *>	iter(imc_idlist);
	while((p = iter.Next()))
		p->Write(out);

	fclose(out);
}

/* load_idlist: load idlist, assumes init_idlist done */
static void load_idlist(void)
{
  FILE *in;
  IMCMailID *p;
  char name[200];

  imc_sncpy(name, imc_prefix, 191);
  strcat(name, "mail-ids");

  in=fopen(name, "r");
  if (!in)
    return;

  p=read_mailid(in);
  while (p)
  {
    add_idlist(p);
    p=read_mailid(in);
  }

  fclose(in);
}

/* datestring: generate a date string for the current time */
static char *datestring(void)
{
  char *buf=imc_getsbuf(100);

  strcpy(buf, ctime(&imc_now));
  buf[strlen(buf)-1]=0;

  imc_shrinksbuf(buf);
  return buf;
}

/* bounce: generate a local bounce note */

static void bounce(IMCMail *item, const char *source, const char *reason)
{
  char temp[IMC_DATA_LENGTH];

  sprintf(temp,
	  "Your mail of %s:\r\n"
	  " to: %s\r\n"
	  " re: %s\r\n"
	  "was undeliverable for the following reason:\r\n"
	  "\r\n"
	  "%s: %s\r\n",
	  item->date,
	  item->to,
	  item->subject,
	  source,
	  reason);

  imc_mail_arrived("Mail-daemon", imc_nameof(item->from), datestring(),
		   "Bounced mail", temp);
}

/* expire old entries in the mailid list; called once an hour */
void ev_mailid_expire(Ptr data)
{
  flush_idlist(imc_now + 24*3600);
  imc_add_event(3600, ev_mailid_expire, NULL, 1);
}

/* give up sending a given qnode */
void ev_qnode_expire(Ptr data)
{
  char temp[200];
  IMCQNode *p=(IMCQNode *)data;

  sprintf(temp, "Unable to send to %s after 12 hours, giving up", p->tomud);
  bounce(p->data, imc_name, temp);
  delete_mq(p->data->id, p->tomud);
  save_ml();
  save_mq();
}

/* try sending a qnode */
void ev_qnode_send(Ptr data) {
	IMCQNode *p = (IMCQNode *)data;
	IMCPacket out;

	save_ml();
	save_mq();

	/* send it.. */

	out.data.Init();

	sprintf(out.to, "Mail-daemon@%s", p->tomud);
	strcpy(out.from, "Mail-daemon");
	strcpy(out.type, "mail");

	out.data.AddKey("to", p->data->to);
	out.data.AddKey("from", p->data->from);
	out.data.AddKey("subject", p->data->subject);
	out.data.AddKey("date", p->data->date);
	out.data.AddKey("text", p->data->text);
	out.data.AddKey("id", p->data->id);

	out.Send();
	out.data.Free();

	//	try resending it in an hour

	imc_add_event(3600, ev_qnode_send, data, 1);
}

/* imc_recv_mailok: a mail-ok packet was received */
void imc_recv_mailok(const char *from, const char *id)
{
  delete_mq(id, imc_mudof(from));
  save_mq();
  save_ml();	       	/* we might have removed the mail if usage==0 */
}

/* imc_recv_mailrej: a mail-reject packet was received */
void imc_recv_mailrej(const char *from, const char *id, const char *reason)
{
  IMCQNode *p;

  p = find_mq(id, imc_mudof(from));
  if (!p)
    return;

  bounce(p->data, from, reason);

  delete_mq(id, imc_mudof(from));
  save_mq();
  save_ml();
}

/* addrtomud: convert a 'to' list to the local mud format (ie strip @mudname
 * when it matches imc_name)
 */

static void addrtomud(const char *list, char *output)
{
  char arg[IMC_NAME_LENGTH];

  output[0]=0;
  list=imc_getarg(list, arg, IMC_NAME_LENGTH);

  while (*arg)
  {
    if (!str_cmp(imc_name, imc_mudof(arg)))
      sprintf(output + strlen(output), "%s ", imc_nameof(arg));
    else
      sprintf(output + strlen(output), "%s ", arg);

    list=imc_getarg(list, arg, IMC_NAME_LENGTH);
  }
}

/* mudtoaddr: add the @mudname to a 'to' list for unqualified names */

static void mudtoaddr(const char *list, char *output)
{
  char arg[IMC_NAME_LENGTH];

  output[0]=0;

  list=imc_getarg(list, arg, IMC_NAME_LENGTH);

  while (*arg)
  {
    if (strchr(arg, '@') == NULL)
      sprintf(output + strlen(output), "%s@%s ", arg, imc_name);
    else
      sprintf(output + strlen(output), "%s ", arg);

    list=imc_getarg(list, arg, IMC_NAME_LENGTH);
  }

  /* chop final space */

  if (arg[0] && arg[strlen(arg) - 1] == ' ')
    arg[strlen(arg) - 1] = 0;
}

/* imc_recv_mail: a mail packet was received */

void imc_recv_mail(const char *from, const char *to, const char *date,
		   const char *subject, const char *id, const char *text) {
	IMCMailID *	mid;
	IMCPacket	out;
	char *		reason;
	char		temp[IMC_DATA_LENGTH];

	out.data.Init();
	sprintf(out.to, "Mail-daemon@%s", imc_mudof(from));
	strcpy(out.from, "Mail-daemon");

	//	check if we've already seen it
	if ((mid = find_id(id))) {
		strcpy(out.type, "mail-ok");
		out.data.AddKey("id", id);

		out.Send();
		out.data.Free();

		mid->received = imc_now;

		return;
	}

	//	check for rignores
	if (imc_isignored(from)) {
		strcpy(out.type, "mail-reject");
		out.data.AddKey("id", id);
		out.data.AddKey("reason", "You are being ignored.");

		out.Send();
		out.data.Free();
		return;
	}

	//	forward it to the mud
	addrtomud(to, temp);

	if ((reason=imc_mail_arrived(from, temp, date, subject, text)) == NULL) {
		//	it was OK

		strcpy(out.type, "mail-ok");
		out.data.AddKey("id", id);

		out.Send();
		out.data.Free();

		mid				= new IMCMailID();
		mid->id			= str_dup(id);
		mid->received	= imc_now;

		add_idlist(mid);
		save_idlist();
		return;
	}

	//	mud rejected the mail

	strcpy(out.type, "mail-reject");
	out.data.AddKey("id", id);
	out.data.AddKey("reason", reason);

	out.Send();
	out.data.Free();
}

/* imc_send_mail: called by the mud to add a piece of mail to the queue */
void imc_send_mail(const char *from, const char *to, const char *date, const char *subject, const char *text) {
	char temp[IMC_DATA_LENGTH];
	IMCMail *m;
	IMCQNode *qroot, *q;
	char arg[IMC_NAME_LENGTH];
	const char *mud = NULL;
	int when=10;

	//	set up the entry for the mail list

	m = new IMCMail();

	mudtoaddr(to, temp);					//	qualify local addresses
	m->to		= str_dup(temp);
	sprintf(temp, "%s@%s", from, imc_name);	//	qualify sender
	m->from		= str_dup(temp);
	m->date		= str_dup(date);
	m->subject	= str_dup(subject);
	m->id		= str_dup(generate_mailid());
	m->text		= str_dup(text);
	m->received	= imc_now;

	qroot = NULL;							//	initialise the local list
	to = imc_getarg(to, arg, IMC_NAME_LENGTH);

	while (*arg) {
		//	get a mudname and check if we've already added a queue entry for that
		//	mud. If not, add it
		if (strchr(arg, '@') && (mud = imc_mudof(arg)) && *mud /* && str_cmp(mud, imc_name)*/) {
			if (!strcmp(mud, "*"))	q = NULL;	//	catch the @* case - not yet implemented
			else for (q = qroot; q; q = q->next)
				if (!str_cmp(q->tomud, mud))
					break;

			if (!q) {			//	not seen yet
				//	add to the top of our mini-queue
				q			= new IMCQNode();
				q->tomud	= str_dup(mud);
				q->data		= m;
				q->next		= qroot;
				m->usage++;
				qroot = q;

				imc_add_event(when, ev_qnode_send, q, 1);
				when += rand()%30+30;
			}
		}// else if (!str_cmp(mud, imc_name)) {
			
//		}

		//	get the next address
		to = imc_getarg(to, arg, IMC_NAME_LENGTH);
	}

	if (!qroot) {			/* boggle, no foreign addresses?? */
		m->usage = 0;
		delete m;
		return;
	}

	//	add mail to mail list, add mini-queue to mail queue
	add_ml(m);
	add_mq(qroot);
	save_ml();
	save_mq();
}


//	imc_mail_startup: start up the mail subsystem
void imc_mail_startup(void) {
	init_mq();
	init_ml();
	init_idlist();

	/* order is important here: we need the maillist to resolve the ID refs in
	 * the mailqueue
	 */

	load_ml();
	load_mq();
	load_idlist();

	/* queue an expiry event */

	imc_add_event(24*3600, ev_mailid_expire, NULL, 0);
}


/* imc_mail_shutdown: shut down the mailer */
void imc_mail_shutdown(void) {
	save_mq();
	save_ml();
	save_idlist();

	free_mq();
	free_ml();
	free_idlist();

	imc_cancel_event(ev_mailid_expire, NULL);
}


/* imc_mail_showqueue: returns the current mail queue
 * buffer handling here is pretty ugly, oh well
 */
char *imc_mail_showqueue(void) {
	char *buf=imc_getsbuf(IMC_DATA_LENGTH);
	char temp[100];
	IMCQNode *p;
	int left = IMC_DATA_LENGTH;

	sprintf(buf, "%-15s %-45s %-10s %s\r\n", "From", "To", "Via", "Time");

	for (p=imc_mq_head->next; p && left > 160; p = p->next) {
		int m, s;

		m=imc_next_event(ev_qnode_send, p);
		if (m<0)
			sprintf(temp, "%-15.15s %-45.45s %-10.10s --:--\r\n",
					p->data->from, p->data->to, p->tomud);
		else {
			s=m%60;
			m/=60;
			sprintf(temp, "%-15.15s %-45.45s %-10.10s %2d:%02d\r\n",
					p->data->from, p->data->to, p->tomud, m, s);
		}
	      
		left -= strlen(temp);
		strcat(buf, temp);
	}

	if (p) {
		int count;

		for (count=0; p; p=p->next, count++) ;

		sprintf(temp, "[%d further entries omitted]\r\n", count);
		strcat(buf, temp);
	}

	imc_shrinksbuf(buf);
	return buf;
}
