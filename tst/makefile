rm	= rm -f
tcmds   = echo north holland ; echo lam begeman ; echo conklin begeman ; \
	  echo und
tfile   = tst.out

dir	= ../src

test	: $(tfile)
	  cp tst.bib /tmp
	  $(dir)/btxindex -s/tmp -w. tst
	  ($(tcmds)) | $(dir)/btxlook -d cat -s. tst > out
	  cmp -s $(tfile) out || echo 1>&2 'test failed.'
	  $(rm) /tmp/tst.bib out tst.bix

make	: tst.bib
	  cp tst.bib /tmp
	  $(dir)/btxindex -s/tmp -w. tst
	  ($(tcmds)) | $(dir)/btxlook -d cat -s. tst > $(tfile)
	  chmod a-w $(tfile)
	  $(rm) /tmp/tst.bib
