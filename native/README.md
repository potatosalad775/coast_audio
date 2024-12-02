# Building Native Library

## Windows

Tested on Ubuntu with example Application, Requires [MXE](https://mxe.cc).

1. Install MXE
```
# Download MXE Codebase
cd /opt
sudo git clone https://github.com/mxe/mxe.git

# Install requirements for MXE
sudo apt-get update
sudo apt-get install autoconf automake autopoint bash bison bzip2 flex g++ g++-multilib gettext \
    git gperf intltool libc6-dev-i386 libgdk-pixbuf2.0-dev libltdl-dev libgl-dev libpcre3-dev \
    libssl-dev libtool-bin libxml-parser-perl lzip make openssl p7zip-full patch perl python3 \
    python3-distutils python3-mako python3-packaging python3-pkg-resources python-is-python3 ruby \
    sed sqlite3 unzip wget xz-utils

# Build MXE (It does take some time building it)
cd /opt/mxe
sudo make cc cmake dlfcn-win32 MXE_TARGETS='x86_64-w64-mingw32.shared'
```

2. Run Makefile

You'll now get libcoast_audio.dll in /prebuilt/windows/x86_64.