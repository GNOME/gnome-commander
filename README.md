# Gnome Commander #

[![GitHub license](https://img.shields.io/badge/license-GPLv2-blue.svg)](https://raw.githubusercontent.com/GNOME/gnome-commander/master/COPYING)

## Introduction ##

Gnome Commander is a fast and powerful twin-panel file manager for the Linux desktop.

![ScreenShot](https://gitlab.gnome.org/GNOME/gnome-commander/-/raw/master/doc/C/figures/gnome-commander_window.png)

Gnome Commander is released under the GNU General Public License (GPL) version 2,
see the file ``COPYING`` for more information.

The online available Git log contains a detailed description on what has changed
in each version. For program users the AppData file might be a better place to
look since it contains change summaries between the different versions.

Generate a human readable version of the appdata file with the following command:
  `appstream-util appdata-to-news data/org.gnome.gnome-commander.appdata.xml`

* Website with more information: https://gcmd.github.io/

### Mailing lists ###

* https://lists.nongnu.org/mailman/listinfo/gcmd-users → for users
* https://lists.nongnu.org/mailman/listinfo/gcmd-devel → for developers

### Distribution packages ###

Gnome Commander is
[available in distributions](https://gcmd.github.io/download.html#external)
like Fedora, Gentoo, Arch, etc.

You can also download tarball releases from the [Gnome download server](https://download.gnome.org/sources/gnome-commander/).

## Contributing ##

### Ideas ###

If you have some good ideas for stuff you want to see in this program you
should check the [TODO](TODO) file first before filing a feature request.

### Translations ###

It would be great if you could help translating using [damned lies](https://l10n.gnome.org/).

### Cool hacks ###

Have a look in the [Gnome Wiki](https://wiki.gnome.org/GitLab#GitLab_workflow_for_code_contribution) how to contribute with new lines of code.

Don't forget to write a good explanation of what your patch does.

## Building ##

Get the latest source from the [Gnome server](https://download.gnome.org/sources/gnome-commander/).

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

### Problem reporting ###

Bugs should be reported on [Gnome GitLab](https://gitlab.gnome.org/GNOME/gnome-commander/issues).
You will need to create an account for yourself.

In the bug report please include:

* Information about your system and anything else you think is relevant.
For instance:
  * What operating system and version
  * What desktop environment
  * What version of the gtk+, glib and gnome libraries
* How to reproduce the bug.
* If the bug was a crash, the exact text that was printed out when the
  crash occurred.
* Further information such as stack traces may be useful, but is not
  necessary. If you do send a stack trace, and the error is an X error,
  it will be more useful if the stack trace is produced running the test
  program with the --sync command line option.

Also, have a look at the list of known bugs on GitLab before opening a new bug.

### Tip for working with git ###

There exists a git-scripts directory with a [pre-commit](pre-commit)
and a [pre-push](pre-push) hook. Just type ``ln -s ../../pre-commit
.git/hooks/pre-commit`` and vice verso for the pre-push hook to
activate each hook.

Both scripts run simple checks before actually committing or pushing
your source code changes.
