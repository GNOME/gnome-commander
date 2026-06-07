# Making a Release - Testing / Creating a Gentoo Ebuild

This file applies if you are the maintainer of the Gentoo ebuild. [Here](https://github.com/gentoo/gentoo/blob/master/gnome-extra/gnome-commander/metadata.xml) you can see who that is currently.

## Steps

1. Create a local overlay via layman:
   https://wiki.gentoo.org/wiki/Ebuild_repository#Layman

2. After successfully running:
   ```bash
   meson setup --reconfigure builddir
   ```
   a new ebuild is created with actual dependencies under `builddir/data/gnome-commander-X.Y.Z.ebuild`.
   Since meson does not know about Cargo dependencies, we now need to execute
   ```bash
   pycargoebuild ./
   ```
   The command creates another skeleton ebuild file with all crates
   through the file `Cargo.lock`.
   Now, merge the CRATES section in the ebuild under `builddir/data/`
   with the one just created under `./gnome-commander-0.1.0.ebuild`.

   Move the merged ebuild file to:
   ```
   /usr/local/portage/gnome-extra/gnome-commander/
   ```

3. Go to the target directory:
   ```bash
   cd /usr/local/portage/gnome-extra/gnome-commander
   ```

4. Create the manifest:
   ```bash
   pkgdev manifest
   ```

5. Run:
   ```bash
   eix-sync -0
   ```

6. Now, the packages needs to be tested in a sandbox. Do it via
   ```bash
   pkg-testing-tool --append-emerge='--autounmask=y' --extra-env-file 'test.conf' --test-feature-scope never -p '=gnome-extra/gnome-commander-X.Y.Z'
   ```
   whereas `test.conf` is the configuration mentioned under https://wiki.gentoo.org/wiki/Package_testing.
   You might need to unmask some packages before the build runs through.
   In case there are too many USEFLAG combinations, you can reduce the number of runs with the
   argument `--max-use-combinations n`, where n is the maximal number of USEFLAG combinations.

6. Install the package:
   ```bash
   emerge -av gnome-commander
   ```

7. Review the Gentoo Git pull request policy:
   https://www.gentoo.org/glep/glep-0066.html

8. Run pkgcheck to validate the ebuild (inside the directory containing the Manifest):
   ```bash
   pkgcheck scan gnome-commander-X.Y.Z.ebuild
   ```

9. Create a remote fork of the Gentoo repository:
   https://github.com/gentoo/gentoo

   Then:
   - Clone your fork. The repo is huge, better create a shallow clone
     ```bash
     git clone git@github.com:$USER/gentoo.git --shallow-since yesterday
     ```
   - Add the new `Manifest` line and add the ebuild file (from step 4 in `/usr/local/portage/gnome-extra/gnome-commander/`)  to the following folder in the forked repo:
     ```
     gnome-extra/gnome-commander/
     ```
   - Update the Manifest using `pkgdev manifest` in that folder.
   - Create a signed commit:
     ```bash
     git add gnome-commander-X.Y.Z.ebuild Manifest
     pkgdev commit
     ```

10. Push your commit to your GitHub repository and open a merge request to the Gentoo repository.
