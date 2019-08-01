# kisslight devel

Allows for cross-compiling kisslight in Windows

Install sysprog's [RPi toolchain](https://gnutoolchains.com/raspberry/),

Then install sysprog's [SmarTTY](https://sysprogs.com/SmarTTY/download/);

In the install directory for SmarTTY, add the following batch file:

```
@echo off
REM Store this in the root directory of SmartTTY application

SmarTTY.exe /UpdateSysroot:..\..\..\path\to\SysGCC\raspberry\arm-linux-gnueabihf\sysroot
```

then add the following to the repo's VSCode task list:

```
{
    // See https://go.microsoft.com/fwlink/?LinkId=733558
    // for the documentation about the tasks.json format
    "version": "2.0.0",
    "tasks": [
        {
            "label": "build kisslight",
            "type": "shell",
            "command": "cd devel; make; cd .."
        },
        {
            "label": "sync sysroot",
            "type": "shell",
            "command": "devel\\sync.cmd"
        }
    ]
}
```

and done, I plan to write a wiki on this subject at some point.