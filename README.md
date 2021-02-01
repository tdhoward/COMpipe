# COMpipe
Links a serial COM port to a named pipe.  This command-line utility was developed to allow Hyper-V virtual machines to use the host machine's COM ports.  It currently runs in the console on Windows only, and has been tested on Windows 10 Pro.

A compiled executable can be found in bin\Release.

Feel free to modify the source code and use as desired.  It was compiled using MinGW32 and Code:Blocks IDE.  No additional libraries are needed.

```
Usage:
  COMpipe [-b <baud rate>] -c <COM port name> -p <pipe name>
  Examples:
    COMpipe -c \\.\COM8 -p \\.\pipe\MyLittlePipe
    COMpipe -b 19200 -c \\.\COM8 -p \\.\pipe\MyLittlePipe
  Notes:
  1. COMpipe does not create a named pipe, it only uses an existing named pipe.
  2. COMpipe must be run as administrator.
  3. The default baud rate is 9600.
  Options include: 4800, 9600, 14400, 19200, 38400, 57600, and 115200.
```
