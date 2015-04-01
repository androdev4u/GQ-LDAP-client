NULL=
packagesdir=$(datadir)/mime/packages
packages_in_files=gq-ldif.xml.in
packages_DATA=$(packages_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

glade_DATA=\
	gq.glade \
	$(NULL)
gladedir=$(prefix)/share/gq

sysdir = $(datadir)/applications
sys_infiles = gq.desktop.in
sys_DATA = $(sys_infiles:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

install-data-hook:
if ENABLE_UPDATE_MIMEDB
	update-mime-database "$(datadir)/mime"
endif
	
CLEANFILES=\
	$(sys_DATA) \
	$(packages_DATA) \
	$(NULL)
EXTRA_DIST=\
	$(glade_DATA) \
	$(packages_in_files) \
	$(sys_infiles) \
	$(NULL)
