#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <functional>
#include <chrono>
#include <thread>
#include <cmath>
#include <exception>

using namespace std;

#define BUFSIZE 512
HANDLE hPipe = INVALID_HANDLE_VALUE;
HANDLE serialHandle = INVALID_HANDLE_VALUE;



void ClearKeyboardBuffer()
{
    while (_kbhit()) _getch();  // clear the input buffer
}


bool CheckUserCancellation()
{
    return (_kbhit() && GetKeyState('Q') < 0);
}


void Shutdown(int returnvalue)
{
    ClearKeyboardBuffer();

    CloseHandle(hPipe);
    CloseHandle(serialHandle);
    exit(returnvalue);
}


// delays in increments of 100ms while checking for user input
void CheckUserAndSleep(int delayInMs)
{
    std::chrono::milliseconds delay(100);
    while (delayInMs >= 100)
    {
        std::this_thread::sleep_for(delay);
        delayInMs -= 100;
        if (CheckUserCancellation())
            Shutdown(0);
    }
}


/// <summary>
/// Calls function "func" with arguments "args", performing retries and exponential backoff.
/// If maxRetries = -1, it will retry forever, and there is no backoff.
/// </summary>
/// <typeparam name="Function"></typeparam>
/// <typeparam name="...Args"></typeparam>
/// <param name="func">Function to be called</param>
/// <param name="maxRetries">Maximum number of retries before giving up.  Set to -1 for retrying forever.</param>
/// <param name="initialDelayInMs">Starting delay in milliseconds.  This is exponentially backed off, unless maxRetries = -1.</param>
/// <param name="...args">Variable number of arguments to be passed to func.</param>
/// <returns></returns>
template<typename Function, typename... Args>
auto RetryWithBackoff(Function func, int maxRetries, int initialDelayInMs, Args... args) -> typename std::result_of<Function(Args...)>::type {
    int delay = initialDelayInMs;
    int retries = 0;

    while (true) {
        try {
            return func(args...);
        }
        catch (const std::exception& e) {
            if (retries >= maxRetries && maxRetries != -1) {
                throw; // After maxRetries attempts, rethrow the last exception
            }

            std::cout << "Exception caught: " << e.what() << ", retrying after " << delay << " milliseconds...\n";

            // Backoff delay before next retry
            CheckUserAndSleep(delay);

            // Exponential backoff
            if(maxRetries != -1)
                delay *= 2;
            retries++;
        }
        catch (...) {
            // If the exception is not derived from std::exception, 
            // we won't be able to print its message, but we can still retry.
            if (retries >= maxRetries && maxRetries != -1) {
                throw; // After maxRetries attempts, rethrow the last exception
            }

            std::cout << "Unknown exception caught, retrying after " << delay << " milliseconds...\n";

            // Backoff delay before next retry
            CheckUserAndSleep(delay);

            // Exponential backoff
            if (maxRetries != -1)
                delay *= 2;
            retries++;
        }
    }
}



class InputParser{
    public:
        InputParser (int &argc, char **argv){
            for (int i=1; i < argc; ++i)
                this->tokens.push_back(string(argv[i]));
        }
        const string& getCmdOption(const string &option) const{
            vector<string>::const_iterator itr;
            itr =  find(this->tokens.begin(), this->tokens.end(), option);
            if (itr != this->tokens.end() && ++itr != this->tokens.end()){
                return *itr;
            }
            static const string empty_string("");
            return empty_string;
        }
        bool cmdOptionExists(const string &option) const{
            return find(this->tokens.begin(), this->tokens.end(), option)
                   != this->tokens.end();
        }
    private:
        vector <string> tokens;
};


void OpenSerialPort(const string serial_port, int baud)
{
    char error[400];
    DWORD gle = 0;
    // attempt to open the COM port
    serialHandle = CreateFileA(serial_port.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (serialHandle == INVALID_HANDLE_VALUE)
    {
        gle = GetLastError();
        switch(gle)
        {
        case ERROR_FILE_NOT_FOUND:
            throw exception("COM port does not exist!");
        case ERROR_ACCESS_DENIED:
            throw exception("Access denied while trying to open COM port!");
        default:
            sprintf_s(error, "An error occurred! GLE=%lu", GetLastError());
            throw exception(error);
        }
    }

    // Do some basic settings
    DCB serialParams = { 0 };
    serialParams.DCBlength = sizeof(serialParams);

    if (!GetCommState(serialHandle, &serialParams))
    {
        sprintf_s(error,"Error getting serial port state! GLE=%lu", GetLastError());
        throw exception(error);
    }
    serialParams.BaudRate = baud;
    serialParams.ByteSize = 8;
    serialParams.StopBits = ONESTOPBIT;
    serialParams.Parity = NOPARITY;
    if (!SetCommState(serialHandle, &serialParams))
    {
        sprintf_s(error, "Error setting serial port state! GLE=%lu", GetLastError());
        throw exception(error);
    }

    // Set timeouts
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 10;
    timeouts.ReadTotalTimeoutConstant = 1;
    timeouts.ReadTotalTimeoutMultiplier = 1;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(serialHandle, &timeouts))
    {
        sprintf_s(error, "Error setting serial port timeouts! GLE=%lu", GetLastError());
        throw exception(error);
    }

    printf("Serial port opened successfully.\n");
}


void OpenNamedPipe(const string pipe_name)
{
    char error[400];
    DWORD gle = 0;
    // Try to open a named pipe; wait for it, if necessary.
    while (1)
    {
        hPipe = CreateFileA(
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
        gle = GetLastError();
        if (gle != ERROR_PIPE_BUSY)
        {
            sprintf_s(error,"Could not open pipe. GLE=%lu", gle);
            throw exception(error);
        }

        // All pipe instances are busy, so wait for 10 seconds.
        if (!WaitNamedPipeA(pipe_name.c_str(), 10000))
        {
            throw exception("Could not open pipe: 10 second wait timed out.");
        }
    }

    // The pipe connected; Set up the mode.
    DWORD dwMode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
    BOOL fSuccess = SetNamedPipeHandleState(
        hPipe,    // pipe handle
        &dwMode,  // new pipe mode
        NULL,     // don't set maximum bytes
        NULL);    // don't set maximum time
    if (!fSuccess)
    {
        sprintf_s(error,"SetNamedPipeHandleState failed. GLE=%lu", GetLastError());
        throw exception(error);
    }

    printf("Named pipe opened successfully.\n");
}


int main(int argc, char **argv)
{
    // buffers for sending and receiving.  Note: IN/OUT is from the perspective of the named pipe.
    DWORD out_bytes = 0;  // number of bytes waiting (in out_buffer) to be sent through the pipe
    char out_buffer[BUFSIZE + 1];
    DWORD in_bytes = 0;  // number of bytes that have been received into in_buffer from the pipe
    char in_buffer[BUFSIZE + 1];

    BOOL   fSuccess = FALSE;
    DWORD  cbRead, cbWritten;

    char error[400];
    DWORD gle = 0;

    printf("COMpipe 0.2\n");
    printf("https://github.com/tdhoward/COMpipe\n\n");

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
        printf("      Options typically include: 4800, 9600, 14400, 19200, 38400, 57600, and 115200. \n");
        printf("  4. Hardware signals like RTS/CTS, DTR/DSR, DCD, and RI are not supported.\n");
        printf("      This is because the named pipe created by the VM does not support these signals.\n\n");
        return 0;
    }

    printf("Press Q to quit.\n");

    // review command line parameters for pipe and COM port names
    InputParser input(argc, argv);

    const string &pipe_name = input.getCmdOption("-p");
    if (pipe_name.empty())
    {
        printf("Please include the name of the pipe.\n");
        return -1;
    }

    const string &serial_port = input.getCmdOption("-c");
    if (serial_port.empty())
    {
        printf("Please include the name of the serial port.\n");
        return -1;
    }

    string baud_rate = input.getCmdOption("-b");
    if (baud_rate.empty())
        baud_rate = "9600";
    int baud = stoi(baud_rate);

    try {
        RetryWithBackoff(OpenSerialPort, 5, 2000, serial_port, baud);
    }
    catch (const exception& e) {
        cerr << "Caught exception after maximum retries: " << e.what() << std::endl;
        Shutdown(-1);
    }

    try {
        RetryWithBackoff(OpenNamedPipe, 5, 2000, pipe_name);
    }
    catch (const exception& e) {
        cerr << "Caught exception after maximum retries: " << e.what() << std::endl;
        Shutdown(-1);
    }

    while (1)
    {
        // check if we should be exiting the loop
        if (CheckUserCancellation())  // Q key exits the loop
        {
            printf("Quitting...");
            break;
        }

        // go read our serial stream (up to 32 bytes at a time, so we don't block too long.)
        if (!ReadFile(serialHandle, out_buffer, 32, &out_bytes, NULL))
        {
            printf("Error reading serial port! GLE=%lu Attempting to reconnect...\n", GetLastError());
            // try to reconnect to pipe
            try {
                CloseHandle(serialHandle);
                Sleep(2000);
                RetryWithBackoff(OpenSerialPort, -1, 5000, serial_port, baud);  // retry every 5 seconds, forever
                continue;  // restart the while loop
            }
            catch (const exception& e) {
                cerr << "Caught exception after maximum retries: " << e.what() << std::endl;
                Shutdown(-1);
            }
        }

        // send what we read
        if (out_bytes > 0)
        {
            // send anything in the out_buffer
            fSuccess = WriteFile(
                hPipe,                  // pipe handle
                out_buffer,             // message
                out_bytes,              // message length
                &cbWritten,             // bytes written
                NULL);                  // not overlapped
            // TODO: check on cbWritten
            if (!fSuccess)
            {
                sprintf_s(error, "WriteFile to pipe failed. GLE=%lu\n", GetLastError());
                throw exception(error);
            }
        }

        // Read from the pipe.
        fSuccess = ReadFile(
            hPipe,    // pipe handle
            in_buffer,    // buffer to receive reply
            BUFSIZE,  // size of buffer
            &cbRead,  // number of bytes read
            NULL);    // not overlapped

        if (!fSuccess)
        {
            gle = GetLastError();
            if (gle != ERROR_NO_DATA)   // no data is fine, but handle any other error.
            {
                switch (gle)
                {
                case ERROR_PIPE_NOT_CONNECTED:
                    // try to reconnect to pipe
                    try {
                        printf("Named pipe disconnected abruptly.  Attempting to reconnect...");
                        CloseHandle(hPipe);
                        Sleep(2000);
                        RetryWithBackoff(OpenNamedPipe, -1, 5000, pipe_name);  // retry every 5 seconds, forever
                        continue;  // restart the while loop
                    }
                    catch (const exception& e) {
                        cerr << "Caught exception after maximum retries: " << e.what() << std::endl;
                        Shutdown(-1);
                    }
                default:
                    sprintf_s(error, "ReadFile from pipe failed. GLE=%lu\n", gle);
                    throw exception(error);
                }
            }
        }

        in_bytes = cbRead;
        in_buffer[in_bytes] = 0;  // null termination is not really necessary

        // send it to the serial stream
        if (!WriteFile(serialHandle, in_buffer, in_bytes, &cbWritten, NULL))
        {
            //error occurred. Report to user.
            sprintf_s(error, "Error writing to serial port! GLE=%lu\n", GetLastError());
            throw exception(error);
        }

        /*
        // create an echo:
        for(int i=0;i<in_bytes;i++)
            out_buffer[i] = in_buffer[i];
        out_bytes = in_bytes;
        out_buffer[out_bytes]=0;
        */
    }

    Shutdown(0);
    return 0;
}
