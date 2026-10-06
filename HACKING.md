# Development Quick Start

The front end development process is currently best supported under Linux on a
x86_64 processor.

To ensure a consistent development environment, facilitate easy issue
reproduction, reduce setup pains for the most common cases, and make the
development process more accessible to contributors across various
distributions and on other platforms, Docker is utilized by the project.

As a result it is *highly recommended* to use the Docker based development
process as this will typically provide the best experience (even if directly
running a Linux-based operation system).

## Prerequisites

The first step (regardless of Docker use) is to make EDG development tools
available.

The recommended way to do this for Linux and MacOS users is to install
[direnv](https://direnv.net/); once installed and enabled in the project
directly this will automatically update your `PATH` and `PYTHONPATH`.

If direnv is not an option, manually add `<EDG PROJECT DIR>/dev_tools/bin` (or
`<EDG PROJECT DIR>/dev_tools/win-bin` for Windows users) to your `PATH` and
`<EDG PROJECT DIR>/dev_tools/pylibs` to your `PYTHONPATH` environment
variables respectively.  This will make the EDG development tools and their
supporting Python libraries available.

## Docker Based (Recommended)

To get started with Docker, it is recommended that Windows and MacOS users
install [Docker Desktop](https://www.docker.com/products/docker-desktop/).
For Linux users, using [Docker Engine](https://docs.docker.com/engine/)
directly will typically result in a better experience (as this avoids
the virtualization imposed by Docker Desktop).

Linux users using Docker Engine must add themselves to the `docker` group for
EDG tools to operate correctly, see:
https://docs.docker.com/engine/install/linux-postinstall/

With docker installed and host system environment variables configured, you
should then run `dev-init.py docker`.  This will verify the docker command is
found, verify EDG Python tools and libraries can be found, and install git
hooks used for project development.

It should be possible to run `edg-docker-test` from anywhere within the project
directory.  This will use docker to build and test the front end.

`edg-docker-shell` can similarly be used from anywhere within the project
directory to enter a development shell.  This enables the use of the included
`gdb` and `rr` debuggers within the context of the dockerized environment
and additionally provides direct access to tools like `edgy`, `cmake`, and
`ninja`.

## Native

When configuring natively, CMake selects the macro configuration
(`EDG_MACRO_CONF`), `EDG_BASE`, and the runtime libraries (`EDG_CPP_RT_LIBS`)
that match the host platform, compiler, and build type, so a plain configure
(such as an IDE's default CMake profile) behaves like the corresponding preset.
Setting any of these explicitly (e.g., with `export EDG_BASE=...` or
`-DEDG_MACRO_CONF=...`) overrides the automatic selection.

### Linux

To get started with Linux-native development, make sure `g++` and `cmake` are
installed (additionally it's highly recommended to install `ninja`).

Once these dependencies are installed and the host system environment variables
are configured, you should then run `dev-init.py native`.  This will verify
that `cmake` can be found, test for `ninja`, build a native version of the C++
`dev_tools`, verify EDG Python tools and libraries can be found, and install
git hooks used for project development.

It should then be possible to follow the build proceedure above with the
`linux-gcc-debug` preset, and perform the equivalent process to
`edg-docker-test` (from the EDG project directory):

```
export EDG_GCC_INCL_SCRAPE=$(edg-scrape-compiler gcc --lang c++ includes)
export EDG_GCC_CINCL_SCRAPE=$(edg-scrape-compiler gcc --lang c includes)
export EDG_GCC_VER_SCRAPE=$(edg-scrape-compiler gcc version)

cmake --preset linux-gcc-debug
cd build/gcc
ninja
# If not using direnv:
# source environment.sh
edgy
```

> [!WARNING]
>
> Building and testing is less automated when developing directly.
> Additionally, minor differences in test output may be observed due to
> slightly different versions of tools and system headers.
>
> Tooling and testing contributions to reduce these issues are welcomed.

### MacOS

To get started with MacOS-native development, make sure `clang` and `cmake` are
installed (additionally it's highly recommended to install `ninja`).

Once these dependencies are installed and the host system environment variables
are configured, you should then run `dev-init.py native`.  This will verify
that `cmake` can be found, test for `ninja`, build a native version of the C++
`dev_tools`, verify EDG Python tools and libraries can be found, and install
git hooks used for project development.

It should then be possible to follow the build proceedure above with the
`macos-arm-clang-debug` preset, and perform something roughly equivalent to the
`edg-docker-test` process (from the EDG project directory):

```
export EDG_CLANG_INCL_SCRAPE=$(edg-scrape-compiler clang --lang c++ includes)
export EDG_CLANG_CINCL_SCRAPE=$(edg-scrape-compiler clang --lang c includes)
export EDG_CLANG_VER_SCRAPE=$(edg-scrape-compiler clang version)

cmake --preset macos-arm-clang-debug
cd build/clang
ninja
# If not using direnv:
# source environment.sh
edgy
```

> [!WARNING]
>
> MacOS native development suffers from similar issues to that of Linux native
> development.  Additionally as Apple has moved MacOS to be a predominately ARM
> based platform built upon the Clang & libc++ C++ stack there are additional
> challenges getting stable recording output.
>
> Tooling and testing contributions to reduce these issues are welcomed.

### Windows

> [!CAUTION]
>
> Windows native development is currently severely limited.  This is primarily
> due to remaining dependencies in the testing system on Bash (`util/eccp.sh`,
> `bin/eccp`, `dev_tools/bin/cpfe-cp-gen-be`).
>
> Tooling improvements to replace eccp with an improved cross platform driver,
> changes to replace scripts like cpfe-cp-gen-be with logic in the edgtest
> Python library, and other changes aimed at closing the gap for Windows
> contribution and testing are welcomed.
>
> Building and running the front end on Windows is supported, but there
> is not an official testing process that works directly on Windows.  Once the
> Bash dependencies are removed, testing directly on Windows should become
> possible.  At this point remaining issues will be similar to the challenges
> faced with running tests on MacOS (namely different system tools and
> different system headers).

To get started with Windows-native development, make sure `msvc`, `python`, and
`cmake` are installed (additionally -- if not using the Visual Studio IDE --
it's highly recommended to install `ninja`).

Once these dependencies are installed and the host system environment variables
are configured, you should then run `dev-init.py native`.  This will verify
that `cmake` can be found, test for `ninja`, build a native version of the C++
`dev_tools`, verify EDG Python tools and libraries can be found, and install
git hooks used for project development.

A development build of the front end can then be acquired by running (typically
in a Visual Studio development shell) cmake with the following for a
Ninja-based build:

```
cmake --preset=windows-msvc-debug
cd build\\msvc
ninja
```

**OR** for a Visual Studio Solution file:

```
cmake --preset=windows-msvc-sln
# generated files will be present in "build\\msvc-sln"
```
