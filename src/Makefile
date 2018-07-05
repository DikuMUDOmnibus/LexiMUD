# Generated automatically from Makefile.in by configure.
# CircleMUD 3.0 Makefile.in - Makefile template used by 'configure'
#

# C compiler to use
CC = g++

# Any special flags you want to pass to the compiler
MYFLAGS = -Wall -Wno-unused -Wno-sign-compare

#flags for profiling (see hacker.doc for more information)
PROFILE = 

##############################################################################
# Do Not Modify Anything Below This Line (unless you know what you're doing) #
##############################################################################

BINDIR = ../bin

CFLAGS = -O $(MYFLAGS) $(PROFILE)

ACTFILES = act.comm.o act.informative.o act.movement.o act.item.o \
	act.offensive.o act.other.o act.social.o act.wizard.o
CUSTOMFILES = elevators.o newfight.o vehicles.o events.o ident.o
OLC = aedit.o cedit.o hedit.o medit.o oedit.o redit.o sedit.o zedit.o \
	scriptedit.o olc.o
MPROGS = mobprog.o mobcmd.o
SCRIPTENGINE = scriptengine.o triggers.o scriptcmd.o
CPPFILES = affects.o characters.o clans.o descriptors.o event.o objects.o \
	files.o index.o rooms.o scripts.o sstring.o parser.o help.o opinion.o \
	queue.o hash.o list.o
IMC = imc-config.o imc-events.o imc-interp.o imc-mail.o imc-mercbase.o \
	imc-util.o imc-version.o imc.o ice.o icec-mercbase.o icec.o

OBJFILES = comm.o $(ACTFILES) ban.o boards.o config.o constants.o db.o \
	fight.o find.o graph.o handler.o house.o interpreter.o limits.o mail.o \
	mobact.o modify.o objsave.o shop.o spec_assign.o spec_procs.o \
	spell_parser.o utils.o weather.o races.o random.o log.o buffer.o \
	track.o $(OLCFILES) $(OLC) $(CUSTOMFILES) $(MPROGS) $(SCRIPTENGINE) \
	$(CPPFILES) $(IMC)


default: 
	$(MAKE) ../bin/circle

all: 
	$(MAKE) ../bin/circle
	
# recreate the dependency file 'Depend'
depend:
	$(CC) -MM *.c++ > Depend

circle:
	$(MAKE) $(BINDIR)/circle

../bin/circle : $(OBJFILES)
	$(CC) -o $(BINDIR)/circle $(PROFILE) $(OBJFILES)

../bin/circle++ : $(OBJFILES)
	$(CC) -o $(BINDIR)/circle++ $(PROFILE) $(OBJFILES)

# remove all the compiled objects for a clean compile
clean:
	rm -f *.o

# default rule to make all of the .c files
%.o: %.c++
	@echo $<
	@$(CC) -c $(CFLAGS) $<

# Dependencies for the object files (automagically generated with
# gcc -MM)

include Depend
