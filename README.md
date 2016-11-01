# mbed-os: u-blox Test Application

This repo contains an application that pulls together all the bits of the u-blox integration of the new mbed 5 release and builds something runnable for Sara-N.

# Prerequisites
To fetch and build the code in this repository you need first to install the [mbed CLI tools](https://github.com/ARMmbed/mbed-cli#installation) and their prerequisites.  This will include a compiler (you can use GCC_ARM, ARM or IAR) and you will need to use `mbed config` to tell the mbed CLI tools where that compiler is. 

# Repo Structure
The repo structure is quite confusing; it goes like this:

* What you see here is the top level. It contains only the test application.
* Look for the vital little file `mbed-os.lib`.  It really is vital.
* Because github can't do references to repos from within repos (like good 'ole SVN did), ARM have invented these `.lib` files.  The name is very confusing as all the file contains is a link to another repo (`lnk` would have been a better file extension to choose).  When you use the [mbed command line tools](https://github.com/ARMmbed/mbed-cli) or the mbed on-line IDE it understands these `.lib` files.  So the effect of `mbed-os.lib` is to tell the mbed tools to go get the github URL inside the `mbed-os.lib` file and put it in the sub-directory `mbed-os`.  In the on-line IDE this is called a library, hence the confusing name.
* So, you MUST use the [mbed command line tools](https://github.com/ARMmbed/mbed-cli) to sort everything out for you.  For instance, to create a new mbed-os application for yourself, let's call it `my-app`, create the `my-app` directory, `cd` to it and then (assuming you have the mbed CLI tools installed) enter `mbed new .`.  This will go and get all of the latest mbed-os release and put it into the correct sub-directories.  All you need to do then is add your application file(s) to the top-level directory and you have a code tree which should compile and run 'on' `mbed-os`.
* Our Sara-N target has been merged into `mbed-os` but not yet back ported to an `mbed-os` release, so you can't currently just do `mbed new .`, instead you need to do:

`mbed add https://github.com/ARMmbed/mbed-os`

...to obtain `mbed-os`.

# Building This Code
You need to set the target and the toolchain that you want to use.  The target and toolchain we'll use with this application is `SARA_NBIOT_EVK` and we will chose the toolchain `ARM`, though note that `GCC_ARM` and `IAR` toolchains are also supported.  To get a list of supported targets and their toolchains enter `mbed compile -S`.

You can set the target and toolchain for this application once by entering the following two commands (while in the top-level directory of the cloned repo):

`mbed target SARA_NBIOT_EVK`

`mbed toolchain ARM`

Once this is done, build the code with:

`mbed compile`

You will find the output files in the sub-directory `.build\SARA_NBIOT_EVK\ARM\`.

# Other Things

* There is a file `mbed-settings.py` in the top-level repo directory.  This is where you make local changes to how you want the code built.  The only non-default setting in this particular file is to change:

  `BUILD_OPTIONS = []`

  to

  `BUILD_OPTIONS = ["debug-info"]`

  ...which will get debug output into the `.elf` file and switch optimisation off so that you can use a debugger.
* Eclipse project files are included but you can also build from the command-line as above.