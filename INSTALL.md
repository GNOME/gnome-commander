Prerequisites
=============

Gnome Commander requires the following packages:

- A working GNOME platform including glib 2.80, gtk 4.14, gio 2.0, and vte 0.76

Gnome Commander can also make use of the following packages:

- exifread for Python3 (Image Metadata plugin)
- mutagen for Python 3 (Multimedia Metadata plugin)
- pypdf for Python 3 (Document Metadata plugin)
- yelp for documentation

In addition, manually compiling Gnome Commander requires:

- A Rust toolchain (minimum required version can be found at the top of
  [Cargo.toml](Cargo.toml))
- Meson (for release builds only)
- gettext tools
- Development packages of the required libraries above. These packages are
  normally indicated by a "-dev" or "-devel" suffix, e.g. libglib2.0-dev,
  libgio-2.0-dev, etc.

Many package managers provide a way to install all the build dependencies for
a package, such as 'dnf builddep' or 'apt-get build-dep', which will install
almost all of the above. The requirements may have changed slightly between
the version packaged in the distribution and the current development version,
so you might have to install some additional packages.

Trying out Gnome Commander
==========================

If you merely want to play around with a Gnome Commander development build you
can do that with `cargo`:

```sh
cargo run
```

This will download all the required dependencies, create a build and run it.
Any changes to the repository will be picked up automatically, running `cargo`
again will recompile as necessary.

Note that release builds produced by `cargo` expect globally installed state
such as settings schemas and cannot be expected to be immediately runnable.

Compiling and installing a release build
========================================

In order to compile and install a release build you will need the `meson` tool:

```sh
meson setup builddir              # prepare the build
meson compile -C builddir         # build Gnome Commander
meson install -C builddir         # install Gnome Commander
```

The last command usually requires root privileges, the default Gnome Commander
install directory typically being `/usr/local/bin`.

You can change meson options by running `meson configure builddir`. For example,
changing install prefix to install Gnome Commander in `/usr/bin` would work as
follows:

```sh
meson configure builddir -Dprefix=/usr
```

Uninstall procedure
===================

If you want to uninstall Gnome Commander again, run as root:

```sh
ninja -C builddir uninstall
```


The Details
===========

More detailed installation instructions can be found in meson's
website: https://mesonbuild.com/Quick-guide.html
