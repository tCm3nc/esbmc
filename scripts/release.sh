#!/bin/bash

function checksanity() {
  # You need a 64 bit machine to fully build a release
  if test `uname -m` != "x86_64"; then
    echo "Please run release.sh on a 64 bit machine"
    exit 1
  fi

  # You also need to be running it in the root ESBMC dir
  stat .git > /dev/null 2>/dev/null
  if test $? != 0; then
    echo "Please run release.sh in the root dir of ESBMC"
    exit 1
  fi

  # Check to see whether or not there's an instance for
  # this version in release notes
  # (Start by removing leading v)
  vernum=`echo $1 | sed s#v\(.*\)#\1#`
  grep "\*\*\*.*$vernum.*\*\*\*" ./scripts/release-notes.txt
  if test $? != 0; then
    echo "Can't find an entry for $1 in release-notes.txt; you need to write one"
    exit 1
  fi
}

checksanity

while getopts ":3:6:2:5:r:" opt; do
  case $opt in
    3)
      satdir32=$OPTARG
      ;;
    2)
      satdir32compat=$OPTARG
      ;;
    6)
      satdir64=$OPTARG
      ;;
    5)
      satdir64compat=$OPTARG
      ;;
    r)
      targetrefname=$OPTARG
    \?)
      echo "Invalid option -$OPTARG" >&2
      exit 1
      ;;
    :)
      echo "Option -$OPTARG requires argument" >&2
      exit 1
      ;;
  esac
done

function missingarg {
  if test -z $1; then
    echo "Missing argument $2" >&2
    exit 1
  fi
}

missingarg "$satdir32" "-3"
missingarg "$satdir64" "-3"
missingarg "$satdir32compat" "-2"
missingarg "$satdir64compat" "-5"

# Tell the user about what version of Z3 we're about to compile with

function printz3 {
  z3ver=`$1/z3/bin/z3 -version | cut "--delim= " -f 3`
  echo "Z3 for $2: $z3ver"
}

printz3 satdir64 "Linux64"
printz3 satdir32 "Linux32"
printz3 satdir64compat "LinuxCompat64"
printz3 satdir32compat "LinuxCompat32"

# Find whatever the current head is
CURHEAD=`git symbolic-ref HEAD`

if test $? != 0; then
  # Not checked out a symbolic ref right now
  CURHEAD=`cat .git/HEAD`
fi

# Strip "refs/heads/" or suchlike from CURHEAD
CURHEAD=`basename $CURHEAD`

# Then, checkout whatever we've been told to release
git stash > /dev/null
git checkout $targetrefname > /dev/null

if test $? != 0; then
  echo "Couldn't checkout $targetrefname"
  exit 1
fi

# And wrap all our modifications into a function, so that upon error we can
# cleanly remove all changes to the checked out copy.

function dobuild () {

  # Install our configuration files.
  cp ./scripts/release_config.inc ./config.inc
  cp ./scripts/release_local.inc ./local.inc

  # And build build build
  rm -rf .release
  mkdir .release

  # For release builds, no debug information
  export EXTRACFLAGS="-DNDEBUG"
  export EXTRACXXFLAGS="-DNDEBUG"

  # Use 64 bit libraries
  export SATDIR=$satdir64

  echo "Building 64 bit ESBMC"
  make clean > /dev/null 2>&1
  make > /dev/null 2>&1

  if test $? != 0; then
    echo "Build failed."
    return 1
  fi

  cp esbmc/esbmc .release/esbmc

  if test $buildcompat = 1; then
    echo "Building compat 64 bit ESBMC"
    export SATDIR=$satdir64compat
    make clean > /dev/null 2>&1
    env CC=gcc34 CXX=g++34 make > /dev/null 2>&1

    if test $? != 0; then
      echo "Build failed."
      return 1
    fi

    cp esbmc/esbmc .release/esbmc_compat
  fi

  # Try for 32 bits
  echo "Building 32 bit ESBMC"
  export SATDIR=$satdir32

  buildfor32bits=0
  make clean > /dev/null 2>&1

  env EXTRACFLAGS="-m32 -DNDEBUG" EXTRACXXFLAGS="-m32 -DNDEBUG" LDFLAGS="-m elf_i386" make > /dev/null 2>&1

  if test $? != 0; then
    echo "Buildling 32 bits failed; do you have the right headers and libraries?"
    return 1
  else
    buildfor32bits=1
    cp esbmc/esbmc .release/esbmc32
    if test $buildcompat = 1; then
      echo "Building 32 bit compat ESBMC"

      export SATDIR=$satdir32compat

      make clean > /dev/null 2>&1

      env CC=gcc34 CXX=g++34 EXTRACFLAGS="-m32 -DNDEBUG" EXTRACXXFLAGS="-m32 -DNDEBUG" LDFLAGS="-m elf_i386" make > /dev/null 2>&1

      if test $? != 0; then
        echo "Building 32 bit compat ESBMC failed"
        return 1
      fi

      cp esbmc/esbmc .release/esbmc32_compat
    fi
  fi

}

function cleanup () {
  echo "Cleaning up"
  make clean > /dev/null 2>&1

  # Clear anything we left behind
  git reset --hard

  # Check back out whatever ref we had before.
  git checkout $CURHEAD
  git stash pop
}

function buildtgz {
  version=$1
  suffix=$2
  binpath=$3

  tmpdirname=`mktemp -d`
  projname="esbmc-$version-linux-$suffix"
  dirname="$tmpdirname/$projname"
  mkdir $dirname
  mkdir $dirname/bin
  mkdir $dirname/licenses
  mkdir $dirname/smoke-tests

  # Copy data in
  cp scripts/README $dirname
  cp scripts/release-notes.txt $dirname
  cp $binpath $dirname/bin/esbmc
  cp scripts/licenses/* $dirname/licenses
  cp regression/smoke-tests/* $dirname/smoke-tests/

  # Create a tarball
  tar -czf .release/$projname.tgz -C $tmpdirname $projname
}

function buildtarballs() {
  version=$1

  buildtgz $version "64" ".release/esbmc"
  buildtgz $version "32" ".release/esbmc32"
  if test $buildcompat = 1; then
    buildtgz $version "64-compat" ".release/esbmc_compat"
    buildtgz $version "32-compat" ".release/esbmc32_compat"
  fi
}

# If we get sigint/term/hup, cleanup before quitting.
trap "echo 'Exiting'; cleanup; exit 1" SIGHUP SIGINT SIGTERM

dobuild

# We now have a set of binaries (or an error)
if test $? != 0; then
  echo "Build failed"
fi

buildtarballs $targetrefname

cleanup

# fini
