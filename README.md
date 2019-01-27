# GNOME Commander #

[![GitHub license](https://img.shields.io/badge/license-GPLv2-blue.svg)](https://raw.githubusercontent.com/GNOME/gnome-commander/master/COPYING)  |  [![GitHub commits](https://img.shields.io/github/commits-since/GNOME/gnome-commander/1.8.0.svg)](https://gitlab.gnome.org/GNOME/gnome-commander/tree/master/)

## Introduction ##

GNOME Commander is a fast and powerful twin-panel file manager for the Linux desktop.

![ScreenShot](https://gcmd.github.io/ss/MainWin-Classic.png)

* Website: https://gcmd.github.io/
* Bugs: https://gitlab.gnome.org/GNOME/gnome-commander/issues

### Mailing lists ###

* https://lists.nongnu.org/mailman/listinfo/gcmd-users → for users
* https://lists.nongnu.org/mailman/listinfo/gcmd-devel → for developers

You can find email addresses of the people who have created GNOME Commander
in the [AUTHORS](AUTHORS) file.

### Distribution packages ###

GNOME Commander is 
[available in many distributions](https://gcmd.github.io/download.html#external)
like Debian, Fedora, Gentoo, Arch, etc.

## Contributing ##


### Ideas ###

If you have some good ideas for stuff you want to see in this program you
should check the [TODO](TODO) file first before filing a feature request.


### Translations ###

This program is hosted on the GNOME git server. Therefore, it would be 
great if you could help translating using [damned lies](https://l10n.gnome.org/).


### Cool hacks ###

Send an email with the patch to the [developers mailing list](https://lists.nongnu.org/mailman/listinfo/gcmd-devel).
Please create the patch either
* with the diff-command: ``diff -Naur $OLD_FILE $NEW_FILE > patch.txt``
* or with git-diff: ``git diff $COMMIT_ID1 $COMMIT_ID2 > patch.txt``

Also, write a good explanation of what the patch does.


### Plugins ###

If you have created a new plugin let us know about it on the [developer mailing list](https://lists.nongnu.org/mailman/listinfo/gcmd-devel).


### Problem reporting ###

Bugs should be reported on [GNOME GitLab](https://gitlab.gnome.org/GNOME/gnome-commander/issues).
You will need to create an account for yourself.

In the bug report please include:

* Information about your system and anything else you think is relevant.
For instance:
  * What operating system and version
  * What version of X
  * What version of the gtk+, glib and gnome libraries
  * For Linux, what version of the C library
* How to reproduce the bug.
* If the bug was a crash, the exact text that was printed out when the
  crash occurred.
* Further information such as stack traces may be useful, but is not
  necessary. If you do send a stack trace, and the error is an X error,
  it will be more useful if the stack trace is produced running the test
  program with the --sync command line option.

Also, have a look at the list of known bugs on GitLab bevore opening a new bug.


## Building ##

Get the latest source from the [GNOME ftp server](https://download.gnome.org/sources/gnome-commander/).

```bash
~ » tar -xf gnome-commander-$VERSION.tar.xz # unpack the sources
~ » cd gnome-commander-$VERSION             # change to the toplevel directory
~ » ./configure                             # run the `configure' script
~ » make                                    # build GNOME Commander
  [ Become root if necessary ]
~ » make install                            # install GNOME Commander
```

For installing GNOME Commander using the sources in the git repository, do the following:

```bash
~ » git clone git@gitlab.gnome.org:GNOME/gnome-commander.git
~ » cd gnome-commander
~ » ./autogen.sh
~ » make
  [ Become root if necessary ]
~ » make install
```

After executing ``./configure`` or ``./autogen.sh`` see the file ``INSTALL``
for detailed information regarding the installation of GNOME Commander.

### Tip for working with git ###

There exists a git-scripts directory with a [pre-commit](pre-commit)
and a [pre-push](pre-push) hook. Just type ``ln -s ../../pre-commit
.git/hooks/pre-commit`` and vice verso for the pre-push hook to
activate each hook.

Both scripts run simple checks before actually committing or pushing
your source code changes.

### Docker ###

There exists also a [Dockerfile](Dockerfile) in the repository. At the
moment, it is for testing purposes for the Ubuntu distribution only,
i.e. when you do ``docker build .``, the GCMD sources are compiled on
the Ubuntu base image. Feel free to push a merge request if you have
cool ideas using Docker together with GCMD.
