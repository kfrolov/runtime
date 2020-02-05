# Building DOTNET runtime (CoreCLR, CoreFX, etc...) for Tizen

In common, following scheme is used:

    ,--------------.      ,---------------.
    | 1) libicu    |      | 2) libopenssl |
    |    x86 rpm   |      |    x86 rpm    |
    |              |      |               |
    | produced by  |      | produced by   |
    | rpmbuild on  |      | rpmbuild on   |
    | the host     |      | the host      |
    `--------------'      `---------------'
           |                    |
           \                    |
            \                  /   libopenssl-x86, libicu-x86 and dotnet-bin-x86.rpm
             \                /    should be commited in repository from which
              |              |     dotnet-build-essentials-noarch.rpm will be built (at step 4)
              V              V
        ,---------------------------.            ,------------------------.
        | 4) dotnet-build-essentials|<-----------| 3) dotnet-bin  x86 rpm |
        |    noarch rpm             |            |                        |
        |                           |            |  produced by rpmbuild  |
        | produced by gbs           |            |  on the host           |
        | in tizen's build root     |            `------------------------'
        `---------------------------'
                   |
                   |
                   | dotnet-bin-noarch.rpm is the build requirement
                   |
                   V
         ,---------------------------.
         |   5) dotnet-runtime       |
         |      armel rpm            |
         |                           |
         | produced by gbs in        |
         | tizen's build root        |
         `---------------------------'

This README file describes only steps 3 (bulding dotnet-bin-x86.rpm) and 5 (building dotnet-runtime-armel.rpm).

To see how to build other packages see packaging/README.md in appropriate repository.

*Both, dotnet-bin-x86.rpm and dotnet-runtime-armel.rpm packages must be built **from same single repository, from same single revision**.*

## How to buld dotnet-bin-x86.rpm

Checkout the source from git and give the command:

    rpmbuild -v -bb packaging/dotnet-bin.spec  --build-in-place

Then you can get resulting package at the path: `$HOME/RPMS/x86_64/dotnet-bin-1-0.x86_64.rpm`.

You should commit this package to repository (this is separate repository) from which dotnet-build-essentials-noarch.rpm do builts.

## How to build dotnet-runtime-armel.rpm

Checkout same revision as on previous step (or use already checked out source tree) and give the command:

    gbs build -A armv7l -P armel --overwrite


