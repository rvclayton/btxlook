# Edits made to this file will be overwritten the next time configure is run.
#
# The all, test, install, and clean targes are written to work with any make.
# The other targets are written to work with gnu make.

# prefix  The common root of all the following directories.
# bindir  Where the btxlook and btxindex executibles go.
# manext  The extension for the man pages.
# mandir  The directory where the man pages go.
# eldir   The directory where btxlook.el goes.
#
# The directories need not exist; install will make them if they don't.

prefix		= /usr/local/packages/btxlook
bindir		= $(prefix)/bin
manext		= 1
mandir		= $(prefix)/man/man$(manext)
eldir		= $(prefix)/share/emacs/site-lisp

# The things above are more likely to need editing.  The things below are
# less likely to need editing.

clibdir		= $(HOME)/lib/c
VPATH		= $(clibdir)

YACC		= bison -y
cc		= gcc
CC		= $(cc) -fno-builtin
CLDFLAGS	= -g
idirs		= -I$(clibdir)
CFLAGS		= $(CLDFLAGS) $(idirs) -Wall -ansi -pedantic -DMOREPATH=\"/bin/more\"

.SUFFIXES	: .man .$(manext)
.man.$(manext)	: 
		  cat common.man $*.man | \
		  sed 's/[$$]Date: \([^ ]*\).*[$$]/\1/' >$@

LEX		= flex

%.o		: %.rcl
		  $(LEX) -t > $*.c -Prcf $<
		  $(CC) -c -o $@ $(CLDFLAGS) $(idirs) $*.c
		  rm $*.c

INSTALL		= /usr/bin/install -c
INSTALL_PROGRAM = cp
INSTALL_DATA	= cp

all		: btxlook btxindex

cmn		= common.o sblock.o str-dupl.o yy-input.o catenate-strs.o \
		  read-line.o expand-str.o catenate-sblock.o

btxlook.objs	= btxlook.o $(cmn) bblock.o clt.o cls.o bl-file.o
btxlook		: $(btxlook.objs)
		  $(CC) -o $@ $(CLDFLAGS) $(btxlook.objs)

btxindex.objs	= btxindex.o $(cmn) bi-file.o string-table.o 
btxindex	: $(btxindex.objs)
		  $(CC) -o $@ $(CLDFLAGS) $(btxindex.objs)

install		: btxlook btxindex btxindex.$(manext) btxlook.$(manext) \
		  btxlook.el
		  $(INSTALL) -d $(bindir)
		  $(INSTALL) -d $(mandir)
		  $(INSTALL) -d $(eldir)
		  $(INSTALL_DATA) btxlook.$(manext) $(mandir)
		  $(INSTALL_DATA) btxindex.$(manext) $(mandir)
		  $(INSTALL_DATA) btxlook.el $(eldir)
	          $(INSTALL_PROGRAM) btxlook $(bindir)
	          $(INSTALL_PROGRAM) btxindex  $(bindir)

clt.o		: clt.l
		  $(LEX) -t > $*.c clt.l
		  $(CC) -c -o $@ $(idirs) $(CLDFLAGS) $*.c

cls.o		: cls.c
		  $(CC) -c -o $@ $(idirs) $(CLDFLAGS) $*.c

cls.h cls.c	: cls.y
		  $(YACC) -d cls.y
		  mv y.tab.c cls.c
		  mv y.tab.h cls.h

sysdefs.h	: find-defs
		  CC=$(cc) ./find-defs

bar		:
		  crm
		  (d=`basename $$PWD`; cd .. ; bar cr $$d ; bar ; eject)

tar		: makefile.in configure find-defs btxlook.man btxindex.man \
		  common.man bblock.c bi-file.rcl bl-file.rcl btxindex.c \
		  btxlook.c cls.y clt.l common.c sblock.c string-table.c \
		  bblock.h bl-common.h common.h sblock.h string-table.h \
		  btxlook.el install-sh Readme History tst/makefile.in \
		  tst/tst.bib tst/tst.out
		  sed "s/@date@/`date +'%y %h %d'`/" < Readme >readme
		  rm -f Readme
		  mv readme Readme
		  chmod -w Readme
		  tar cf btxlook-`date +%y%h%d | tr 'A-Z' 'a-z'`.tar $^

test		: btxindex btxlook
		  @cd ../tst ; $(MAKE) -s

clean		:
		  rm -rf *.o *.$(manext) cls.[hc] *~ clt.c *-rcfile.c \
		         btxindex btxlook
		  rcsclean

depends		: $(wildcard *.c)
		  $(CXX) -MM $^ $(idirs) | \
		  sed 's;$(HOME);$$(HOME);g' > dependencies

include		dependencies

# $Log: makefile,v $
# Revision 1.1.1.1  2006-01-08 19:56:46  rclayton
# Created
#
# Revision 1.2  2005/09/08 01:13:22  rclayton
# .emacs
#
# Revision 1.1  2005/05/12 08:26:01  rclayton
# Created.
#
# Revision 1.2  2004/11/30 02:37:27  rclayton
# Replace code and data install with cp.
#
# Revision 1.1.1.1  2004/11/14 03:45:07  rclayton
# Created.
#
# Revision 1.5  2004/11/11 13:51:32  rclayton
# Add catenate-sblock.o
#
# Revision 1.4  2004/11/07 13:25:20  rclayton
# Use install -d to create install directories.
#
# Revision 1.3  2004/11/07 13:10:42  rclayton
# Define and use cmn.
#
# Revision 1.2  2004/11/06 16:22:41  rclayton
# Use the dependencies include; use $(idirs) in the flex and yacc compiles.
#
# Revision 1.1.1.1  2004/11/06 02:47:11  rclayton
# Created.
#
# Revision 1.1.1.1  2004/11/05 23:22:41  rclayton
# Created.
#
# Revision 1.1  2004/11/05 23:18:41  rclayton
# Initial revision
#
