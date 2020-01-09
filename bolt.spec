Name:          bolt
Version:       0.4
Release:       3%{?dist}
Summary:       Thunderbolt device manager
License:       LGPLv2+
URL:           https://gitlab.freedesktop.org/bolt/bolt
Source0:       %{url}/-/archive/%{version}/%{name}-%{version}.tar.bz2
Patch0:        py2-compat.patch
Patch1:        restricting-capabilities.patch

BuildRequires: gcc
BuildRequires: asciidoc
BuildRequires: meson
BuildRequires: libudev-devel
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: pkgconfig(libudev)
BuildRequires: pkgconfig(systemd)
BuildRequires: pkgconfig(libsystemd)
BuildRequires: polkit-devel
BuildRequires: systemd
%{?systemd_requires}

# for the integration test (optional)
%if 0%{?fedora}
BuildRequires: pygobject3-devel
BuildRequires: python3-dbus
BuildRequires: python3-dbusmock
BuildRequires: umockdev-devel
%endif

%description
bolt is a system daemon to manage thunderbolt 3 devices via a D-BUS
API.  Thunderbolt 3 features different security modes that require
devices to be authorized before they can be used. The D-Bus API can be
used to list devices, enroll them (authorize and store them in the
local database) and forget them again (remove previously enrolled
devices). It also emits signals if new devices are connected (or
removed). During enrollment devices can be set to be automatically
authorized as soon as they are connected.  A command line tool, called
boltctl, can be used to control the daemon and perform all the above
mentioned tasks.

%prep
%setup -q
%patch0 -p1
%patch1 -p1

%build
%meson -Ddb-path=%{_localstatedir}/lib/boltd
%meson_build

%check
%meson_test

%install
%meson_install
install -m0755 -d $RPM_BUILD_ROOT%{_localstatedir}/lib/boltd


%post
%systemd_post %{name}.service

%preun
%systemd_preun %{name}.service

%postun
%systemd_postun_with_restart %{name}.service

%files
%license COPYING
%doc README.md
%{_bindir}/boltctl
%{_libexecdir}/boltd
%{_unitdir}/%{name}.service
%{_udevrulesdir}/*-%{name}.rules
%{_sysconfdir}/dbus-1/system.d/org.freedesktop.bolt.conf
%{_datadir}/dbus-1/interfaces/org.freedesktop.bolt.xml
%{_datadir}/polkit-1/actions/org.freedesktop.bolt.policy
%{_datadir}/polkit-1/rules.d/org.freedesktop.bolt.rules
%{_datadir}/dbus-1/system-services/org.freedesktop.bolt.service
%{_mandir}/man1/boltctl.1*
%{_mandir}/man8/boltd.8*
%dir %{_localstatedir}/lib/boltd

%changelog
* Wed Jul 18 2018 Christian Kellner <ckellner@redhat.com> - 0.4-3
- Include patch to tighten sandbox by restricting capabilities
- Resolves: #1559611

* Wed Jun  6 2018 Christian Kellner <ckellner@redhat.com> - 0.4-2
- bolt 0.4 upstream release
- Resolves: #1559611
