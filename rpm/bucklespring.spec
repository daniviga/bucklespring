%global debug_package %{nil}

Name:           bucklespring
Version:        1.5.99
Release:        1%{?dist}
Summary:        Nostalgia bucklespring keyboard sound

License:        GPLv2
URL:            https://github.com/daniviga/bucklespring/
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  sed
BuildRequires:  gcc
BuildRequires:  openal-soft-devel
BuildRequires:  alure-devel
BuildRequires:  libXtst-devel

%description
Fork of https://github.com/zevv/bucklespring

Copyright 2016 Ico Doornekamp

Bucklespring runs as a background process and plays back the sound of each key pressed and released on your keyboard, just as if you were using an IBM Model-M.
The sound of each key has carefully been sampled, and is played back while simulating the proper distance and direction for a realistic 3D sound palette of pure nostalgic bliss.

To temporarily silence bucklespring, for example to enter secrets, press ScrollLock twice (but be aware that those ScrollLock events are delivered to the application); same to unmute.
The keycode for muting can be changed with the -m option. Use keycode 0 to disable the mute function.

%prep
%autosetup
sed -i 's|./wav/001|/usr/share/bucklespring/001|g' Makefile

%build
make

%install
install -m 755 -d %{buildroot}%{_bindir}
install -m 755 buckle %{buildroot}%{_bindir}/buckle
install -m 755 -d wav %{buildroot}%{_datadir}/bucklespring
cp -R wav/* %{buildroot}%{_datadir}/bucklespring


%files
%license LICENSE
%{_bindir}/buckle
%{_datadir}/bucklespring


%changelog
* Fri May 14 2021 Daniele Viganò <daniele@vigano.me> - 1.5.99
- Release 1.5.0

* Mon Nov 5 2018 Daniele Viganò <daniele@vigano.me> - 1.4.99
- First release
