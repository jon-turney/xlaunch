
bin_PROGRAMS = xlaunch

if DEBUG
DEBUG_FLAGS=-D_DEBUG
endif

AM_CXXFLAGS = $(DEBUG_FLAGS) -Wall
LDADD = -lcomctl32 $(LIBX11_LIBS)
AM_LDFLAGS = -mwindows

xlaunch_SOURCES = \
	config.cc \
	main.cc \
	window/dialog.cc \
	window/util.cc \
	window/window.cc \
	window/wizard.cc \
	resources/resources.rc

.rc.o:
	$(AM_V_GEN)$(WINDRES) --use-temp-file -i $< --input-format=rc -o $@ -O coff -I$(top_srcdir)

RESOURCES_IMAGES = \
	resources/multiwindow.bmp \
	resources/fullscreen.bmp \
	resources/windowed.bmp \
	resources/nodecoration.bmp \
	resources/X.ico

RESOURCES_RC = \
	resources/images.rc \
	resources/dialog.rc \
	resources/strings.rc

resources/resources.o: \
	resources/resources.rc \
	resources/resources.h \
	$(RESOURCES_RC) \
	$(RESOURCES_IMAGES) \
	XLaunch.exe.manifest

EXTRA_DIST = \
	COPYING \
	config.h \
	resources/resources.h \
	resources/resources.rc \
	XLaunch.exe.manifest \
	$(RESOURCES_RC) \
	$(RESOURCES_IMAGES) \
	window/dialog.h \
	window/util.h \
	window/window.h \
	window/wizard.h
