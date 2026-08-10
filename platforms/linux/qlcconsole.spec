%define version %(echo $QLCPLUS_VERSION)
#define ui qmlui

Summary: qlcconsole - a fork of Q Light Controller Plus - The free DMX lighting console
License: Apache License, Version 2.0
Name: qlcconsole
Version: %{version}
BuildRequires:  desktop-file-utils
BuildRequires:  fdupes
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig
BuildRequires:  pkgconfig(Qt5Multimedia)
BuildRequires:  pkgconfig(Qt5Script)
BuildRequires:  pkgconfig(Qt5Widgets)
BuildRequires:  pkgconfig(Qt5SerialPort)
BuildRequires:  pkgconfig(alsa)
BuildRequires:  pkgconfig(fftw3)
BuildRequires:  pkgconfig(libftdi1)
BuildRequires:  pkgconfig(libola)
BuildRequires:  pkgconfig(libudev)
#BuildRequires:  pkgconfig(mad)
BuildRequires:  pkgconfig(sndfile)
%if %{defined fedora}
BuildRequires:  pkgconfig(libusb-1.0)
BuildRequires:  qt5-linguist
BuildRequires:  qt5-qtconfiguration-devel
%if "%{ui}" == "qmlui"
BuildRequires:  qt5-qt3d-devel
BuildRequires:  qt5-qtsvg-devel
%endif
%else
BuildRequires:  pkgconfig(libusb1)
BuildRequires:  libqt5-linguist-devel
BuildRequires:  update-desktop-files
%endif
Release: 1
Source: qlcconsole-%{version}.tar.gz
URL: https://www.qlcplus.org/

%description
qlcconsole is a fork of the great QLC project written
by Heikki Junnila. This project aims to continue
the development of QLC and to introduce new features.
The primary goal is to bring QLC+ at the level
of other lighting control commercial softwares.

#############################################################################
# Preparation
#############################################################################

%prep
%setup -q

sed -ie '/UDEVRULESDIR/s|/etc/udev/rules.d|/usr/lib/udev/rules.d|' variables.pri

#############################################################################
# Build
#############################################################################

%build
# qmake-qt5 will only include existing files in install_translations - create the .qm files first

%if "%{ui}" == "qmlui"
    ./translate.sh release qmlui
    qmake-qt5 CONFIG+=qmlui
%else
    ./translate.sh release ui
    qmake-qt5
%endif
make %{?_smp_mflags}

#############################################################################
# Install
#############################################################################

%install
INSTALL_ROOT=$RPM_BUILD_ROOT make install
%if "%{ui}" == "qmlui"
mv %{buildroot}/%{_bindir}/qlcconsole-qml %{buildroot}/%{_bindir}/qlcconsole
sed -i -e 's/Exec=qlcconsole --open %f/Exec=qlcconsole/g' %{buildroot}/%{_datadir}/applications/qlcconsole.desktop
%endif

desktop-file-validate %{buildroot}/%{_datadir}/applications/*.desktop

#############################################################################
# Post
#############################################################################

%if %{defined suse_version}
%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig
%endif

#############################################################################
# Files
#############################################################################

%files
%{_bindir}/*
%{_libdir}/libqlcplusengine.so*
%if "%{ui}" != "qmlui"
%{_libdir}/libqlcplusui.so*
%{_libdir}/libqlcpluswebaccess.so*
%endif
%dir %{_datadir}/qlcconsole
%{_datadir}/applications/*
%{_datadir}/metainfo/*
%{_datadir}/mime/packages/qlcconsole.xml
%{_datadir}/pixmaps/*
%{_datadir}/qlcconsole/Sample.qxw
%{_datadir}/qlcconsole/fixtures
%{_datadir}/qlcconsole/gobos
%{_datadir}/qlcconsole/inputprofiles
%{_datadir}/qlcconsole/miditemplates
%{_datadir}/qlcconsole/modifierstemplates
%{_datadir}/qlcconsole/rgbscripts
%{_datadir}/qlcconsole/translations
%if "%{ui}" == "qmlui"
%{_datadir}/qlcconsole/colorfilters
%{_datadir}/qlcconsole/meshes
%else
%{_datadir}/qlcconsole/web
%endif
#%_libdir/qt5/plugins/qlcconsole/audio/libmadplugin.so
%_libdir/qt5/plugins/qlcconsole/audio/libsndfileplugin.so
%_libdir/qt5/plugins/qlcconsole/libartnet.so
%_libdir/qt5/plugins/qlcconsole/libdmx4linux.so
%_libdir/qt5/plugins/qlcconsole/libdmxusb.so
%_libdir/qt5/plugins/qlcconsole/libe131.so
%_libdir/qt5/plugins/qlcconsole/libenttecwing.so
%_libdir/qt5/plugins/qlcconsole/libhidplugin.so
%_libdir/qt5/plugins/qlcconsole/libloopback.so
%_libdir/qt5/plugins/qlcconsole/libmidiplugin.so
%_libdir/qt5/plugins/qlcconsole/libos2l.so
%_libdir/qt5/plugins/qlcconsole/libosc.so
%_libdir/qt5/plugins/qlcconsole/libpeperoni.so
%_libdir/qt5/plugins/qlcconsole/libspi.so
%_libdir/qt5/plugins/qlcconsole/libudmx.so
%if "%{ui}" != "qmlui"
%_mandir/*/*
%doc /usr/share/qlcconsole/documents
%endif
/usr/lib/udev/rules.d/z65-anyma-udmx.rules
/usr/lib/udev/rules.d/z65-dmxusb.rules
/usr/lib/udev/rules.d/z65-fx5-hid.rules
/usr/lib/udev/rules.d/z65-peperoni.rules
/usr/lib/udev/rules.d/z65-spi.rules
