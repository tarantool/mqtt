Name: tarantool-mqtt
Version: 1.4.0
Release: 1%{?dist}
Summary: Tarantool MQTT module
Group: Applications/Databases
License: BSD
URL: https://github.com/tarantool/mqtt
Source0: https://github.com/tarantool/mqtt/archive/%{version}/mqtt-%{version}.tar.gz
BuildRequires: cmake >= 2.8
BuildRequires: gcc >= 4.5
BuildRequires: tarantool >= 1.6.8.0
BuildRequires: tarantool-devel >= 1.6.8.0
BuildRequires: mosquitto-devel >= 1.4.8
BuildRequires: mosquitto >= 1.4.8
BuildRequires: openssl-devel

Requires: mosquitto >= 1.4.8
Requires: openssl

%description
Tarantool bindings to mqtt library

%prep
%setup -q -n mqtt-%{version}

%build
%cmake . -DCMAKE_BUILD_TYPE=RelWithDebInfo
make %{?_smp_mflags}
make -j2 test

%install
%make_install

%files
%{_libdir}/tarantool/*/
%{_datarootdir}/tarantool/*/
%doc README.md
%{!?_licensedir:%global license %doc}
%license LICENSE

%changelog
* Mon Jul 9 2018 V. Soshnikov <dedok.mad@gmail.com> 1.4.0
- Fixed: https://github.com/tarantool/mqtt/issues/14

* Mon May 28 2018 V. Soshnikov <dedok.mad@gmail.com> 1.3.0
- Fixed: https://github.com/tarantool/mqtt/issues/12
- Fixed: https://github.com/tarantool/mqtt/issues/11
- Fixed: https://github.com/tarantool/mqtt/issues/9

* Sun Apr 2 2017 V. Soshnikov <dedok.mad@gmail.com> 1.1.0
- Update requires

* Mon Sep 26 2016 Konstantin Nazarov <racktear@tarantool.org> 1.0.0-1
- Initial version of the RPM spec
