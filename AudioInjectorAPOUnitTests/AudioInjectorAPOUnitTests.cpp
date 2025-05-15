#include "CppUnitTest.h" 
#include "../AudioInjectorAPO/AudioFileReader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


namespace AudioInjectorAPOUnitTests
{
   TEST_CLASS(AudioFileReaderTests)
   {
   private:
       std::wstring GetTestFilePath(const std::wstring& fileName) const
       {
           static wchar_t modulePath[MAX_PATH]{0};
           HMODULE hModule = nullptr;

           // Get handle to the current module (the test DLL)
            GetModuleHandleExW(  
               GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,  
               reinterpret_cast<LPCWSTR>(&modulePath[0]),
               &hModule);

           // Get the full path to the module
           GetModuleFileNameW(hModule, modulePath, MAX_PATH);

           // Remove the file name to get the directory
           std::wstring dirPath(modulePath);
           size_t pos = dirPath.find_last_of(L"\\/");
           if (pos != std::wstring::npos)
               dirPath = dirPath.substr(0, pos + 1);

           // Append the file name
           return dirPath + fileName;

       }
   public:

       TEST_METHOD(CanLoadAudioFile)
       {
           std::wstring filePath = GetTestFilePath(L"test.wav");
           AudioFileReader reader;

           HRESULT hr = reader.Initialize(filePath.c_str());
           Assert::IsTrue(SUCCEEDED(hr), (L"Initialize should succeed for file: " + filePath).c_str());
           Assert::IsTrue(reader.IsValid(), L"Reader should be valid after initialization");
           Assert::IsTrue(reader.GetFrameCount() > 0, L"Frame count should be greater than 0");
           Assert::IsTrue(reader.GetChannelCount() > 0, L"Channel count should be greater than 0");
           Assert::IsTrue(reader.GetSampleRate() > 0, L"Sample rate should be greater than 0");
       }
       // Test to verify that an invalid file path fails to initialize
       TEST_METHOD(FailsOnInvalidFile)
       {
           std::wstring invalidFilePath = GetTestFilePath(L"nonexistent.wav");
           AudioFileReader reader;

           HRESULT hr = reader.Initialize(invalidFilePath.c_str());
           Assert::IsFalse(SUCCEEDED(hr), L"Initialize should fail with an invalid file path");
           Assert::IsFalse(reader.IsValid(), L"Reader should not be valid for an invalid file");
       }

       // Test to verify that the audio data pointer is not null after a valid initialization
       TEST_METHOD(AudioDataIsNotNull)
       {
           std::wstring filePath = GetTestFilePath(L"test.wav");
           AudioFileReader reader;

           HRESULT hr = reader.Initialize(filePath.c_str());
           Assert::IsTrue(SUCCEEDED(hr), L"Initialize should succeed for a valid wav file");
           Assert::IsNotNull(reader.GetAudioData(), L"Audio data pointer should not be null");
       }

       // Test the resampling functionality and cleanup behavior
       TEST_METHOD(CanResampleAndCleanup)
       {
           std::wstring filePath = GetTestFilePath(L"test.wav");
           AudioFileReader reader;

           HRESULT hr = reader.Initialize(filePath.c_str());
           Assert::IsTrue(SUCCEEDED(hr), L"Initialize should succeed for a valid wav file");
           Assert::IsTrue(reader.IsValid(), L"Reader should be valid after initialization");

           const UINT32 targetSampleRate = 48000;  // Example target sample rate
           const UINT32 targetChannelCount = 2;      // Example target channel count
           hr = reader.ResampleAudio(targetSampleRate, targetChannelCount);
           Assert::IsTrue(SUCCEEDED(hr), L"ResampleAudio should succeed");

           // Assuming the resampling updates the reader's properties:
           Assert::AreEqual(targetSampleRate, reader.GetSampleRate(), L"Sample rate should be updated to target value");
           Assert::AreEqual(targetChannelCount, reader.GetChannelCount(), L"Channel count should be updated to target value");

           // Cleanup should release resources and change the reader's valid state
           reader.Cleanup();
           Assert::IsFalse(reader.IsValid(), L"Reader should not be valid after cleanup");
       }

	   // Test to verify that the resampling function works correctly
       TEST_METHOD(ResampleAudioWorksCorrectly)
       {
           std::wstring filePath = GetTestFilePath(L"test.wav");
           AudioFileReader reader;

           HRESULT hr = reader.Initialize(filePath.c_str());
           Assert::IsTrue(SUCCEEDED(hr), L"Initialization should succeed for a valid wav file");

           // Save original properties
           const UINT32 originalFrameCount = reader.GetFrameCount();
           const UINT32 originalSampleRate = reader.GetSampleRate();
           const UINT32 originalChannelCount = reader.GetChannelCount();

           // Define new target parameters for resampling.
           const UINT32 targetSampleRate = originalSampleRate * 2;
           const UINT32 targetChannelCount = (originalChannelCount == 1) ? 2 : 1;

           // Perform resampling
           hr = reader.ResampleAudio(targetSampleRate, targetChannelCount);
           Assert::IsTrue(SUCCEEDED(hr), L"ResampleAudio should succeed for valid parameters");

           // Verify updated properties
           Assert::AreEqual(targetSampleRate, reader.GetSampleRate(), L"Sample rate should be updated to target");
           Assert::AreEqual(targetChannelCount, reader.GetChannelCount(), L"Channel count should be updated to target");

           // Calculate expected frame count and allow a small tolerance due to rounding differences.
           UINT32 expectedFrameCount = static_cast<UINT32>(
               (static_cast<UINT64>(originalFrameCount) * targetSampleRate) / originalSampleRate);

           UINT32 actualFrameCount = reader.GetFrameCount();
           const UINT32 tolerance = 100; // Adjust tolerance as needed.
           UINT32 diff = (expectedFrameCount > actualFrameCount) ? (expectedFrameCount - actualFrameCount) : (actualFrameCount - expectedFrameCount);
           Assert::IsTrue(diff <= tolerance, L"Frame count should be properly adjusted after resampling (within tolerance).");
       }

       TEST_METHOD(ReinitializeWorks)
       {
           std::wstring filePath = GetTestFilePath(L"test.wav");
           AudioFileReader reader;

           // First initialization
           HRESULT hr = reader.Initialize(filePath.c_str());
           Assert::IsTrue(SUCCEEDED(hr), L"Initial initialization should succeed");
           Assert::IsTrue(reader.IsValid(), L"Reader should be valid after initial initialization");
           UINT32 initialFrameCount = reader.GetFrameCount();

           // Cleanup
           reader.Cleanup();
           Assert::IsFalse(reader.IsValid(), L"Reader should be invalid after cleanup");

           // Reinitialize and check state consistency
           hr = reader.Initialize(filePath.c_str());
           Assert::IsTrue(SUCCEEDED(hr), L"Reinitialization should succeed");
           Assert::IsTrue(reader.IsValid(), L"Reader should be valid after reinitialization");
           Assert::AreEqual(initialFrameCount, reader.GetFrameCount(), L"Frame count should match after reinitialization");
       }

       TEST_METHOD(CleanupIdempotence)
       {
           std::wstring filePath = GetTestFilePath(L"test.wav");
           AudioFileReader reader;
           HRESULT hr = reader.Initialize(filePath.c_str());
           Assert::IsTrue(SUCCEEDED(hr), L"Initialization should succeed");

           // Calling Cleanup multiple times should be safe
           reader.Cleanup();
           reader.Cleanup();
           Assert::IsFalse(reader.IsValid(), L"Reader should remain invalid after multiple cleanup calls");
       }

       TEST_METHOD(InvalidResampleParameters)
       {
           std::wstring filePath = GetTestFilePath(L"test.wav");
           AudioFileReader reader;
           HRESULT hr = reader.Initialize(filePath.c_str());
           Assert::IsTrue(SUCCEEDED(hr), L"Initialization should succeed");

           // Invalid sample rate
           hr = reader.ResampleAudio(0, reader.GetChannelCount());
           Assert::IsFalse(SUCCEEDED(hr), L"ResampleAudio should fail with a sample rate of 0");

           // Invalid channel count
           hr = reader.ResampleAudio(reader.GetSampleRate(), 0);
           Assert::IsFalse(SUCCEEDED(hr), L"ResampleAudio should fail with a channel count of 0");
       }

       TEST_METHOD(ZeroLengthFile)
       {
           // Assuming you have a zero-length or silent wav file named "empty.wav"
           std::wstring filePath = GetTestFilePath(L"empty.wav");
           AudioFileReader reader;
           HRESULT hr = reader.Initialize(filePath.c_str());

           // Expected behavior can vary: either initialization fails or reports zero frames.
           Assert::IsFalse(reader.IsValid(), L"Reader should be invalid for a zero-length file");
           Assert::AreEqual(0u, reader.GetFrameCount(), L"Frame count should be zero for a zero-length file");
       }

       TEST_METHOD(UnsupportedFormat)
       {
           // Assuming you have an unsupported file, e.g., "unsupported.wav" (or a different extension)
           std::wstring filePath = GetTestFilePath(L"unsupported.wav");
           AudioFileReader reader;
           HRESULT hr = reader.Initialize(filePath.c_str());
           Assert::IsFalse(SUCCEEDED(hr), L"Initialization should fail for an unsupported audio format");
           Assert::IsFalse(reader.IsValid(), L"Reader should be invalid for an unsupported format");
       }
   };
}
