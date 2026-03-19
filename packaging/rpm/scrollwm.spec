Name:           scrollwm
Version:        0.1.0
Release:        1%{?dist}
Summary:        Experimental scrollable tiling window manager for X11

License:        MIT
URL:            https://example.com/scrollwm
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc-c++, meson, ninja-build, pkgconfig(xcb), pkgconfig(xcb-util), pkgconfig(xcb-keysyms), pkgconfig(xcb-icccm)

%description
ScrollWM is a keyboard-first tiling window manager with a scrollable layout,
workspace support, configurable keybindings, and display manager integration.

%prep
%autosetup

%build
%meson
%meson_build

%install
%meson_install

%files
%license LICENSE
%doc README.md docs/milestones.md config/config.toml.example
%{_bindir}/scrollwm
%{_bindir}/scrollwm-session
%{_datadir}/xsessions/scrollwm.desktop
%{_mandir}/man1/scrollwm.1*
%{_mandir}/man1/scrollwm-session.1*

%changelog
* Wed Mar 18 2026 ScrollWM Maintainers <maintainers@example.com> - 0.1.0-1
- Initial RPM scaffolding.
