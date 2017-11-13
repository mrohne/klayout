# Building on macOS

This is a fork of https://github.com/klayoutmatthias/klayout. The
purpose is to provide a stable build on recent macOS, supported by
homebrew. There are three complications to this goal:

* Toolkit: local/qt5 - Homebrew doesn't provide Qt4, only Qt5 and
  built with clang. The default (black) background makes graphics
  except the grid invisible.  Workaround: select another background

* Compiler: local/clang - Apple/LLVM clang++ seems to work differently
  from GNU/GCC g++.

  std::iterator_traits<it> requires /all/ five typedefs are
  consistently defined.

  Different treatment of template instantiation in convenience
  libraries (weak symbols) causes eg singletons to be
  duplicated. Workaround: use -fvisibility=default

* Application bundle: local/build - Qt5 macdeployqt doesn't quite cut
  it.

  Some Qt-frameworks /inside/ the bundle still points to other
  Qt-frameworks /outside/ the bundle. Workaround: run final fixup of
  references, see bundle.sh

# klayout


This repository will hold the main sources for the KLayout project.

Plugins can be included into the "plugins" directory from external sources.

For more details see http://www.klayout.org.


## Building requirements

* Qt 4.8 or later (4.6 with some restrictions)
* gcc 4.x or later

Here is a list of packages required for various Linux flavors:

* CentOS (6, 7): gcc gcc-c++ make qt qt-devel ruby ruby-devel python python-devel
* OpenSuSE: (13.2, 41.1): gcc gcc-c++ make libqt4 libqt4-devel ruby ruby-devel python3 python3-devel 
* Ubuntu (14.04, 16.10): gcc g++ make libz-dev libqt4-dev-bin libqt4-dev ruby ruby-dev python3 python3-dev

## Build options

* <b>Ruby</b>: with this option, Ruby scripts can be executed and developped within KLayout. Ruby support is detected automatically by the build script.
* <b>Python</b>: with this option, Python scripts can be executed and developped within KLayout. Python support is detected automatically by the build script.
* <b>Qt binding</b>: with this option, Qt objects are made available to Ruby and Python scripts. Qt bindings are enabled by default. Qt binding offers an option to create custom user interfaces from scripts and to interact with KLayout's main GUI. On the other hand, they provide a considerable overhead when building and running the application.
* <b>64 bit coordinate support</b>: with this option, the coordinate type used internally is extended to 64bit as compared to 32bit in the standard version. This will duplicate memory requirements for coordinate lists, but allow a larger design space. 64bit coordinate support is experimental and disabled by default.

## Building instructions (Linux)

### Plain building for Qt4

    ./build.sh 
    
### Plain building for Qt5

    ./build.sh -qt5 
    
### Building without Qt binding

    ./build.sh -without-qtbinding
    
### Debug build

    ./build.sh -debug

### Building with a particular Ruby version

    ./build.sh -ruby <path-to-ruby>

(path-to-ruby is the full path of the particular ruby interpreter)

### Building with a particular Python version

    ./build.sh -python <path-to-python>

(path-to-python is the full path of the particular python interpreter)

### Building with a particular Qt version

    ./build.sh -qmake <path-to-qmake>

(path-to-qmake is the full path of the particular qmake installation)

### Building with 64bit coordinate support (experimental)

    ./build.sh -with-64bit-coord

### Pass make options

    ./build.sh -j4 
    
(for running 4 jobs in parallel)

### More options

For more options use

    ./build.sh -h

## Running the Test Suite (Linux)

Go to the build directory (i.e. "bin-release") and enter

    export TESTTMP=testtmp    # path to a directory that will hold temporary data (will be created)
    export TESTSRC=..         # path to the source directory
    ./ut_runner
    
For more options use

    ./ut_runner -h


