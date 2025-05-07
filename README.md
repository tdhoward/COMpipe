# COMpipe
Links a serial COM port to a named pipe.  This command-line utility was developed to allow Hyper-V virtual machines to use the host machine's COM ports. Hyper-V provides a named pipe that maps into the VM as a COM port. It does not connect that named pipe to the host's COM port, which is what you would need to pass the data through to your VM.  This utility currently runs in the console on Windows only, and has been tested on Windows 8 Pro, 10 Pro, and 11 Pro.  If you need to run this as a Windows service, you can use something like [NSSM](https://nssm.cc/), which has been used with COMpipe successfully.

A compiled executable can be found in x64\Release.

Feel free to modify the source code and use as desired.  It was compiled using Visual Studio 2022.  No additional libraries are needed.

```
Usage:
  COMpipe [-b <baud rate>] [-d <data bits>] [-r <parity>] [-s <stop bits>] -c <COM port name> -p <pipe name>

Examples:
  COMpipe -c \\.\COM8 -p \\.\pipe\MyLittlePipe
  COMpipe -b 19200 -c \\.\COM8 -p \\.\pipe\MyLittlePipe

Notes:
  1. COMpipe does not create a named pipe, it only uses an existing named pipe.
  2. COMpipe must be run as administrator.
  3. The default baud rate is 9600.
      Options typically include: 4800, 9600, 14400, 19200, 38400, 57600, and 115200.
  4. The default number of data bits is 8. Allowable options are: 4-8
  5. The default parity is 0 (None).
      Options are: 0=None, 1=Odd, 2=Even, 3=Mark, 4=Space.
  6. The default stop bits value is 0 (1 stop bit).
      Options are: 0=1, 1=1.5, 2=2
  7. Hardware signals like RTS/CTS, DTR/DSR, DCD, and RI are not supported.
      This is because the named pipe created by the VM does not support these signals.
```

*Changelog*
Version 0.3 (07-May-2025)
Added command-line options for data bits, parity, and stop bits.

Version 0.2 (20-July-2023)
Added automatic retries and exponential backoff for when a connection to a named pipe or serial port is temporarily lost.  This can happen when rebooting the virtual machine, for example.  Also, other miscellaneous updates.

