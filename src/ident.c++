
#include "types.h"
#include <sys/socket.h>
#include <netdb.h>

#if defined(CIRCLE_MAC)
#include <machine/endian.h>
#endif

#include "ident.h"
#include "ban.h"
#include "utils.h"
#include "buffer.h"

extern SInt32		port;
extern bool			no_external_procs;
extern char *		GREETINGS;

IdentServer::IdentServer(void) : descriptor(-1), child(-1), pid(-1), port(113) {
#if defined(CIRCLE_UNIX)
	SInt32	fds[2];
#endif
	
	this->lookup.host = true;
	this->lookup.user = false;
	
	if (no_external_procs)
		return;		//	No external processes.
	
#if defined(CIRCLE_UNIX)
	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) < 0) {
		perror("IdentServer::IdentServer: socketpair");
		return;
	}
	
	this->descriptor = fds[0];
	this->child = fds[1];
	
	if ((this->pid = fork()) < 0) {
		perror("Error creating Ident process");
	} else if (this->pid > 0) {
		log("Starting Ident Server.");
		close(this->child);
	} else {
		close(this->descriptor);
		this->Loop();
		exit(0);
	}
#endif
}


IdentServer::~IdentServer(void) {
#if defined(CIRCLE_UNIX)
	if (this->pid > 0) {
		log("Terminating Ident Server.");
		kill(this->pid, SIGKILL);
		close(this->descriptor);
	}
#endif
}


void IdentServer::Receive(void) {
	DescriptorData *	d;
	LListIterator<DescriptorData *>		iter(descriptor_list);
	Message				msg;
	
	read(this->descriptor, (char *)&msg, sizeof(Message));
	
	switch (msg.type) {
		case Message::Nop:
		case Message::Error:
			break;
		case Message::IdRep:
			while ((d = iter.Next())) {
				if (d->descriptor == msg.fd) {
					*d->host = '\0';
					if (*msg.user) {
						strcpy(d->user_name, msg.user);
						sprintf(d->host + strlen(d->host), "%s@", msg.user);
					}
					if (*msg.host) {
						strcpy(d->host_name, msg.host);
						strcat(d->host, msg.host);
					} else
						strcat(d->host, d->host_ip);
					

					//	determine if the site is banned
					if (isbanned(d->host) == BAN_ALL) {
						d->Write("Sorry, this site is banned.\r\n");
						STATE(d) = CON_DISCONNECT;
						mudlogf(CMP , LVL_STAFF, TRUE,  "Connection attempt denied from [%s]", d->host);
					} else {	//	Log new connections
						SEND_TO_Q(GREETINGS, d);
						STATE(d) = CON_GET_NAME;
						mudlogf(CMP, LVL_STAFF, TRUE, "New connection from [%s]", d->host);
					}
					return;
				}
			}
			//	No more logging here - it means they dropped connection before
			//	domain name resolution.
//			log("SYSERR: IdentServer lookup for descriptor not connected: %d", msg.fd);
			break;
		default:
			log("SYSERR: IdentServer::Receive: msg.type = %d", msg.type);
	}
}


void IdentServer::Lookup(DescriptorData *d) {
	Message		msg;
	char *		str;
	
	if (this->pid > 0) {
		STATE(d) = CON_IDENT;
		msg.type	= Message::Ident;
		msg.address	= d->saddr;
		msg.fd		= d->descriptor;
		write(this->descriptor, (const char *)&msg, sizeof(Message));
	} else {
		if (this->lookup.host && (str = this->LookupHost(d->saddr)))	strcpy(d->host_name, str);
		if (this->lookup.user && (str = this->LookupUser(d->saddr)))	strcpy(d->user_name, str);

		*d->host = '\0';
		if (*d->user_name)	sprintf(d->host + strlen(d->host), "%s@", d->user_name);
		if (*d->host_name)	strcat(d->host, d->host_name);
		else				strcat(d->host, d->host_ip);
		SEND_TO_Q(GREETINGS, d);
	}
}


void IdentServer::Loop(void) {
	SInt32			n;
	fd_set			inp_set;
	Message			msg;
	char *			str;
	
	for (;;) {
		FD_ZERO(&inp_set);
		FD_SET(this->child, &inp_set);
		
		if (select(this->child + 1, &inp_set, (fd_set *) NULL, (fd_set *) NULL, NULL) < 0) {
			perror("IdentServer::Loop(): select");
			continue;
		}
		
		if (FD_ISSET(this->child, &inp_set)) {
			n = read(this->child, (char *)&msg, sizeof(Message));
			memset(msg.host, 0, 256);
			memset(msg.user, 0, 256);
			
			switch (msg.type) {
				case Message::Nop:
					break;
				case Message::Quit:
					write(this->child, (char *)&msg, sizeof(Message));
					close(this->child);
					exit(0);
				case Message::Ident:
					if (this->lookup.host && (str = this->LookupHost(msg.address)))		strcpy(msg.host, str);
					if (this->lookup.user && (str = this->LookupUser(msg.address)))		strcpy(msg.host, str);
					msg.type = Message::IdRep;
					break;
				default:
					log("SYSERR: IdentServer::Loop(): msg.type = %d", msg.type);
					msg.type = Message::Error;
			}
			write(this->child, (char *)&msg, sizeof(Message));
		}
	}
}


char *IdentServer::LookupHost(struct sockaddr_in sa) {
	static char			hostname[256];
	struct hostent *	hent = NULL;
	
	if (!(hent = gethostbyaddr((char *)&sa.sin_addr, sizeof(sa.sin_addr), AF_INET))) {
		perror("IdentServer::LookupHost(): gethostbyaddr");
		return NULL;
	}
	strcpy(hostname, hent->h_name);
	return hostname;
}


char *IdentServer::LookupUser(struct sockaddr_in sa) {
	static char			username[256];
	char *				string;
	struct hostent *	hent;
	struct sockaddr_in	saddr;
	SInt32				sock;
	
	string = get_buffer(MAX_INPUT_LENGTH);

	sprintf(string, "%d, %d\r\n", ntohs(sa.sin_port), port);
	
	if (!(hent = gethostbyaddr((char *) &sa.sin_addr, sizeof(sa.sin_addr), AF_INET)))
		perror("IdentServer::LookupUser(): gethostbyaddr");
	else {
		char *	result;
		saddr.sin_family	= hent->h_addrtype;
		saddr.sin_port		= htons(this->port);
		memcpy(&saddr.sin_addr, hent->h_addr, hent->h_length);
		
		if ((sock = socket(hent->h_addrtype, SOCK_STREAM, 0)) < 0) {
			perror("IdentServer::LookupUser(): socket");
			release_buffer(string);
			return NULL;
		}
		
		if (connect(sock, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
			if (errno != ECONNREFUSED)
				perror("IdentServer::LookupUser(): connect");
			close(sock);
			release_buffer(string);
			return NULL;
		}
		
		write(sock, string, strlen(string));
		
		read(sock, string, 256);
		
		close(sock);
		
		SInt32	sport, cport;
		char	*mtype, *otype;
		mtype = get_buffer(MAX_INPUT_LENGTH);
		otype = get_buffer(MAX_INPUT_LENGTH);
		
		sscanf(string, " %d , %d : %s : %s : %s ", &sport, &cport, mtype, otype, username);
		
		result = (!strcmp(mtype, "USERID") ? username : NULL);
		
		release_buffer(string);
		release_buffer(mtype);
		release_buffer(otype);
		
		return result;
	}
	release_buffer(string);
	return NULL;
}
