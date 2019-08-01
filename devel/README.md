# kisslight devel

Allows for cross-compiling kisslight in Windows (and linux at some point!)

## Windows

---
this will walk through the process to setup a toolchain on windows

### toolchain installation

---
Install sysprog's [RPi toolchain](https://gnutoolchains.com/raspberry/),

Then install sysprog's [SmarTTY](https://sysprogs.com/SmarTTY/download/);

In the install directory for SmarTTY, add the following batch file:

```batch
@echo off
REM Store this in the root directory of SmartTTY application

SmarTTY.exe /UpdateSysroot:..\..\..\path\to\SysGCC\raspberry\arm-linux-gnueabihf\sysroot
```

then add the following to the repo's VSCode task list json file:

```json
{
    // See https://go.microsoft.com/fwlink/?LinkId=733558
    // for the documentation about the tasks.json format
    "version": "2.0.0",
    "tasks": [
        // perhaps you already have the json file
        // just add the two blocks of code below
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

and done.