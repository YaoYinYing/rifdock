# RifDock
RifDock Library for Conformational Search

Ideally RifDock should be merged into Rosetta, but no one has stepped up to this herculean task yet.

Here's some incomplete documentation:
https://github.com/rifdock/rifdock/blob/master/help/some_rifdock_documentation.md

***Building***

RifDock links against Rosetta. As RifDock is not managed by Rosetta Commons, it has fallen behind in terms of the code. The latest version of Rosetta will not work. Instead, use one of the two following versions:

<b>Academic/Commercial Version</b>: Rosetta 3.9

<b>Github branch</b>: bcov/stable1

To build RifDock:

Optain a copy of gcc with version >= 5.0

Install boost version 1.65 or later.

Build a Rosetta cxx11_omp build with:  
```bash
cd rosetta/main/source
# git checkout bcov/stable1 # If you have git access, otherwise use Rosetta 3.9
CXX=/my/g++/version CC=/my/gcc/version ./ninja_build cxx11_omp -t rosetta_scripts -remake  
```

Clone this repository and perform:  
```bash
cd rifdock  
mkdir build  
cd build  
CXX=/my/g++/version CC=/my/gcc/version CMAKE_ROSETTA_PATH=/Path/to/a/rosetta/main cmake .. -DCMAKE_BUILD_TYPE=Release  
make -j3 rif_dock_test rifgen  
```

There is an optional CMAKE flag if you do did not link against the standard cxx11_omp build to specify which build you did use. That flag is as follows:  
```bash
CMAKE_FINAL_ROSETTA_PATH=/Path/to/a/rosetta/main/source/cmake/build_my_custom_build_type  
```

Use this flag in addition to the CMAKE_ROSETTA_PATH flag.

Unit tests may be built with:  
```bash
make test_libscheme  
```

***Running***

The executables for RifDock are built at:  
```bash
rifdock/build/apps/rosetta/rifgen  
rifdock/build/apps/rosetta/rif_dock_test  
```

The unit test executable is at:  
```bash
rifdock/build/schemelib/test/test_libscheme  
```


***Modifying***

RifDock is licenced under the Apache 2 License which can be found at rifdock/LICENSE.



***Ubuntu Specific Additional Compilation Help***

If you're on ubuntu and things aren't going your way. Try this out:

```bash 
# Yinying arrange this based on the the following documentation:
# https://github.com/rifdock/rifdock/blob/master/help/ubuntu_rifdock_compilation_extra.pdf


# install gcc-5 
sudo apt-get update
sudo apt install gcc-5
sudo apt install g++-5

ROSETTA39=/software/rosetta/3.9/;
SOFTWARE=/software;


# use a clean copy of rosetta source code
cd $ROSETTA39;
tar xvzf <path-to-rosetta_src_3.9_bundle.tgz>
cd rosetta_src_2018.09.60072_bundle/main/source/

# use python2.7 for ninja_build.py
conda activate python27
CC=$(which gcc-5) CXX=$(which g++-5) ./ninja_build.py cxx11_omp -t rosetta_scripts -remake
CC=$(which gcc-5) CXX=$(which g++-5) ./ninja_build.py cxx11_omp_debug -t rosetta_scripts -remake


# build boost w/ gcc-5
cd $SOFTWARE;
mkdir boost_1_65
cd boost_1_65/
aria2c https://sourceforge.net/projects/boost/files/boost/1.65.0/boost_1_65_0.tar.gz/download -x16

tar zvfx  boost_1_65_0.tar.gz
cd boost_1_65_0
CC=$(which gcc-5) CXX=$(which g++-5) ./bootstrap.sh
CC=$(which gcc-5) CXX=$(which g++-5) ./b2 -j20


# compile rifdock w/ gcc-5

cd $SOFTWARE
git clone https://github.com/rifdock/rifdock.git
cd rifdock
mkdir build_ubuntu/
cd build_ubuntu/

CC=gcc-5 CXX=g++-5 CXXFLAGS="-isystem $SOFTWARE/boost_1_65/boost_1_65_0" CMAKE_ROSETTA_PATH=$ROSETTA39/rosetta_src_2018.09.60072_bundle/main CMAKE_FINAL_ROSETTA_PATH=$ROSETTA39/rosetta_src_2018.09.60072_bundle/main/source/cmake/build_cxx11_omp DCMAKE_BUILD_TYPE=Release DCMAKE_PREFIX_PATH=$SOFTWARE/boost_1_65/boost_1_65_0/stage/lib cmake ..
make -j20 rif_dock_test rifgen
```
