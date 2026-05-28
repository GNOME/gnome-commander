FROM rust:1.92

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
  apt-get install -y appstream-util desktop-file-utils build-essential meson gettext yelp-tools \
  gi-docgen gobject-introspection libgtk-4-dev libvte-2.91-gtk4-dev 7zip python3-pypdf \
  python3-exifread python3-mutagen xvfb

RUN rustup component add rustfmt

WORKDIR /app
CMD ["bash"]
