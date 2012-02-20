
doc_DATA = xlaunch.chm

HTMLHELP_IMAGES = \
	moin-www.png \
	X2.bmp

HTMLHELP_TEXT = \
	common.css \
	screen.css \
	IDH_AUTHORS.htm \
	IDH_CLIENTS.htm \
	IDH_CLIPBOARD.htm \
	IDH_DISPLAY.htm \
	IDH_FINISH.htm \
	IDH_PROGRAM.htm \
	IDH_XDMCP.htm

xlaunch.chm_sources = \
	$(HTMLHELP_IMAGES) \
	$(HTMLHELP_TEXT) \
	xlaunch.hhp \
	map.h

# hhc:
# - can only make it's output it the same place as the input file
# - returns odd exit statuses, so ignore those and just check if it created a .chm file
# - seems to have difficulty creating it's output files in distcheck builds, so help it with that
xlaunch.chm: $(addprefix $(srcdir)/,$(xlaunch.chm_sources))
	cat /dev/null >$(srcdir)/xlaunch.chm
	cat /dev/null >$(srcdir)/xlaunch.hhc
	cd $(srcdir) && $(HHC) xlaunch.hhp || true
	test -s $(srcdir)/xlaunch.chm || ( rm $(srcdir)/xlaunch.chm && false )
	mv $(srcdir)/xlaunch.chm $(builddir)/xlaunch.chm

CLEANFILES = xlaunch.chm
EXTRA_DIST = $(xlaunch.chm_sources)
