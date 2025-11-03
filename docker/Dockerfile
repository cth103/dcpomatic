FROM ubuntu:24.04

RUN apt update
RUN apt -y upgrade
RUN apt -y install \
	    autoconf \
	    cmake \
	    g++ \
	    gettext \
	    git \
	    gpac \
	    libboost-date-time-dev \
	    libboost-filesystem-dev \
	    libboost-locale-dev \
	    libboost-regex-dev \
	    libboost-test-dev \
	    libboost-thread-dev \
	    libcurl4-openssl-dev \
	    libfftw3-dev \
	    libfluidsynth-dev \
	    libfmt-dev \
	    libhpdf-dev \
	    liblilv-dev \
	    libmagick++-dev \
	    libnanomsg-dev \
	    libpangomm-1.4-dev \
	    libpolkit-gobject-1-dev \
	    libssh-dev \
	    libsndfile-dev \
	    libsqlite3-dev \
	    libssl-dev \
	    libwxgtk3.2-dev \
	    libx264-dev \
	    libxerces-c-dev \
	    libxml++2.6-dev \
	    libxmlsec1-dev \
	    libzip-dev \
	    meson \
	    m4 \
	    nlohmann-json3-dev \
	    pkg-config \
	    python-is-python3 \
	    valgrind \
	    wx3.2-i18n \
	    xmlsec1

ADD https://dcpomatic.com/deps/linux/nasm-2.16.03.tar.xz /src/
RUN cd /src && tar xJf nasm-2.16.03.tar.xz && cd nasm-2.16.03 && ./configure && make -j8 && make install

ADD https://dcpomatic.com/deps/linux/dav1d-0.9.2.tar.bz2 /src/
RUN cd /src && \
    tar xjf dav1d-0.9.2.tar.bz2 && \
    cd dav1d-0.9.2 && \
    mkdir build && \
    meson build --default-library=static --libdir=/usr/local/lib && \
    ninja -C build install && \
    sed -i "s/-ldav1d/-ldav1d -pthread -ldl/" /usr/local/lib/pkgconfig/dav1d.pc

ADD https://dcpomatic.com/deps/linux/fast_float-7.0.0.tar.gz /src/
RUN cd /src && tar xzf fast_float-7.0.0.tar.gz && cd fast_float-7.0.0 && cmake . && make -j4 install

RUN mkdir -p /work /root/.ssh
ADD id_rsa /work
RUN echo "git.carlh.net ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBDd4O2zuNKuYxEWfmOoHnaJOgY6hxsppED9g+9Cmgy9tJdPwwVovrf+UnBa/GguP5OZZV0ocJIgWCU+4JZNgvK8=" > /root/.ssh/known_hosts
RUN git clone git://git.carlh.net/git/cdist.git && cd cdist && git checkout b90daaeaf433c3b499b38042db20376422bfc171 && cp cdist /usr/local/bin
ADD cdist-config /work/
ENV PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig
ARG CACHEBURST=1
RUN cdist -p dcpomatic -c v2.19.x --key /work/id_rsa --config /work/cdist-config -w /usr/local -t local --local-distro ubuntu --local-version 24.04 -k build

RUN userdel -r ubuntu
RUN useradd -ms /bin/bash -u 1000 carl

