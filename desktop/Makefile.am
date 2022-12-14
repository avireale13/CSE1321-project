## Makefile.am for gimp/desktop

desktop_in_files = \
	gimp.desktop.in.in

desktop_files = $(desktop_in_files:.desktop.in.in=.desktop)

appstream_in_files = \
	org.gimp.GIMP.appdata.xml.in

appstream_files = $(appstream_in_files:.xml.in=.xml)

$(appstream_files): $(appstream_in_files)
	$(AM_V_GEN) $(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

if DESKTOP_DATADIR

applicationsdir = $(DESKTOP_DATADIR)/applications
applications_DATA = $(desktop_files)

appdatadir = $(DESKTOP_DATADIR)/metainfo
appdata_DATA = $(appstream_files)

icons16dir = $(DESKTOP_DATADIR)/icons/hicolor/16x16/apps
icons16_DATA = 16x16/gimp.png

icons22dir = $(DESKTOP_DATADIR)/icons/hicolor/22x22/apps
icons22_DATA = 22x22/gimp.png

icons24dir = $(DESKTOP_DATADIR)/icons/hicolor/24x24/apps
icons24_DATA = 24x24/gimp.png

icons32dir = $(DESKTOP_DATADIR)/icons/hicolor/32x32/apps
icons32_DATA = 32x32/gimp.png

icons48dir = $(DESKTOP_DATADIR)/icons/hicolor/48x48/apps
icons48_DATA = 48x48/gimp.png

icons64dir = $(DESKTOP_DATADIR)/icons/hicolor/64x64/apps
icons64_DATA = 64x64/gimp.png

icons256dir = $(DESKTOP_DATADIR)/icons/hicolor/256x256/apps
icons256_DATA = 256x256/gimp.png

iconsscalabledir = $(DESKTOP_DATADIR)/icons/hicolor/scalable/apps
iconsscalable_DATA = scalable/gimp.svg


# touch the toplevel icon directory as required by the icon theme spec
install-data-hook:
	touch $(DESTDIR)$(DESKTOP_DATADIR)/icons/hicolor

endif

EXTRA_DIST = \
	$(appstream_in_files) \
	test-appdata.sh.in    \
	test-desktop.sh       \
	gimp.desktop.in.in    \
	16x16/gimp.png        \
	22x22/gimp.png        \
	24x24/gimp.png        \
	32x32/gimp.png        \
	48x48/gimp.png        \
	64x64/gimp.png        \
	256x256/gimp.png      \
	scalable/gimp.svg


DISTCLEANFILES = $(desktop_files) $(appstream_files)

gimp.desktop: gimp.desktop.in $(wildcard $(top_srcdir)/po/*.po)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

validate: gimp.desktop
	( dfvalidate=`which desktop-file-validate`; \
	  if test x$$dfvalidate != x && test -x $$dfvalidate; then \
            $$dfvalidate $< || ( echo "* $< INVALID *"; exit 1 ) \
	  else \
	    echo "Can't find desktop-file-validate to validate $<; proceed with fingers crossed..."; \
          fi )

dist-hook: validate

TESTS_ENVIRONMENT = \
	chmod +x test-appdata.sh; \
	GIMP_TESTING_ABS_TOP_BUILDDIR=@abs_top_builddir@
TESTS =

if HAVE_APPSTREAMCLI
TESTS += test-appdata.sh
endif

if HAVE_DESKTOP_FILE_VALIDATE
TESTS += test-desktop.sh
endif
