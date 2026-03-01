
# =====================================================================
# OMNeT++ 6.0.1 + INET 4.5.2 + Full IDE (GTK/SWT) + Qtenv + noVNC
# Works in devcontainers/wrappers (single-line apt installs, locales ready)
# =====================================================================
ARG VARIANT="22.04"
FROM ubuntu:${VARIANT} AS stage-base

ARG USER_UID
ARG USER_GID

ENV DEBIAN_FRONTEND=noninteractive \
    DISPLAY=:1 \
    VNC_RESOLUTION=1600x900 \
    USER_NAME=ubuntu \
    LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8 \
    QT_QPA_PLATFORM=xcb \
    SWT_GTK3=1

# ---------------------------------------------------------------------
# Base system & tools (SINGLE LINE to avoid wrapper parse issues)
# Includes: Qt for Qtenv, FULL GTK/SWT stack for IDE, xdg-utils, noVNC
# ---------------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    apt-transport-https ca-certificates \
    sudo wget curl git nano xz-utils unzip tini \
    build-essential cmake gdb make clang pkg-config \
    bison flex libxml2-dev zlib1g-dev \
    python3 python3-pip python3-dev swig \
    # GUI stack
    fluxbox xterm xdg-utils \
    # Qt for Qtenv
    qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools libqt5opengl5-dev libqt5svg5-dev \
    # GTK/SWT deps (Eclipse-based OMNeT++ IDE)
    locales \
    libgtk-3-0 libgtk-3-bin libgtk-3-common \
    libglib2.0-0 libglib2.0-dev \
    libgdk-pixbuf-2.0-0 libgdk-pixbuf2.0-dev \
    libpango-1.0-0 libpangocairo-1.0-0 \
    libatk1.0-0 libatk-bridge2.0-0 \
    libx11-6 libxcomposite1 libxcursor1 libxdamage1 libxext6 libxi6 libxinerama1 \
    libxrandr2 libxrender1 libxtst6 \
    libgstreamer1.0-0 libgstreamer-plugins-base1.0-0 \
    libglu1-mesa libgl1-mesa-glx \
    dbus-x11 \
    libcanberra-gtk3-module \
    libcanberra-gtk-module \
    # noVNC + proxy
    novnc websockify \
    # TigerVNC server + supervisord
    tigervnc-standalone-server tigervnc-common tigervnc-tools supervisor \
    # network tools \
    libcap2-bin \
    iproute2 iputils-ping net-tools \
    tcpdump tshark wireshark-common \
    nmap socat netcat-openbsd \
    ethtool bridge-utils arping \
    dnsutils traceroute \
    iftop nethogs iperf3 \
    ngrep tcpflow \
    mtr-tiny whois \
 && rm -rf /var/lib/apt/lists/*

RUN setcap cap_net_raw,cap_net_admin=eip /usr/bin/tcpdump \
 && setcap cap_net_raw,cap_net_admin=eip /usr/bin/dumpcap

# ---------------------------------------------------------------------
# Locales (some wrappers call 'locale-gen' explicitly; make it available)
# ---------------------------------------------------------------------
RUN sed -i 's/^[# ]*en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen \
 && locale-gen en_US.UTF-8 \
 && update-locale LANG=en_US.UTF-8

# ---------------------------------------------------------------------
# Python stack OMNeT++ IDE checks for (run as root during build)
# ---------------------------------------------------------------------
FROM stage-base AS stage-omnnett
# ---------------------------------------------------------------------
# User
# ---------------------------------------------------------------------
RUN groupadd --gid ${USER_GID} ${USER_NAME} \
 && useradd -m --uid ${USER_UID} --gid ${USER_GID} -s /bin/bash ${USER_NAME} \
 && echo "%${USER_NAME} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers


RUN python3 -m pip install --upgrade pip \
 && pip install numpy scipy pandas matplotlib posix_ipc

# ---------------------------------------------------------------------
# OMNeT++
# ---------------------------------------------------------------------
ARG OMNET_VERSION=6.0.1
ENV OMNET_ROOT=/opt/omnetpp-${OMNET_VERSION}
WORKDIR /opt

RUN wget -O omnetpp-${OMNET_VERSION}-linux-x86_64.tgz \
      "https://github.com/omnetpp/omnetpp/releases/download/omnetpp-${OMNET_VERSION}/omnetpp-${OMNET_VERSION}-linux-x86_64.tgz" \
  ||  wget -O omnetpp-${OMNET_VERSION}-linux-x86_64.tgz \
      "https://omnetpp.org/omnetpp-${OMNET_VERSION}-linux-x86_64.tgz" \
 && tar -xzf omnetpp-${OMNET_VERSION}-linux-x86_64.tgz \
 && rm omnetpp-${OMNET_VERSION}-linux-x86_64.tgz \
 && chown -R ${USER_NAME}:${USER_NAME} ${OMNET_ROOT}

# Make OMNeT++ tools available for everyone
RUN echo "source ${OMNET_ROOT}/setenv" >> /etc/bash.bashrc
ENV PATH="${OMNET_ROOT}/bin:${PATH}"

# ---------------------------------------------------------------------
# Build OMNeT++ (Qtenv + IDE). We keep NO_INSTALL_SHORTCUTS to be safe.
# ---------------------------------------------------------------------
USER ${USER_NAME}
WORKDIR ${OMNET_ROOT}
RUN . ./setenv \
 && ./configure WITH_QTENV=yes WITH_OSG=no \
 && make -j"$(nproc)" NO_INSTALL_SHORTCUTS=yes

# ---------------------------------------------------------------------
# INET
# ---------------------------------------------------------------------
ARG INET_VERSION=4.5.2
WORKDIR /home/${USER_NAME}/packages
RUN git clone --branch v${INET_VERSION} --depth 1 https://github.com/inet-framework/inet.git \
 && cd inet \
 && bash -lc ". ${OMNET_ROOT}/setenv && source ./setenv && make makefiles && make -j\"$(nproc)\""


FROM stage-omnnett AS stage-finalization


# IMPORTANT: run as non-root at runtime
USER ubuntu


#USER root
#RUN chown -R 1001:1001 /workspaces/inet
#RUN chown -R 1001:1001 /opt/omnetpp-6.0.1
RUN mkdir -p /home/${USER_NAME}/src && chown -R ${USER_NAME}:${USER_NAME} /home/${USER_NAME}
# ---------------------------------------------------------------------
# noVNC entrypoint
# ---------------------------------------------------------------------
USER root

COPY entrypoint.sh /usr/local/bin/entrypoint.sh

RUN chmod +x /usr/local/bin/entrypoint.sh \
 && mkdir -p /etc/supervisor/conf.d

USER ubuntu
ENTRYPOINT ["/bin/bash", "-c", "/usr/local/bin/entrypoint.sh"]
