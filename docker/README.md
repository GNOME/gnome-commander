## Build and execution environment for Gnome Commander

Building Gnome Commander requires installing many dependencies. This Docker image exists to make the job somewhat simpler. While it is primarily meant for our continuous integration (CI) builds, you can also use it to build Gnome Commander:

```sh
alias meson='docker run --rm -v .:/app registry.gitlab.gnome.org/gnome/gnome-commander/buildenv:latest meson'
meson setup builddir
meson compile -C builddir
meson test -C builddir
```

### Updating this image

This image needs to be updated whenever the minimum required Rust version or any of the build or runtime dependencies change. Make the necessary changes to the `Dockerfile`, then run (replacing `N.N.N` by the desired version number):

```sh
docker build -t registry.gitlab.gnome.org/gnome/gnome-commander/buildenv:vN.N.N .
docker tag registry.gitlab.gnome.org/gnome/gnome-commander/buildenv:vN.N.N registry.gitlab.gnome.org/gnome/gnome-commander/buildenv:latest
docker push -a registry.gitlab.gnome.org/gnome/gnome-commander/buildenv
```

Update `.gitlab-ci.yml` file to pull the new version of the image after that.
