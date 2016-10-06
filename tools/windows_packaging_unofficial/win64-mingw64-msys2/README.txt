How To Compile Ardour on Windows with MinGW64 and MSYS2

----------

Tested on Windows 10 64-bit at Ardour version 5.4 
(commit bb3312c3bb9c6ed9b75ac6739a6ee720ddf86c86) on 2016-10-03

Note: These are unofficial and unsupported instructions, and they use Jack2 
on Windows, which is also an unsupported configuration. Following this will 
modify the Ardour source code in ways which may break Linux compatibility. 
Or not. I didn't test it on Linux after making the changes. 

This method is possible thanks to this earlier blog post by Guy Sherman, and 
Final Fantasy XIII running too slow on Wine: 
https://guysherman.com/2015/08/16/building-ardour-on-windows-with-msys2/

----------


Quickstart:

There are a few steps which I don't know how to automate:

Install ASIO4ALL, Jack2 64-bit, and MSYS2, and add the MSYS2 /usr/bin dir to
your Windows PATH.

Now the rest is automatic. Run this script in an MSYS2 shell. You can run it 
directly from the web without needing to clone this git repository first:

wget -O - https://raw.githubusercontent.com/Ardour/ardour/master/tools/windows_packaging_unofficial/win64-mingw64-msys2/00-01-prepare.sh | sh

After that, run this other script in a MinGW64 MSYS2 shell:

wget -O - https://raw.githubusercontent.com/Ardour/ardour/master/tools/windows_packaging_unofficial/win64-mingw64-msys2/00-02-install.sh | sh

If it succeeds, you're done!

You can run ardour by first starting Jack with 
QJackCtl, and then running this from the MinGW64 MSYS2 shell:

cd /ardour-mingw64-msys2/ardour/gtk2_ardour
./ardev-win


----------


Long Version:

Install ASIO4ALL ( http://www.asio4all.com ) 
and JACK2 64-bit ( http://jackaudio.org/downloads/ )


Start Jack Control, a.k.a. QJackCtl a.k.a. JACK Audio Connection Kit,
and click Setup.

Set it on Driver: portaudio, and click the > next to Interface:, selecting
ASIO::ASIO4ALL v2.

Depending on your system, you could reduce the Frames/Period to maybe 512,
reducing latency, without getting xruns.

Click Ok, and then try starting up Jack by clicking the Start button
in QJackCtl. If it succeeds, it should say Started on the interface,
and you should briefly see a popup message from ASIO4ALL.

You can stop Jack and close QJackCtl now.


Install MSYS2 64-bit ( https://msys2.github.io )

Open the MSYS2 shell and install a bunch of dependencies with pacman by
running this script: 

./01-bin-deps.sh


In the MSYS2 shell, make a new directory for your downloads, then clone
a git repo that has pkgbuilds for more source dependencies we need, and
finally install the dependencies:

./02-msys2-deps.sh


Download the Ardour git repository: 

./03-get.sh


Patch Ardour's source code so it compiles on Windows: 

./04-patch.sh


Now close your MSYS2 shell and open a MinGW64 MSYS2 shell, then configure and
build ardour: 

./05-build.sh


Install some final source dependencies:
 
./06-mingw64-deps.sh


If it all succeeds, WOW! Now you can run ardour by first starting Jack with 
QJackCtl, and then running this from the MinGW64 MSYS2 shell:

cd /ardour-mingw64-msys2/ardour/gtk2_ardour
./ardev-win


----------


Other Examples:


Run a set of locally cached build scripts instead of downloading fresh ones:

wget -O - https://raw.githubusercontent.com/Ardour/ardour/master/tools/windows_packaging_unofficial/win64-mingw64-msys2/00-01-prepare.sh | ARG1="-l" sh
wget -O - https://raw.githubusercontent.com/Ardour/ardour/master/tools/windows_packaging_unofficial/win64-mingw64-msys2/00-02-install.sh | ARG1="-l" sh

*If you're downloading fresh scripts and it seems like the older versions are 
running instead of the new ones, remove any existing build scripts in your 
current working directory and try again.*


Use the build scripts from Defcronyke's fork repository, they may be newer 
than the ones currently available in the official Ardour repository:

wget -O - https://raw.githubusercontent.com/defcronyke/ardour/win64-mingw-msys/tools/windows_packaging_unofficial/win64-mingw64-msys2/00-01-prepare.sh | ARG1="-d" sh
wget -O - https://raw.githubusercontent.com/defcronyke/ardour/win64-mingw-msys/tools/windows_packaging_unofficial/win64-mingw64-msys2/00-02-install.sh | ARG1="-d" sh


----------


Let me know how it goes, and if it breaks I'll happily accept pull requests 
which fix this process.

This build system was created for Ardour, but it is very customizable and 
could be used for building any other MSYS2-compatible software. See 
00-00-conf.sh and 00-01-prepare.sh for shell environment variables which 
you can set before running the scripts to modify their behaviour.

~Jeremy Carter <Jeremy@JeremyCarter.ca> 2016
