Name:		dotnet-runtime
Version:	3.1
Release:	0
Summary:	Microsoft .NET Runtime
Group:		Development/Languages
License:	MIT
URL:		http://github.com/dotnet/runtime

Source0: %{name}-%{version}.tar.gz
Source1: %{name}.manifest

BuildRequires:  python
BuildRequires:  python-xml
BuildRequires:  pkgconfig(libunwind)
BuildRequires:  pkgconfig(uuid)
BuildRequires:  libicu-devel
BuildRequires:  tizen-release
BuildRequires:  coreutils

BuildRequires:  cmake-3.15
BuildRequires:  llvm >= 3.8
BuildRequires:  llvm-devel >= 3.8
BuildRequires:  clang >= 3.8
BuildRequires:  clang-devel >= 3.8

# do i need this?
BuildRequires:  lldb >= 3.8
BuildRequires:  lldb-devel >= 3.8

BuildRequires:  gettext-tools
BuildRequires:  libopenssl1.1-devel
BuildRequires:  libstdc++-devel
BuildRequires:  pkgconfig(lttng-ust)
BuildRequires:  krb5-devel

BuildRequires: dotnet-build-essentials

Requires: glibc
Requires: libgcc
Requires: libstdc++
Requires: libunwind
Requires: libuuid

# Accelerate python, clang
%ifarch %{arm}
BuildRequires: python-accel-armv7l-cross-arm
BuildRequires: clang-accel-armv7l-cross-arm
%endif

%ifarch aarch64
BuildRequires: python-accel-aarch64-cross-aarch64
BuildRequires: clang-accel-aarch64-cross-aarch64
%endif

%description
Microsoft .NET runtime (CoreCLR, CoreFX).
The CoreCLR repo contains the complete runtime implementation for .NET Core. It includes RyuJIT, the .NET GC, native interop and many other components. It is cross-platform, with multiple OS and CPU ports in progress.


%package devel
Summary: Microsoft .NET runtime Development package
Requires: dotnet-runtime
%description devel
Headers and static libraries...


# define default build type
%if %{undefined build_type}
%define build_type release
%endif

# capitalized (CamelCase) build type
%define Build %(printf "%s" "%{build_type}" | sed 's/^./\U&/')

# number of processors available for running compiler
%define nproc %(getconf _NPROCESSORS_CONF)

%define clang_ver 5.0

# default build options dependent on architecture
%ifarch arm
%define arch armel
%define buildopts %{build_type} arm clang-%{clang_ver} numproc %{nproc}
%else
%define arch %{_arch}
%define buildopts %{build_type} clang numproc %{nproc}
%endif

# additional CFLAGS
%define clr_cflags -cmakeargs '-DCMAKE_VERBOSE_MAKEFILE=ON -DCLR_ADDITIONAL_COMPILER_OPTIONS=-fasynchronous-unwind-tables\\;-gdwarf-2'

# directory, where coreclr will be installed
%define destdir %{_datadir}/%{name}/%{version}

# path to files contained in dotnet-build-essentials
%define dotnet_bin /opt


# add *.pdb files to "debuginfo" package
%package  debuginfo
Summary:  Microsoft .NET debugging info
Group:    Development/Debug
AutoReq:  0
AutoProv: 1
%description debuginfo
This package provides debugging information for %{name} package
(this also includes .pdb files). This package is required for
profiling running CoreCLR with "perf".
%files debuginfo -f debugfiles.list
%global __debug_install_post %{__debug_install_post} \
    find %{buildroot}/%{destdir} -maxdepth 1 -type f -iname '*.pdb' >> debugfiles.list


%prep
%setup -q -n %{name}-%{version}
cp %{SOURCE1} .


%build
# create in source tree dummy git repository with at least one remote
# (required by microsoft's build system)
test -d .git || cat <<. | base64 -d | gunzip | cpio -i
H4sIACTkMl4CA9WUW2wUVRjHT2kx3bW6QLbpGpowrTy4W9wZd2evQWGhCy6lQWv1QXqby5ndoXNZ
z8yS1toETDU8yIMmJkRoIIpREZ9AQo0hKGYb08SYGJs1vhhTI6YGhXhLCMYz5yztzrbGvnpevjlz
fvOd7/9dZva5puY3/Hcy1xuvNzYDACJDzMJ64KxwXrUBmMXnvwfp+T3kXKrct3TOmuIhKNkW5coP
U25dlduwgmN5jpCD4a1HHbKhSiawPVJPsoIC46LCRZVYTOSEiBSVYJRXokmej0FZjMOEnJDjEQDG
GqaPZ7zXmA1g4o/WCd/U8SffemLfgZnfWg/NP3Nu4ys/3e6a3GVeu8pdWWjfvH8Oqa83T301YAx5
bkx35reFtgYHvjwZGVj87thU5pOfvxl6+9MH509sOv/h2VtHFydadny0Jb7lx7/bJz9PnDhbntlb
vnok2rl4aXjznW8vXJ4tX/n4fHq7o2g8VKt9Z2XTSu1FQRol6p//b1Y1FJOwEy42u2pORUK+EKrN
aZbk1FdPsslITJbifETC+RNTAhfnYIwXlaQcT0WSSUWEkE9xvJPTruC+3qe5EQC829Y1kBt+CdBY
mqpxe5djQVCxCHMz4NbmczOsLeSr3fJr4F+7hZAFKMjU563ARVe3tGEbrCdZXbBsiADgubX1jZf4
PuOv9c0stGD7GfUtmYai5sFByURw0OtBsGhaqm2iccVEumAfhshSTYN5lOG8HkXVoG7KEO9sVIJe
jyggZ6MImoV3mpkXNA0HWyrKgg2tu9hBhL+yIdNpIjWvGp34mhLS8GnBtotWmmVxHIWSGJZMnZVN
24BYccmwVR37VKAtFTDaVZODUJpsqFeLpV7ZkNdR+r5LabZyL7YZqlQ1ZDgGQHeubzd+6RQE9Pdl
s9g8ADgsr+fFwfKuOWPmzMgPPX/Nv3bs9o1cE/p6x9jky+9F3nz2pcQX38crgT3v3iQZPe26Z66y
EdsWeo8MLQmpRRunDd9W0vVxxslqmMT3gd9dZT+26+l3uw/09ub6h7Pduf7ep/YC1VBtWr13/LU9
dKq2I5cmaNrl91TttDkMC8ckrSRDwv7Z5u7e+5dZEQmGVIC0I4fClGusxlpzLy41ZUbqGJ+boSND
SCHsnoTWVcjqOBBeDLsz1Y7t6VX5u0PBrXExax0epkdFqqYxe5CpmYeZ7aNhhTzttATdKhl5p2Mf
Yx6JJbko/p2kUkwXF+U4D36rqzbzkFNBVdCCaWa5lsN1qnx1qh7PZrrB/0LJOVfH0f9kG1VCROD6
pJkVRaLfnnR19KtkSqu9UDDNUdIBYGk1VC2l+vsyuf3Zvo6ODrDm9Q/kvbfIAAgAAA==
.

# use cmake >= 3.15 (required by microsoft's build system)
mkdir -p %{buildroot}
ln -sf /opt/cmake-3.15/bin/cmake  %{buildroot}/cmake

# provide `nproc' executable (required by microsoft)
cat <<-"." > %{buildroot}/nproc
	#! /bin/sh
	echo %{nproc}
.
chmod +x %{buildroot}/nproc

# provide compiler wrapper (for --target)
for each in clang-%{clang_ver} clang++-%{clang_ver}; do
cat <<-.  > %{buildroot}/$each
        #! /bin/sh
        exec /usr/bin/$each --target=%{_host} "\$@"
.
chmod +x %{buildroot}/$each
done

# add cmake-3.15 and nproc to PATH
export PATH=%{buildroot}:${PATH}

# configure paths to contents of `dotnet-build-essentials' package
export NUGET_PACKAGES=%{dotnet_bin}/packages
export DOTNET_INSTALL_DIR=%{dotnet_bin}/dotnet

# additional x64 libraries required by `dotnet' binary
export LD_LIBRARY_PATH=%{dotnet_bin}/libicu:%{dotnet_bin}/libopenssl

# supplementary functions (exclude arguments $2..$n in variable $1)
filterout() (
    set +x
    set -- $({ for each in ${!1}; do printf '%%s\n' $each; done; } \
        | eval $(shift; for each; do set -- "$@" "grep -v -- '^$each\$'"; done; shift $(($#/2)); IFS='|'; echo "$*"));
    echo '' "$*"
)

# filter-out some compiler flags (not acceptable by clang)
for var in CFLAGS CXXFLAGS; do
    export $var="$(filterout $var -Wa,-mimplicit-it=thumb -Wl,--hash-style=gnu -Wl,-O1 -Wl,--as-needed)"
done

# fix issue with clang and limits.h
for var in CFLAGS CXXFLAGS; do
    export $var="${!var} -DMB_LEN_MAX=16 -DPATH_MAX=4096 -DLINE_MAX=2048 -DPTHREAD_DESTRUCTOR_ITERATIONS=4 -DIOV_MAX=1024"
done

# fix issue with strerror_r (-D_POSIX_C_SOURCE=200809L)
#for var in CFLAGS CXXFLAGS; do
#    export $var="${!var} -D_GNU_SOURCE=1"
#done

# fix issue with clang-accel
#for var in CFLAGS CXXFLAGS ASFLAGS; do
#    export $var="${!var} --target=%{_host}"
#done

# build coreclr runtime itself
cd src/coreclr
./build.sh %{buildopts} -skipmanagedtools -cmakeargs \
	'-DFEATURE_GDBJIT=TRUE -DFEATURE_NGEN_RELOCS_OPTIMIZATIONS=true -DFEATURE_PREJIT=true -DARM_SOFTFP=true -DFEATURE_ENABLE_NO_ADDRESS_SPACE_RANDOMIZATION=true' %{clr_cflags}
cd ../..

# build native libraries for CoreFX
src/libraries/Native/build-native.sh %{buildopts} %{clr_cflags}

# build managed libraries
./libraries.sh --arch %{arch} --configuration %{build_type} /p:BinPlaceNETCoreAppPackage=true /p:BuildNative=false /p:BuildPackages=false


%install
rm -rf %{buildroot}/%{destdir}

# CoreCLR runtime, including subdirectories
cp -a "artifacts/bin/coreclr/Linux.%{arch}.%{Build}" %{destdir}

# all PDB's should be in same directory as related DLL files
mv %{destdir}/PDB/*.pdb %{destdir}
rmdir %{destdir}/PDB

# unused directory (contains same files as in destdir)
rm -rf ${destdir}/sharedFramework

# CoreFX native libraries (only top level files)
find "artifacts/bin/native/Linux-%{arch}-%{Build}" -maxdepth 1 -type f \
    | (while read f; do test -f "$f" && { echo "Wont overwrite $f!"; false; } || install "$f" %{destdir}; done)

# CoreFX managed libraries (only top level files)
find "artifacts/bin/runtime/netcoreapp5.0-Linux-%{Build}-%{arch}" -maxdeth 1 -type f -not -name '*.so' -not -name '*.a' \
    | (while read f; do test -f "$f" && { echo "Wont overwrite $f!"; false; } || install "$f" %{destdir}; done)

# create symlinks for compatibility
for each in %{version} 2.0.0 2.1.0 2.1.1 2.1.4 3.0.0; do
    ln -s %{destdir} %{_datadir}/dotnet/shared/Microsoft.NETCore.App/$each
done


%files
%manifest %{name}.manifest
# CoreCLR parts
%{destdir}/crossgen
%{destdir}/libclrjit.so
%{destdir}/libcoreclr.so
%{destdir}/libcoreclrtraceptprovider.so
%{destdir}/libdbgshim.so
%{destdir}/libmscordaccore.so
%{destdir}/libmscordbi.so
%{destdir}/System.Private.CoreLib.dll
# CoreFX native libs
%{destdir}/System.Globalization.Native.so
%{destdir}/System.IO.Compression.Native.so
%{destdir}/System.IO.Ports.Native.so
%{destdir}/System.Native.so
%{destdir}/System.Net.Security.Native.so
%{destdir}/System.Security.Cryptography.Native.OpenSsl.so
# CoreFX managed libs
%{destdir}/*.dll

%files devel
%manifest %{name}.manifest
# CoreCLR parts
%{destdir}/libclrgc.so
%{destdir}/libjitinterface.so
%{destdir}/libsuperpmi-shim-collector.so
%{destdir}/libsuperpmi-shim-counter.so
%{destdir}/libsuperpmi-shim-simple.so
%dir %{destdir}/IL
%{destdir}/*.md
%dir %{destdir}/gcinfo
%dir %{destdir}/inc
%dir %{destdir}/lib
%{destdir}/ilasm
%{destdir}/ildasm
%{destdir}/mcs
%{destdir}/superpmi
# CoreFX static libs
%{destdir}/*.a

