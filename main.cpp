#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <string>
#include <vector>
#include <algorithm>

#define BUFSIZE 512


class InputParser{
    public:
        InputParser (int &argc, char **argv){
            for (int i=1; i < argc; ++i)
                this->tokens.push_back(std::string(argv[i]));
        }
        const std::string& getCmdOption(const std::string &option) const{
            std::vector<std::string>::const_iterator itr;
            itr =  std::find(this->tokens.begin(), this->tokens.end(), option);
            if (itr != this->tokens.end() && ++itr != this->tokens.end()){
                return *itr;
            }
            static const std::string empty_string("");
            return empty_string;
        }
        bool cmdOptionExists(const std::string &option) const{
            return std::find(this->tokens.begin(), this->tokens.end(), option)
                   != this->tokens.end();
        }
    private:
        std::vector <std::string> tokens;
};


int main(int argc, char **argv)
{
    HANDLE hPipe;
    HANDLE serialHandle;

    // buffers for sending and receiving.  Note: IN/OUT is from the perspective of the named pipe.
    DWORD out_bytes=0;  // number of bytes waiting (in out_buffer) to be sent through the pipe
    char out_buffer[BUFSIZE+1];
    DWORD in_bytes=0;  // number of bytes that have been received into in_buffer from the pipe
    char in_buffer[BUFSIZE+1];

    BOOL   fSuccess = FALSE;
    DWORD  cbRead, cbWritten, dwMode;


    // Handle command line parameters
    if(argc < 2)
    {
        printf("Usage:\n");
        printf("  COMpipe [-b <baud rate>] -c <COM port name> -p <pipe name>\n\n");
        printf("Examples:\n");
        printf("  COMpipe -c \\\\.\\COM8 -p \\\\.\\pipe\\MyLittlePipe\n");
        printf("  COMpipe -b 19200 -c \\\\.\\COM8 -p \\\\.\\pipe\\MyLittlePipe\n\n");
        printf("Notes:\n  1. COMpipe does not create a named pipe, it only uses an existing named pipe.\n");
        printf("  2. COMpipe must be run as administrator.\n");
        printf("  3. The default baud rate is 9600.\n");
        printf("      Options include: 4800, 9600, 14400, 19200, 38400, 57600, and 115200. \n\n");
        return 0;
    }

    // review command line parameters for pipe and COM port names
    InputParser input(argc, argv);

    const std::string &pipe_name = input.getCmdOption("-p");
    if (pipe_name.empty())
    {
        printf("Please include the name of the pipe.\n");
        return -1;
    }

    const std::string &serial_port = input.getCmdOption("-c");
    if (serial_port.empty())
    {
        printf("Please include the name of the serial port.\n");
        return -1;
    }

    std::string baud_rate = input.getCmdOption("-b");
    if (baud_rate.empty())
        baud_rate = "9600";
    int baud = std::stoi(baud_rate);

    // attempt to open the COM port
    serialHandle = CreateFile(serial_port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if(serialHandle==INVALID_HANDLE_VALUE)
    {
        if(GetLastError()==ERROR_FILE_NOT_FOUND)
        {
            //serial port does not exist. Inform user.
            printf("COM port does not exist!");
            return -1;
        }
        //some other error occurred. Inform user.
        printf("An error occurred! GLE=%lu\n", GetLastError());
        return -1;
    }

    // Do some basic settings
    DCB serialParams = { 0 };
    serialParams.DCBlength = sizeof(serialParams);

    if(!GetCommState(serialHandle, &serialParams))
    {
        printf("Error getting serial port state! GLE=%lu\n", GetLastError());
        return -1;
    }
    serialParams.BaudRate = baud;
    serialParams.ByteSize = 8;
    serialParams.StopBits = ONESTOPBIT;
    serialParams.Parity = NOPARITY;
    if(!SetCommState(serialHandle, &serialParams))
    {
        printf("Error setting serial port state! GLE=%lu\n", GetLastError());
        return -1;
    }

    // Set timeouts
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 10;
    timeouts.ReadTotalTimeoutConstant = 1;
    timeouts.ReadTotalTimeoutMultiplier = 1;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if(!SetCommTimeouts(serialHandle, &timeouts))
    {
        printf("Error setting serial port timeouts! GLE=%lu\n", GetLastError());
        return -1;
    }

    printf("Serial port opened successfully.\n");


    // Try to open a named pipe; wait for it, if necessary.
    while (1)
    {
        hPipe = CreateFile(
                    pipe_name.c_str(),   // pipe name
                    GENERIC_READ |  // read and write access
                    GENERIC_WRITE,
                    0,              // no sharing
                    NULL,           // default security attributes
                    OPEN_EXISTING,  // opens existing pipe
                    0,              // default attributes
                    NULL);          // no template file

        // Break if the pipe handle is valid.
        if (hPipe != INVALID_HANDLE_VALUE)
            break;

        // Exit if an error other than ERROR_PIPE_BUSY occurs.
        if (GetLastError() != ERROR_PIPE_BUSY)
        {
            printf( "Could not open pipe. GLE=%lu\n", GetLastError() );
            return -1;
        }

        // All pipe instances are busy, so wait for 20 seconds.
        if ( ! WaitNamedPipe(pipe_name.c_str(), 20000))
        {
            printf("Could not open pipe: 20 second wait timed out.");
            return -1;
        }
    }

    // The pipe connected; Set up the mode.
    dwMode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
    fSuccess = SetNamedPipeHandleState(
        hPipe,    // pipe handle
        &dwMode,  // new pipe mode
        NULL,     // don't set maximum bytes
        NULL);    // don't set maximum time
    if ( ! fSuccess)
    {
        printf( "SetNamedPipeHandleState failed. GLE=%lu\n", GetLastError() );
        return -1;
    }

    printf("Named pipe opened successfully.\n");
    printf("Press Q to quit.\n");


    // enter the main loop   ------------------------------------
    while (1)
    {
        // check if we should be exiting the loop
        if(kbhit() && GetKeyState('Q') < 0)  // Q key exits the loop
        {
            printf("Quitting...");
            break;
        }

        // go read our serial stream (up to 32 bytes at a time, so we don't block too long.)
        if(!ReadFile(serialHandle, out_buffer, 32, &out_bytes, NULL))
        {
            //error occurred. Report to user.
            printf( "Error reading serial port! GLE=%lu\n", GetLastError() );
            return -1;
        }

        // send what we read
        if(out_bytes > 0)
        {
            // send anything in the out_buffer
            fSuccess = WriteFile(
                           hPipe,                  // pipe handle
                           out_buffer,             // message
                           out_bytes,              // message length
                           &cbWritten,             // bytes written
                           NULL);                  // not overlapped
            // TODO: check on cbWritten
            if ( ! fSuccess)
            {
                printf( "WriteFile to pipe failed. GLE=%lu\n", GetLastError() );
                return -1;
            }
        }

        // Read from the pipe.
        fSuccess = ReadFile(
                       hPipe,    // pipe handle
                       in_buffer,    // buffer to receive reply
                       BUFSIZE,  // size of buffer
                       &cbRead,  // number of bytes read
                       NULL);    // not overlapped

        // TODO: figure out a better way to tell when we had an error
        if ( !fSuccess && GetLastError() != ERROR_NO_DATA)  // we weren't able to read any more because of an error
        {
            printf( "ReadFile from pipe failed. GLE=%lu\n", GetLastError() );
            break;
        }

        in_bytes = cbRead;
        in_buffer[in_bytes]=0;  // null termination is not really necessary

        // send it to the serial stream
        if(!WriteFile(serialHandle, in_buffer, in_bytes, &cbWritten, NULL))
        {
            //error occurred. Report to user.
            printf( "Error writing to serial port! GLE=%lu\n", GetLastError() );
            return -1;
        }

        /*
        // create an echo:
        for(int i=0;i<in_bytes;i++)
            out_buffer[i] = in_buffer[i];
        out_bytes = in_bytes;
        out_buffer[out_bytes]=0;
        */
    }

    while(kbhit()) getch();  // clear the input buffer

    CloseHandle(hPipe);
    CloseHandle(serialHandle);

    return 0;
}
