using System;
using System.Runtime.InteropServices;

namespace AudioInjectorTest
{
    public static class AudioInjectorClientWrapper
    {
        // Import the external DLL function for injecting audio  
        [DllImport("AudioInjectorClient.dll",  CallingConvention = CallingConvention.Cdecl)]
        private static extern int StartInjection(string deviceName, string filePath);

        // Public wrapper method to call the DLL function  
        public static int DoStartInjection(string deviceName, string filePath)
        {
            try
            {
                return StartInjection(deviceName, filePath);
            }
            catch (DllNotFoundException)
            {
                throw new Exception("AudioInjectorClient.dll not found. Ensure the DLL is in the application directory.");
            }
            catch (EntryPointNotFoundException)
            {
                throw new Exception("The StartInjection function was not found in AudioInjectorClient.dll.");
            }
        }
        public static int DoCancelInjection(string deviceName)
        {
            try
            {
                return 0; // CancelInjection(deviceName);
            }
            catch (DllNotFoundException)
            {
                throw new Exception("AudioInjectorClient.dll not found. Ensure the DLL is in the application directory.");
            }
            catch (EntryPointNotFoundException)
            {
                throw new Exception("The CancelInjection function was not found in AudioInjectorClient.dll.");
            }
        }
    }
}
