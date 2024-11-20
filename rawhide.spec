%define dist alt

Name:           rawhide
Version:        3.3
Release:        %{?dist}1
Summary:        find files using pretty C expressions
License:        GPL-3.0-or-later
URL:            https://raf.org/rawhide
Group:          File tools
Source:         https://github.com/raforg/rawhide/releases/download/v%{version}/%{name}-%{version}.tar.gz
BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  libe2fs-devel
BuildRequires:  libpcre2-devel
BuildRequires:  libacl-devel
BuildRequires:  libmagic-devel
BuildRequires:  libattr-devel
Requires:       libe2fs >= 1.46
Requires:       libpcre2 >= 10.43
Requires:       libacl >= 2.3
Requires:       libmagic >= 5.45
Requires:       libattr >= 2.5

%description
Rawhide (rh) lets you search for files on the command line using expressions and user-defined functions in a mini-language inspired by C. It's like find(1), but more fun to use.

%prep
%setup -q
%autopatch -p1

%build
./configure
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
%make_install DESTDIR=%buildroot PREFIX=%_prefix MANDIR=%_mandir INFODIR=%_infodir install

%files
%doc README.md COPYING CHANGELOG LICENSE
%{_bindir}/rh
%{_mandir}/man1/rh.1*
%{_mandir}/man5/rawhide.conf.5*
%config(noreplace) %_sysconfdir/rawhide.conf
%config(noreplace) %_sysconfdir/rawhide.conf.d/attributes

%changelog
