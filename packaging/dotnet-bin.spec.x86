Name:       dotnet-bin
Version:    1
Release:    0
Summary:    Binary files required for building .NET runtime
Group:      Development/Languages
License:    MIT
URL:        http://github.com/dotnet/runtime

%description
Binary files required for building .NET runtime.

%define archive dotnet-bin.tar.xz

%build
mkdir -p %{buildroot}/opt
rm -rf artifacts
packaging/build-coreclr.tcl --out "" --bin %{buildroot}/opt/%{archive}

%post
cd /opt
xz -d < %{archive} | tar -x
rm %{archive}

%files
/opt/%{archive}
