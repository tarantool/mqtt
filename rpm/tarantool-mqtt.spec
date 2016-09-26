Name: tarantool-mqtt
Version: 1.0.0
Release: 1%{?dist}
Summary: Tarantool mqtting module
Group: Applications/Databases
License: BSD
URL: https://github.com/tarantool/mqtt
Source0: https://github.com/tarantool/mqtt/archive/%{version}/mqtt-%{version}.tar.gz
BuildRequires: tarantool >= 1.6.8.0
BuildRequires: tarantool-connpool >= 1.1.0
Requires: tarantool >= 1.6.8.0
BuildRequires: tarantool-devel >= 1.6.8.0
Requires: mosquitto >= 1.4.8
BuildRequires: mosquitto-devel >= 1.4.8
# For tests
%if (0%{?fedora} >= 22)
BuildRequires: python >= 2.7
BuildRequires: python-six >= 1.9.0
BuildRequires: python-gevent >= 1.0
BuildRequires: python-yaml >= 3.0.9
# Temporary for old test-run
# https://github.com/tarantool/mqtt/issues/1
BuildRequires: python-daemon
%endif

%description
Tarantool bindings to mqtt library

%prep
%setup -q -n mqtt-%{version}

%build
%cmake . -DCMAKE_BUILD_TYPE=RelWithDebInfo
make %{?_smp_mflags}

%install
%make_install

%files
%{_libdir}/tarantool/*/
%{_datarootdir}/tarantool/*/
%doc README.md
%{!?_licensedir:%global license %doc}
%license LICENSE

%changelog
* Mon Sep 26 2016 Konstantin Nazarov <racktear@tarantool.org> 1.0.0-1
- Initial version of the RPM spec
