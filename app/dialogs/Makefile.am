## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Gimp-Dialogs\"			\
	-DDESKTOP_DATADIR=\""$(DESKTOP_DATADIR)"\"	\
	-I$(top_builddir)				\
	-I$(top_srcdir)					\
	-I$(top_builddir)/app				\
	-I$(top_srcdir)/app				\
	$(APPSTREAM_GLIB_CFLAGS)			\
	$(GEGL_CFLAGS)					\
	$(GTK_CFLAGS)					\
	-I$(includedir)

noinst_LIBRARIES = libappdialogs.a

libappdialogs_a_sources = \
	dialogs-types.h			\
	dialogs.c			\
	dialogs.h			\
	dialogs-constructors.c		\
	dialogs-constructors.h		\
	\
	about-dialog.c			\
	about-dialog.h			\
	action-search-dialog.c		\
	action-search-dialog.h		\
	channel-options-dialog.c	\
	channel-options-dialog.h	\
	color-profile-dialog.c		\
	color-profile-dialog.h		\
	color-profile-import-dialog.c	\
	color-profile-import-dialog.h	\
	convert-indexed-dialog.c	\
	convert-indexed-dialog.h	\
	convert-precision-dialog.c	\
	convert-precision-dialog.h	\
	data-delete-dialog.c		\
	data-delete-dialog.h		\
	extensions-dialog.c		\
	extensions-dialog.h		\
	file-open-dialog.c		\
	file-open-dialog.h		\
	file-open-location-dialog.c	\
	file-open-location-dialog.h	\
	file-save-dialog.c		\
	file-save-dialog.h		\
	fill-dialog.c			\
	fill-dialog.h			\
	grid-dialog.h                   \
	grid-dialog.c                   \
	image-merge-layers-dialog.c	\
	image-merge-layers-dialog.h	\
	image-new-dialog.c		\
	image-new-dialog.h		\
	image-properties-dialog.c	\
	image-properties-dialog.h	\
	image-scale-dialog.c		\
	image-scale-dialog.h		\
	input-devices-dialog.c		\
	input-devices-dialog.h		\
	item-options-dialog.c		\
	item-options-dialog.h		\
	keyboard-shortcuts-dialog.c	\
	keyboard-shortcuts-dialog.h	\
	layer-add-mask-dialog.c		\
	layer-add-mask-dialog.h		\
	layer-options-dialog.c		\
	layer-options-dialog.h		\
	lebl-dialog.c			\
	lebl-dialog.h			\
	metadata-rotation-import-dialog.c \
	metadata-rotation-import-dialog.h \
	module-dialog.c			\
	module-dialog.h			\
	palette-import-dialog.c		\
	palette-import-dialog.h		\
	preferences-dialog.c		\
	preferences-dialog.h		\
	preferences-dialog-utils.c	\
	preferences-dialog-utils.h	\
	print-size-dialog.c		\
	print-size-dialog.h		\
	quit-dialog.c			\
	quit-dialog.h			\
	resize-dialog.c			\
	resize-dialog.h			\
	resolution-calibrate-dialog.c	\
	resolution-calibrate-dialog.h	\
	scale-dialog.c			\
	scale-dialog.h			\
	stroke-dialog.c			\
	stroke-dialog.h			\
	template-options-dialog.c	\
	template-options-dialog.h	\
	tips-dialog.c			\
	tips-dialog.h			\
	tips-parser.c			\
	tips-parser.h			\
	user-install-dialog.c		\
	user-install-dialog.h		\
	vectors-export-dialog.c		\
	vectors-export-dialog.h		\
	vectors-import-dialog.c		\
	vectors-import-dialog.h		\
	vectors-options-dialog.c	\
	vectors-options-dialog.h	\
	welcome-dialog.c		\
	welcome-dialog.h		\
	welcome-dialog-data.c		\
	welcome-dialog-data.h

libappdialogs_a_built_sources = \
	authors.h

libappdialogs_a_SOURCES = \
	$(libappdialogs_a_built_sources) $(libappdialogs_a_sources)

EXTRA_DIST = \
	authors.xsl


$(srcdir)/about-dialog.c: authors.h

authors.h: $(top_srcdir)/authors.xml $(srcdir)/authors.xsl
if HAVE_XSLTPROC
	$(XSLTPROC) $(srcdir)/authors.xsl $< > $(@) || rm -f $(@)
else
	@echo "*** xsltproc is required to regenerate $(@) ***"; exit 1;
endif

$(srcdir)/welcome-dialog.c: welcome-dialog-data.h welcome-dialog-data.c

welcome-dialog-data.h: $(top_srcdir)/desktop/org.gimp.GIMP.appdata.xml.in.in $(top_srcdir)/tools/generate-welcome-dialog-data.py
	$(top_srcdir)/tools/generate-welcome-dialog-data.py --header $(GIMP_VERSION) > $@

welcome-dialog-data.c: $(top_srcdir)/desktop/org.gimp.GIMP.appdata.xml.in.in $(top_srcdir)/tools/generate-welcome-dialog-data.py
	$(top_srcdir)/tools/generate-welcome-dialog-data.py $(GIMP_VERSION) > $@
