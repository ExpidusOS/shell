# ExpidusOS Shell

The default desktop environment on ExpidusOS is called ExpidusOS Shell. This repository contains the code for building and running ExpidusOS Shell. It supports X11 and will support Wayland fully in the future.

## Requirements

### Dependencies
* XFCE 4 Window Manager
* `accountsservice`
* `gio-2.0`
* `gtk+-3.0`
* `libnm` (NetworkManager library)
* `libpulse` + `libpulse-mainloop-glib`
* `libwnck-3.0`
* `libxfconf`
* UPower
* xsettingsd
* valac (host)

Make sure any dependency listed with the code blocks have their GObject Introspection and/or vapi bindings generated and installed.

### Hardware
* CPU: Pentium 4 *or better*
* RAM: 1Gb *or higher*, 2Gb **recommended**

## Building & Installing

ExpidusOS Shell installs and builds just like any other meson project, however it requires the Vala compiler. It also needs `xsettingsd` and `xfwm4` in order to start.
Optionally, it needs the Papirus icon theme and Tokyo theme when using the default settings.
The shell also requires elogind if not using systemd, this enables the shell to reboot and shutdown.
