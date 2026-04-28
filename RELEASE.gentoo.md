# Making a Release - Testing / Creating a Gentoo Ebuild

This file applies if you are the maintainer of the Gentoo ebuild. [Here](https://github.com/gentoo/gentoo/blob/master/gnome-extra/gnome-commander/metadata.xml) you can see who that is currently.

## Steps

1. Create a local overlay via layman:
   https://wiki.gentoo.org/wiki/Ebuild_repository#Layman

2. After successfully running:
   ```bash
   meson dist -C builddir
   ```
   move the ebuild file from:
   ```
   builddir/data/
   ```
   to:
   ```
   /usr/local/portage/gnome-extra/gnome-commander/
   ```

3. Go to the target directory:
   ```bash
   cd /usr/local/portage/gnome-extra/gnome-commander
   ```

4. Check the ebuild and create the manifest:
   ```bash
   pkgdev manifest
   ```

5. Run:
   ```bash
   eix-update
   ```
   if needed.

6. Install the package:
   ```bash
   emerge -av gnome-commander
   ```
   If possible verify correct handling of USE flags.

7. Review the Gentoo Git pull request policy:
   https://www.gentoo.org/glep/glep-0066.html

8. Run pkgcheck to validate the ebuild (inside the directory containing the Manifest):
   ```bash
   pkgcheck scan gnome-commander-1.16.0.ebuild
   ```

9. Create a fork of the Gentoo repository:
   https://github.com/gentoo/gentoo

   Then:
   - Clone your fork
   - Add the new `Manifest` line and add the ebuild file (from step 2 in `/usr/local/portage/gnome-extra/gnome-commander/`) to the following folder in the forked repo:
     ```
     gnome-extra/gnome-commander/
     ```
   - Create a signed commit:
     ```bash
     git add -p
     pkgdev commit
     ```

10. Push your commit to your GitHub repository and open a merge request to the Gentoo repository.
