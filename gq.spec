# -*- mode: rpm-spec -*-
#
# $Id: gq.spec.in 978 2006-09-14 12:05:54Z herzi $
#

%define name     gq
%define version  1.2.3
%define release  0

%define prefix /usr

# define langpack as "no" if you do not want to use a langpack
# you can do this either by uncommenting the following line and removing
# the first underscore within it:
#
#  %_define langpack no
# 
# or on the commandline of rpm using 
#
#  rpm -ba --define "langpack no" gq.spec
#
# The same works for tarball building
#
# If you want to use a specific langpack version you may define the
# langpack macro to another version in a way similar to as shown for "no"
#
# If you want to try a langpack from a different version of gq, you
# might to the same for the setting of the "langpackversion" macro.

%define _langpack_        %{!?langpack:1}%{?langpack:%{langpack}}
%define _langpackversion_ %{!?langpackversion:1.2.3}%{?langpackversion:%{langpackversion}}

Name:		%{name}
Summary:	Interactive graphical LDAP browser
Version:	%{version}
Release:	%{release}
Copyright:	GPL with OpenSSL excemption
Group:		Networking/Utilities
URL:		http://biot.com/gq/
Packager:	Peter Stamfest <peter@stamfest.at>
Source:		http://prdownloads.sourceforge.net/gqclient/gq-%{version}.tar.gz

%if "%{_langpack_}" != "no"
Source1:	http://prdownloads.sourceforge.net/gqclient/gq-%{_langpackversion_}-langpack-%{_langpack_}.tar.gz
%endif

BuildRoot:	%{_tmppath}/%{name}-%{version}-root
Requires:	gtk+ >= 1.2.0

%description
GQ is a GTK+ LDAP client and browser utility. It can be used
for searching a LDAP directory as well as browsing it using a
tree view. Furthermore, it lets you inspect the LDAP schema a 
server is using. 

Install gq if you need a graphical tool to manage the contents
of a LDAP server.

%prep
%setup -q

%if "%{_langpack_}" != "no"
%setup -T -D -a 1
    ./gq-%{_langpackversion_}-langpack-%{_langpack_}/langpack .
    %{__cp} ./gq-%{_langpackversion_}-langpack-%{_langpack_}/po/LINGUAS po/
%endif

%build
%{configure} --with-included-gettext #   --enable-cache --enable-browser-dnd
%{__make}

%install
[ "%{buildroot}" != "/" ] && %{__rm} -rf %{buildroot}
%{makeinstall}


%if "%{_langpack_}" == "no"

cat >&2 <<EOF
No language pack selected for building - removing locale directory to
not confuse RPM and the find-lang script.
EOF

# arghhh, this fixes a problem with newer RPMs that complain when the
# directory exists, but does not contain any message catalogs. Reported
# by Simon Matter

if [ "%{buildroot}" != "/" ]; then
    %{__rm} -rf  %{buildroot}%{_datadir}/locale
fi

%else

# this file belongs to glibc-common, so we don't package it

    %{__rm} -f %{buildroot}%{_datadir}/locale/locale.alias

%endif

%clean
[ "%{buildroot}" != "/" ] && %{__rm} -rf %{buildroot}

%files
%defattr(-, root, root)
%{_bindir}/*
%{_datadir}/applications/%{name}.desktop
%dir %{_datadir}/pixmaps/%{name}
%{_datadir}/pixmaps/%{name}/*

%if "%{_langpack_}" != "no"
%{_datadir}/locale/*/LC_MESSAGES/*.mo
%endif


%doc README
%doc INSTALL
%doc COPYING
%doc ChangeLog
%doc NEWS
%doc TODO
%doc AUTHORS
%doc ABOUT-NLS
%doc README.TLS
%doc README.NLS


%changelog
* Wed Oct 22 2003 Simon Matter <simon.matter@invoca.ch>
- replaced several values by corresponding RPM macros

* Thu Apr 25 2002 Peter Stamfest <peter@stamfest.at>
- Updated for I18N, added new enable arguments as a reminder

* Mon Sep 25 2000 Bert Vermeulen <bert@biot.com>
- changed RPM spec maintainer

* Wed Mar 08 2000 Ross Golder <rossigee@bigfoot.com>
- Integrated spec file into source tree
- Added GNOME panel menu entry

* Fri May 28 1999 Borek Lupomesky <Borek.Lupomesky@ujep.cz>
- Update to 0.2.2

* Sat May 22 1999 Borek Lupomesky <Borek.Lupomesky@ujep.cz>
- Modified spec to use buildroot
