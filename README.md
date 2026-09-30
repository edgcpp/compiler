# EDG Compiler Project

Welcome to the open source EDG C/C++ compiler project!

## About

The EDG C/C++ front end is the primary focus of this repository.  The EDG front
end is known for: its excellent parsing compatibility and bug emulation, to
make sure source that compiles with Clang, GCC, and MSVC compiles with EDG;
extensive documentation, to assist with development and modification; and
extreme configurability, to allow it to be derived for use into a large number
of C/C++ oriented products.

The compiler project also includes: a C-generating back end, which can be used
to generate C code for C++ programs; a C++-generating back end, which is useful
for source-to-source transformation applications; a prelinker, which handles
automatic template instantiation; a minimal runtime support library (but not
any "real" libraries, e.g., for stream I/O); utilities to write the
intermediate language to a file, read it back in, and display it in
human-readable form; a name demangler; and a collection of purpose built
development tools.

## Documentation

See the documentation on: https://edgcpp.org/doc/.  Documentation for the
current development branch (`main`) is on: https://edgcpp.org/dev/doc/.

> [!NOTE]
>
> If you would like to build a local copy of the documentation see the
> [documentation build guide](doc/README.md).

## Building & Contributing

To get started, it's recommended for most people to clone the repository using
a partial clone (this keeps a full copy of the git history while only
downloading the most recent version of each file, git can implicitly fetch
older versions as needed):

```
git clone --filter=blob:none git@github.com:edgcpp/compiler.git
```

If you are looking to build the front end from source and use it, please
see [the build instructions](BUILD.md).

If you are looking to make changes to the front end, please see the
[contribution guide](CONTRIBUTING.md) and the [development guide](HACKING.md)
for quickly getting setup.

## Tips, Tricks, & Tutorials

To further aid humans and AI agents in product development there is additional
documentation for tips, tricks, and tutorials that can be found in the
[development annex](dev_annex/README.md).
