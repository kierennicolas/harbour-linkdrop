Name:       harbour-linkdrop
Summary:    Upload a file and copy a share link
Version:    0.1.1
Release:    1
Group:      Qt/Qt
License:    MIT
URL:        https://github.com/kierennicolas/harbour-linkdrop
Source0:    %{name}-%{version}.tar.bz2

Requires:       sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Gui)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  desktop-file-utils

%description
LinkDrop shares files over HTTPS from Sailfish OS using temporary
Litterbox hosting. It does not encrypt files end to end.

%prep
%autosetup -n %{name}-%{version}

%build
%qmake5 VERSION=%{version}
%make_build

%install
%qmake5_install
desktop-file-install --delete-original --dir %{buildroot}%{_datadir}/applications %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
