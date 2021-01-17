#!/bin/bash
builddir=`dirname "$0"`
GCONV_PATH="${builddir}/iconvdata"

usage () {
  echo "usage: $0 [--tool=strace] PROGRAM [ARGUMENTS...]" 2>&1
  echo "       $0 --tool=valgrind PROGRAM [ARGUMENTS...]" 2>&1
  exit 1
}

toolname=default
while test $# -gt 0 ; do
  case "$1" in
    --tool=*)
      toolname="${1:7}"
      shift
      ;;
    --*)
      usage
      ;;
    *)
      break
      ;;
  esac
done

if test $# -eq 0 ; then
  usage
fi

case "$toolname" in
  default)
    exec   env GCONV_PATH="${builddir}"/iconvdata LOCPATH="${builddir}"/localedata LC_ALL=C  "${builddir}"/elf/ld-linux-x86-64.so.2 --library-path "${builddir}":"${builddir}"/math:"${builddir}"/elf:"${builddir}"/dlfcn:"${builddir}"/nss:"${builddir}"/nis:"${builddir}"/rt:"${builddir}"/resolv:"${builddir}"/mathvec:"${builddir}"/support:"${builddir}"/crypt:"${builddir}"/nptl ${1+"$@"}
    ;;
  strace)
    exec strace  -EGCONV_PATH=/home/nuc/graphene_test/graphene/LibOS/glibc-build/iconvdata  -ELOCPATH=/home/nuc/graphene_test/graphene/LibOS/glibc-build/localedata  -ELC_ALL=C  /home/nuc/graphene_test/graphene/LibOS/glibc-build/elf/ld-linux-x86-64.so.2 --library-path /home/nuc/graphene_test/graphene/LibOS/glibc-build:/home/nuc/graphene_test/graphene/LibOS/glibc-build/math:/home/nuc/graphene_test/graphene/LibOS/glibc-build/elf:/home/nuc/graphene_test/graphene/LibOS/glibc-build/dlfcn:/home/nuc/graphene_test/graphene/LibOS/glibc-build/nss:/home/nuc/graphene_test/graphene/LibOS/glibc-build/nis:/home/nuc/graphene_test/graphene/LibOS/glibc-build/rt:/home/nuc/graphene_test/graphene/LibOS/glibc-build/resolv:/home/nuc/graphene_test/graphene/LibOS/glibc-build/mathvec:/home/nuc/graphene_test/graphene/LibOS/glibc-build/support:/home/nuc/graphene_test/graphene/LibOS/glibc-build/crypt:/home/nuc/graphene_test/graphene/LibOS/glibc-build/nptl ${1+"$@"}
    ;;
  valgrind)
    exec env GCONV_PATH=/home/nuc/graphene_test/graphene/LibOS/glibc-build/iconvdata LOCPATH=/home/nuc/graphene_test/graphene/LibOS/glibc-build/localedata LC_ALL=C valgrind  /home/nuc/graphene_test/graphene/LibOS/glibc-build/elf/ld-linux-x86-64.so.2 --library-path /home/nuc/graphene_test/graphene/LibOS/glibc-build:/home/nuc/graphene_test/graphene/LibOS/glibc-build/math:/home/nuc/graphene_test/graphene/LibOS/glibc-build/elf:/home/nuc/graphene_test/graphene/LibOS/glibc-build/dlfcn:/home/nuc/graphene_test/graphene/LibOS/glibc-build/nss:/home/nuc/graphene_test/graphene/LibOS/glibc-build/nis:/home/nuc/graphene_test/graphene/LibOS/glibc-build/rt:/home/nuc/graphene_test/graphene/LibOS/glibc-build/resolv:/home/nuc/graphene_test/graphene/LibOS/glibc-build/mathvec:/home/nuc/graphene_test/graphene/LibOS/glibc-build/support:/home/nuc/graphene_test/graphene/LibOS/glibc-build/crypt:/home/nuc/graphene_test/graphene/LibOS/glibc-build/nptl ${1+"$@"}
    ;;
  container)
    exec env GCONV_PATH=/home/nuc/graphene_test/graphene/LibOS/glibc-build/iconvdata LOCPATH=/home/nuc/graphene_test/graphene/LibOS/glibc-build/localedata LC_ALL=C  /home/nuc/graphene_test/graphene/LibOS/glibc-build/elf/ld-linux-x86-64.so.2 --library-path /home/nuc/graphene_test/graphene/LibOS/glibc-build:/home/nuc/graphene_test/graphene/LibOS/glibc-build/math:/home/nuc/graphene_test/graphene/LibOS/glibc-build/elf:/home/nuc/graphene_test/graphene/LibOS/glibc-build/dlfcn:/home/nuc/graphene_test/graphene/LibOS/glibc-build/nss:/home/nuc/graphene_test/graphene/LibOS/glibc-build/nis:/home/nuc/graphene_test/graphene/LibOS/glibc-build/rt:/home/nuc/graphene_test/graphene/LibOS/glibc-build/resolv:/home/nuc/graphene_test/graphene/LibOS/glibc-build/mathvec:/home/nuc/graphene_test/graphene/LibOS/glibc-build/support:/home/nuc/graphene_test/graphene/LibOS/glibc-build/crypt:/home/nuc/graphene_test/graphene/LibOS/glibc-build/nptl /home/nuc/graphene_test/graphene/LibOS/glibc-build/support/test-container env GCONV_PATH=/home/nuc/graphene_test/graphene/LibOS/glibc-build/iconvdata LOCPATH=/home/nuc/graphene_test/graphene/LibOS/glibc-build/localedata LC_ALL=C  /home/nuc/graphene_test/graphene/LibOS/glibc-build/elf/ld-linux-x86-64.so.2 --library-path /home/nuc/graphene_test/graphene/LibOS/glibc-build:/home/nuc/graphene_test/graphene/LibOS/glibc-build/math:/home/nuc/graphene_test/graphene/LibOS/glibc-build/elf:/home/nuc/graphene_test/graphene/LibOS/glibc-build/dlfcn:/home/nuc/graphene_test/graphene/LibOS/glibc-build/nss:/home/nuc/graphene_test/graphene/LibOS/glibc-build/nis:/home/nuc/graphene_test/graphene/LibOS/glibc-build/rt:/home/nuc/graphene_test/graphene/LibOS/glibc-build/resolv:/home/nuc/graphene_test/graphene/LibOS/glibc-build/mathvec:/home/nuc/graphene_test/graphene/LibOS/glibc-build/support:/home/nuc/graphene_test/graphene/LibOS/glibc-build/crypt:/home/nuc/graphene_test/graphene/LibOS/glibc-build/nptl ${1+"$@"}
    ;;
  *)
    usage
    ;;
esac
