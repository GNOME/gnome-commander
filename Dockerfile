# This is a Docker image for building gnome-commander
FROM ubuntu:17.04
MAINTAINER Uwe Scholz <u.scholz83@gmx.de>

ENV GCMD_PATH /gnome-commander

RUN \
  apt-get update -qq && \
  apt-get install -yq sudo && \
  echo $LANG && \
  echo $LC_ALL && \
  sudo apt-get build-dep gnome-commander -y && \
  sudo apt-get install -y -qq autoconf-archive cmake flex git-core gnome-common libglib2.0-dev libgtest-dev libunique-dev yelp-tools

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
