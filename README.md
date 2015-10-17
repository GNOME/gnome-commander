# GNOME Commander #

## Introduction ##

GNOME Commander is a fast and powerful twin-panel file manager for the GNOME desktop, 
released under [GPL v2](https://www.gnu.org/licenses/gpl-2.0.html).

* Bugs: http://bugzilla.gnome.org/browse.cgi?product=gnome-commander
* Download: http://ftp.gnome.org/pub/GNOME/sources/gnome-commander
* Git: https://git.gnome.org/browse/gnome-commander/
* Website: https://gcmd.github.io/

## Contributing ##

### Ideas ###

If have some good ideas for stuff that you want to see in this program you
should check the [TODO](TODO) file first before filing a feature request. 

### Translations ###

This program is hosted on the GNOME git server. Therefore, it would be 
great if you could help translating using [damned lies](https://l10n.gnome.org/).

### Cool hacks ###

Send an email with the patch to the [developers mailing list](https://lists.nongnu.org/mailman/listinfo/gcmd-devel).
Please use the -u flag when generating the patch as it makes the patch
more readable. Write a good explanation of what the patch does.

### Plugins ###

If you have created a new plugin let us know about it on the mailing list.


## Building ##

```bash
~ » tar -xf gnome-commander-$VERSION.tar.xz    # unpack the sources
~ » cd gnome-commander-$VERSION                # change to the toplevel directory
~ » ./configure                                # run the `configure' script
~ » make                                       # build GNOME Commander
  [ Become root if necessary ]
~ » make install                               # install GNOME Commander
```

For installing GNOME Commander via git, do the following:

```bash
~ » git clone git://git.gnome.org/gnome-commander
~ » cd gnome-commander
~ » ./autogen.sh
~ » make
  [ Become root if necessary ]
~ » make install
```

After executing ``./configure`` or ``./autogen.sh`` see the file ``INSTALL``
for detailed information regarding the installation of GNOME Commander.

## Communication ##

This project has two mailing lists, one for users and one for developers.
Subscription to those mailing lists can be done at:

* https://lists.nongnu.org/mailman/listinfo/gcmd-users -> for users
* https://lists.nongnu.org/mailman/listinfo/gcmd-devel -> for developers

You can also find email addresses of the people who have created gnome-commander
in the [AUTHORS](AUTHORS) file.


## Problem reporting ##

Bugs should be reported to the [GNOME bugzilla](http://bugzilla.gnome.org/browse.cgi?product=gnome-commander).
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

See the project page [on Bugzilla](http://bugzilla.gnome.org/browse.cgi?product=gnome-commander) for the list of known bugs.
