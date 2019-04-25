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
  sudo apt-get install gcc-8 g++-8 -y && \
  sudo apt-get build-dep gnome-commander -y && \
  sudo apt-get install -y -qq cmake flex git-core libglib2.0-dev libgtest-dev libunique-dev yelp-tools && \
  sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 700 --slave /usr/bin/g++ g++ /usr/bin/g++-7 && \
  sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 800 --slave /usr/bin/g++ g++ /usr/bin/g++-8

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
