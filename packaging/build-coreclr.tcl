#!/bin/sh
# Run this script to build coreclr. \
exec tclsh "$0" "$@"

# Additional options which passed to CMake to build CoreCLR on Tizen:
set CORECLR_CMAKE_ARGS [list \
    -DFEATURE_GDBJIT=TRUE \
    -DFEATURE_NGEN_RELOCS_OPTIMIZATIONS=true \
    -DFEATURE_PREJIT=true \
    -DARM_SOFTFP=true \
    -DFEATURE_ENABLE_NO_ADDRESS_SPACE_RANDOMIZATION=true \
]

set CFLAGS {}
set LDFLAGS {}
set buildopts {}

set result "coreclr.zip"
set zipopt "-jy1"

set bindeps ""

proc lgrep {list args} {eval concat [lmap v $args {lsearch -inline $list $v}]}

if {[lgrep $::argv -h -help --help help] ne {}} {
    puts "Usage: $::argv0 ?options?..."
    puts "Acceptable options are:"
    puts "--clean                    -- rebuild rootfs, erase caches"
    puts "--out <file>               -- set name of output archive"
    puts "--bin <file>               -- set name for binary dependencies"
    puts "--skipbuild                -- only pack artefacts"
    puts "clang* | gcc*              -- the name of the compiler"
    puts "release | checked | debug  -- build type"
    puts "cross                      -- perform cross-platform build for TIZEN"
    puts "arm64 | armel | x86 | x64  -- architecture"
    puts "-verbose                   -- verbose build"
    puts "cmakeargs <arg>            -- additional cmake arguments"
    puts "CFLAGS | CXXFLAGS <flags>  -- additional compiler flags"
    puts "LDFLAGS <flags>            -- additional linker flags"
    puts ""
    puts "If some option requires an argument, it must follow the option (as next arg)."
    puts "All other arguments will be passed to M$ build system as is."
    puts "Default architecture is 'armel', default build type is 'release'."
    puts "Default output archive name is '$result'."
    puts "By default binary dependencies not packed."
    exit 0
}

# build `buildoptions' array from the command line arguments:
# (cut CFLAGs, CXXFLAGS and LDFLAGS options with following arguments)
for {set i 0} {$i < [llength $::argv]} {incr i} {
    set opt [lindex $::argv $i]

    switch -glob $opt {
        {CFLAGS} - {CXXFLAGS} - {LDFLAGS} {
            incr i
            set opt [expr {$opt eq {CXXFLAGS} ? {CFLAGS} : $opt}]
            set $opt [concat [set $opt] [lindex $::argv $i]]
        }
        {CFLAGS=*} - {CXXFLAGS=*} - {LDFLAGS=*} {
            regexp {([^=]*)=(.*)} $opt {} opt val
            set opt [expr {$opt eq {CXXFLAGS} ? {CFLAGS} : $opt}]
            set $opt [concat [set $opt]  $val]
        }
        {--clean} {}
        {--out=*} {regexp {([^=]*)=(.*)} $opt {} {} ::result}
        {--out} {
            incr i
            set ::result [lindex $::argv $i]
        }
        {--bin=*} {regexp {([^=]*)=(.*)} $opt {} {} ::bindeps}
        {--bin} {
            incr i
            set ::bindeps [lindex $::argv $i]
        }
        {--skipbuild} {
            set ::skipbuild {}
        }
        {--*} {
            puts stderr "Unknown option: $opt"
            exit 1
        }
        default {lappend buildopts $opt}
    }
}

if {$result eq {} && $bindeps eq {}} {
    puts stderr "No output file(s) set!"
    exit 1
}

# set default architecture and build type
if ![llength [lgrep $buildopts arm64 arm armel x86 x64]] {lappend buildopts armel}
if ![llength [lgrep $buildopts release checked debug]] {lappend buildopts release}

# extract architecture and build type from `buildopts' list
set arch [lgrep $buildopts arm64 arm armel x86 x64]
set build [lgrep $buildopts release checked debug]

# use `cross' build for other than x64 architectures
if {$arch ne {x64}} {lappend buildopts cross}

# set default compiler as `clang'
if ![llength [eval concat [lmap v {clang* gcc*} {lsearch -glob $buildopts $v}]]] {lappend buildopts clang}

# make additional `-cmakeargs' arguments which should be passed to M$ build system
set CFLAGS [list -cmakeargs -DCLR_ADDITIONAL_COMPILER_OPTIONS="[join $CFLAGS {\;}]"]
set LDFLAGS [list -cmakeargs -DCLR_ADDITIONAL_COMPILER_OPTIONS="[join $LDFLAGS {\;}]"]

# how to run external process
proc spawn {args} {
    puts "exec $args"
    eval exec -ignorestderr -- $args >&@stdout
}

# configure microsoft's paths
set ::env(NUGET_PACKAGES) "[pwd]/.packages"
set ::env(DOTNET_INSTALL_DIR) "[pwd]/.dotnet"
set ::env(HOME) [pwd]

# perform full cleanup, if requested.
if {[lsearch $::argv --clean] >= 0} {
    puts "Erasing root-fs"
    spawn ./build.sh --clean
    file delete -force -- $::env(NUGET_PACKAGES) $::env(DOTNET_INSTALL_DIR)
    spawn sudo rm -rf .tools/rootfs
}

# build root-fs if needed
if {[lsearch $::argv cross] >= 0} {
    set arch [array names argnames -regexp {^(armel|x86)$}]
    if ![llength $arch] {
        puts stderr "Unsuppoted Tizen architecture for cross build."
        exit 1
    }

    set rootfs ".tools/rootfs/$arch"
    if [file isdirectory $rootfs] {
        puts "Skipping root-fs creation (remove '$rootfs' to rebuild root-fs."
    } else {
        echo "Building root-fs for cross build"
        spawn sudo ./eng/common/cross/build-rootfs.sh $arch tizen
    }
}

# remove build artefact from previous build (THIS IS REQUIRED, DON'T REMOVE!)
if ![info exists skipbuild] {spawn ./build.sh --clean}

# build CoreCLR's runtime
if ![info exists skipbuild] {
    cd src/coreclr
    eval spawn ./build.sh $buildopts -skipmanagedtools -cmakeargs [list $CORECLR_CMAKE_ARGS] $CFLAGS
    cd ../..
}

# pack results
if {$result ne {}} {
    # remove old archive
    file delete -force -- $result

    set Build [string toupper $build 0]
    set dir "artifacts/bin/coreclr/Linux.$arch.$Build"
    set files [glob -directory $dir -types f *]
    set files [eval concat [lmap v $files {expr {[string match *.a $v] ? {} : $v}}]]
    eval spawn zip $zipopt [list $result] $files
}

# build native code for CoreFX
if ![info exists skipbuild] {
    eval spawn src/libraries/Native/build-native.sh $buildopts $CFLAGS
}

# pack results
if {$result ne {}} {
    set dir "artifacts/bin/native/Linux-$arch-$Build"
    set files [glob -directory $dir -types f *]
    set files [eval concat [lmap v $files {expr {[string match *.a $v] ? {} : $v}}]]
    eval spawn zip $zipopt [list $result] $files
}

# build managed code for CoreFX
# (this command also rebuilds native code for CoreFX, so it should be packed before)
if ![info exists skipbuild] {
    spawn ./libraries.sh --arch $arch --configuration $build /p:BinPlaceNETCoreAppPackage=true
}

# pack results
if {$result ne {}} {
    set dir "artifacts/bin/runtime/netcoreapp5.0-Linux-$Build-$arch"
    set files [glob -directory $dir -types f *]
    set files [eval concat [lmap v $files {expr {[regexp {[.](a|so)$} $v] ? {} : $v}}]]
    eval spawn zip $zipopt [list $result] $files
}

# pack microsofts binaries
if {$bindeps ne {}} {
    #spawn $::env(DOTNET_INSTALL_DIR)/dotnet tool install coverlet.console
    file delete -force -- $bindeps
    set packer [expr {[catch {exec sh -c "type pixz"}] ? {xz -9} : {pixz -9}}]
    set dirs [lmap v [list $::env(NUGET_PACKAGES) $::env(DOTNET_INSTALL_DIR)] {file tail $v}]
    spawn sh -c "tar -c --xform 's,^\\.,,' $dirs | $packer > $bindeps"
}

