Summary:	Interactive graphical LDAP browser
Summary(pl.UTF-8):	Klient i przeglądarka LDAP
Summary(pt_BR.UTF-8):	Navegador gráfico para LDAP
Name:		gq
Version:	1.2.1
Release:	0.1
License:	GPL
Group:		Networking/Utilities
Source0:	http://dl.sourceforge.net/gqclient/%{name}-%{version}.tar.gz
# Source0-md5:	e56613c81e70727c20ffe9974cdc6df0
Source1:	http://dl.sourceforge.net/gqclient/%{name}-%{version}-langpack-1.tar.gz
# Source1-md5:	9429d161c91e12d000c8b0c2f6e21d63
Source2:	%{name}.png
Patch0:		%{name}-iconv-in-libc.patch
Patch1:		%{name}-desktop.patch
URL:		http://biot.com/gq/
BuildRequires:	cyrus-sasl-devel
BuildRequires:	gettext-tools
BuildRequires:	gnome-keyring-devel >= 0.4.4
BuildRequires:	gtk+2-devel >= 2:2.6.0
BuildRequires:	libglade2-devel >= 2.0
BuildRequires:	libxml2-devel >= 2.0
BuildRequires:	openldap-devel >= 2.4.6
BuildRequires:	openssl-devel
BuildRequires:	pkgconfig
BuildRoot:	%{tmpdir}/%{name}-%{version}-root-%(id -u -n)

%description
GQ is GTK+ LDAP client and browser utility. It can be used for
searching LDAP directory as well as browsing it using tree view. It
has limited modify/add functionality, too.

%description -l pl.UTF-8
GQ jest napisanym przy użyciu GTK+ klientem oraz przeglądarką LDAP.
Można go użyć do przeszukiwania katalogu LDAP oraz przeglądania go w
formie drzewa. Posiada również (w ograniczonym stopniu) możliwość
dodawania i modyfikacji danych.

%description -l pt_BR.UTF-8
GQ é um client LDAP feito em GTK+. Ele pode ser usado para pesquisar
diretórios LDAP e também para visualizar um diretório em forma de
árvore. Também existem recursos de edição e inserção de registros,
embora um pouco limitados.

%prep
%setup -q -a1
cp %{name}-%{version}-langpack-1/po/* po
%patch0 -p1
%patch1 -p1

%build
%configure \
	--enable-browser-dnd \
	--disable-update-mimedb
%{__make}

%install
rm -rf $RPM_BUILD_ROOT

%{__make} install \
	DESTDIR=$RPM_BUILD_ROOT

install %{SOURCE2} $RPM_BUILD_ROOT%{_pixmapsdir}

%find_lang %{name}

%clean
rm -rf $RPM_BUILD_ROOT

%files -f %{name}.lang
%defattr(644,root,root,755)
%doc AUTHORS ChangeLog NEWS README TODO
%attr(755,root,root) %{_bindir}/*
%{_datadir}/gq
%{_datadir}/mime/packages/gq-ldif.xml
%{_pixmapsdir}/gq
%{_pixmapsdir}/gq.png
%{_desktopdir}/*.desktop
