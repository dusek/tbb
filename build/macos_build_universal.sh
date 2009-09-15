#!/bin/sh
# Usage: /path/to/macos_build_universal.sh <make arguments>
# example: /path/to/macos-build-universal.sh compiler=icc tbb_root=...
# if you want to specify different set of architectures than the default "ia32 intel64",
# do so with the "archs" environment variable: archs="ia32 intel64 ppc32" /path/to/macos-build-universal.sh
thisdir="`dirname "$0"`"
builddir="$thisdir"
rootdir="$builddir/.."
archs=${archs:-ia32 intel64}
configs="debug release"

tbblibs="tbb tbbmalloc"

log() {
    echo "$@"
    "$@"
}

# build
for arch in $archs; do
    gnumake -C "$rootdir" "arch=$arch" "$@"
done

# merge archs
for config in $configs; do
    config_name=""
    [ $config != "debug" ] || config_name="_debug"
    for tbblib in $tbblibs; do
        tbblibname=lib${tbblib}${config_name}.dylib
        lipo_lib_args=""
        for arch in $archs; do
            config_dir="`ls -d "${builddir}"/macos*${arch}*${config} | head -n1`"
            lipo_lib_args="${lipo_lib_args} ${config_dir}/${tbblibname}"
        done
        log lipo ${lipo_lib_args} -create -output "${builddir}/${tbblibname}"
    done
done
