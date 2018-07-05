
#include "descriptors.h"

struct IdentPrefs {
};

class IdentServer {
public:
						IdentServer(void);
						~IdentServer(void);
	
	//	Functions
	void				Lookup(DescriptorData *newd);
	char *				LookupHost(struct sockaddr_in sa);
	char *				LookupUser(struct sockaddr_in sa);
	void				Receive(void);
	void				Loop(void);
	
	//	Sub-Accessors
	bool				IsActive(void) { return (this->descriptor > 0); }

	SInt32				descriptor;
	SInt32				child;
	SInt32				pid;
	SInt32				port;

	typedef struct {
		enum { Error = -1, Nop, Ident, Quit, IdRep }
							type;
		struct sockaddr_in	address;
		SInt32				fd;
		char				host[256];
		char				user[256];
	} Message;
protected:
	struct IdentPrefs {
		bool				host;
		bool				user;
	} lookup;
};
