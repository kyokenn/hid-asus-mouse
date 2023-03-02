%define debug_package %{nil}

Name: hid-asusmouse-kmod-common

Version:        0.2.0
Release:        1%{?dist}.1
Summary:        ASUS mouse HID kernel module

Group:          System Environment/Kernel

License:        GPL-2.0
URL:            https://github.com/kyokenn/hid-asus-mouse
Source0:        hid-asusmouse_%{version}.orig.tar.xz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires:       kmod-hid-asusmouse


%description
Kernel module for ASUS mice


%prep
%autosetup -n hid-asusmouse_%{version}


%setup -q -c -T -a 0


%build


%install


%clean
rm -rf ${RPM_BUILD_ROOT}


%pre


%files
