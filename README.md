# Gnome Commander #

[![GitHub license](https://img.shields.io/badge/license-GPLv3-blue.svg)](https://gitlab.gnome.org/GNOME/gnome-commander/-/raw/main/COPYING)

## Introduction ##

Gnome Commander is a fast and powerful twin-panel file manager for the Linux desktop.

![ScreenShot](https://gitlab.gnome.org/GNOME/gnome-commander/-/raw/main/doc/C/figures/gnome-commander_window.png)

Gnome Commander is released under the GNU General Public License (GPL) version 3,
see the file [COPYING](COPYING) for more information.

Check the list of [releases](https://gitlab.gnome.org/GNOME/gnome-commander/-/releases) to see what has changed in each version.

### Scripts for the file popup menu

Under this [link](https://gitlab.gnome.org/GNOME/gnome-commander/tree/main/gcmd-scripts) some sample scripts can be found.
Move them into `~/.gnome-commander/scripts/` to extend the file popup menu.


## Contributing ##

### Ideas ###

If you have some good ideas for stuff you want to see in this program you
should check the list of open [issues marked as feature](https://gitlab.gnome.org/GNOME/gnome-commander/-/issues?label_name%5B%5D=1.%20Feature) before filing a new feature request.

### Translations ###

If you are interested in improving the translation, check this [link](https://welcome.gnome.org/team/translation/).

## Building ##

Get the latest source from the [GNOME server](https://download.gnome.org/sources/gnome-commander/).

```bash
~ » tar -xf gnome-commander-$VERSION.tar.xz # unpack the sources
~ » cd gnome-commander-$VERSION             # change to the toplevel directory
~ » meson setup builddir                    # setup the output directory for building the sources through meson
~ » meson compile -C builddir               # compile Gnome Commander into builddir directory
~ » meson install -C builddir               # install Gnome Commander in the system
```

For installing Gnome Commander using the sources in the git repository, do the following:

```bash
~ » git clone git@gitlab.gnome.org:GNOME/gnome-commander.git
~ » cd gnome-commander
```

and execute the meson commands from the section above. See the file [INSTALL](INSTALL)
for detailed information regarding the installation of Gnome Commander.

### Branches and Versions

Gnome Commander is being developed in several branches:

 - The main branch, where all the normal development takes place. It should always contain a runable version of GCMD.
 - One or more stable branches, named after the current stable release (e.g. gcmd-1-18), which only includes bugfixes, doc updates, translation updates, but no new features;
 - Optionally, some feature branches, where new ideas or big features are cooked until they are ready to be merged in the main branch.

The version numbers (major, minor and micro) follow the standard of odd and even versioning. Even numbers are stable versions, that are intended for all-day use. Odd versions are development versions. The version number is stored as git-tags in the git repository. The current one is stored at the top of [meson.build](meson.build).

### Problem reporting ###

Bugs should be reported on [GNOME GitLab](https://gitlab.gnome.org/GNOME/gnome-commander/issues).

First, have a look at the list of known bugs on GitLab before opening a new issue there.

In the bug report please include:

* Information about your system and anything else you think is relevant.
For instance:
  * What operating system and version
  * What desktop environment
  * What version of the gtk+, glib and Rust libraries
* How to reproduce the bug.
* If the bug was a crash, the exact text that was printed out when the
  crash occurred.
* Further information such as stack traces may be useful, but is not
  necessary. If you do send a stack trace, and the error is an X error,
  it will be more useful if the stack trace is produced running the test
  program with the --sync command line option.

### Tip for working with git ###

There exists a git-scripts directory with a [pre-commit](pre-commit)
and a [pre-push](pre-push) hook. Just type ``ln -s ../../pre-commit
.git/hooks/pre-commit`` and vice verso for the pre-push hook to
activate each hook.

Both scripts run simple checks before actually committing or pushing
your source code changes.
