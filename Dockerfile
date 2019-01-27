# This is a Docker image for building gnome-commander
FROM ubuntu:18.04
MAINTAINER Uwe Scholz <u.scholz83@gmx.de>

ENV GCMD_PATH /gnome-commander

RUN \
  sed -Ei 's/^# deb-src /deb-src /' /etc/apt/sources.list && \
  apt-get update -qq && \
  apt-get install -yq sudo && \
  echo $LANG && \
  echo $LC_ALL && \
  sudo apt-get build-dep gnome-commander -y && \
  sudo apt-get install -y -qq cmake flex git-core libglib2.0-dev libgtest-dev libunique-dev yelp-tools

RUN \
  cd /usr/src/gtest && \
  sudo cmake . && \
  sudo make -j4 && \
  sudo ln -s /usr/src/gtest/libgtest.a /usr/lib/libgtest.a && \
  sudo ln -s /usr/src/gtest/libgtest_main.a /usr/lib/libgtest_main.a

ADD . $GCMD_PATH

RUN \
  export CXX=g++ && \
  export CC=gcc && \
  cd $GCMD_PATH && \
  ./autogen.sh && \
  make && \
  make check
