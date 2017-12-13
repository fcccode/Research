#include "stdafx.h"
#include <winternl.h>
#include <minwinbase.h>
#pragma comment(lib, "ntdll.lib")

typedef long NTSTATUS;

typedef NTSTATUS(WINAPI * PFN_NTQUERYINFORMATIONFILE)(
  IN HANDLE FileHandle,
  OUT PIO_STATUS_BLOCK IoStatusBlock,
  OUT PVOID FileInformation,
  IN ULONG Length,
  IN FILE_INFORMATION_CLASS FileInformationClass
  );


// FILE_NAME_INFORMATION contains name of queried file object. 
typedef struct _FILE_NAME_INFORMATION {
  ULONG FileNameLength;
  WCHAR FileName[1];
} FILE_NAME_INFORMATION, *PFILE_NAME_INFORMATION;


WCHAR const NTFS[] = L"NTFS";
DWORD const UNIT_PAGE_SIZE = 0x4000;  // 16MB
DWORD const UNIT_1_MB = 1024 * 1024;
DWORD const UNIT_100_MB = UNIT_1_MB * 50;

//using JOURNAL_MAP = std::map<DWORDLONG, std::pair<CString, DWORDLONG>>;
//
//JOURNAL_MAP usnMap;

class USNJournal
{
public:
  USNJournal()
    : _disk(NULL)
    , _driveLetter(L'\0')
    , _startUSN(0)
  {
  }

  ~USNJournal()
  {
    if (_disk)
    {
      DeleteJournal();
      CloseHandle(_disk);
    }
  }

  bool Start(WCHAR szDisk)
  {
    bool success = false;

    if (IsDiskNTFS(szDisk) && OpenDisk(szDisk))
    {
      _driveLetter = szDisk;

      success = CreateJournal();
    }

    return success;
  }

  void Stop()
  {
    QueryJournal();

#ifdef _DEBUG
    CString temp;
    temp.Format(L"Record count = %lld\n", _usnJournalData.NextUsn - _startUSN);
    OutputDebugString(temp);
#endif

    READ_USN_JOURNAL_DATA_V0 ReadData = { _startUSN, 0xFFFFFFFF, FALSE, 0, 0, _usnJournalData.UsnJournalID };
    PUSN_RECORD UsnRecord = {};

    BYTE buffer[UNIT_PAGE_SIZE] = {};

    while (true)
    {
      ZeroMemory(buffer, UNIT_PAGE_SIZE);

      DWORD bytesRead = 0;
      if (!DeviceIoControl(_disk, FSCTL_READ_USN_JOURNAL, &ReadData, sizeof(ReadData), &buffer, UNIT_PAGE_SIZE, &bytesRead, nullptr))
      {
        return;
      }

      DWORD remainingBytes = bytesRead - sizeof(USN);

      // Find the first record
      UsnRecord = reinterpret_cast<PUSN_RECORD>(reinterpret_cast<PUCHAR>(&buffer) + sizeof(USN));

      if (ReadData.StartUsn >= _usnJournalData.NextUsn)
        break;

      // This loop could go on for a long time, given the current buffer size.
      while (remainingBytes > 0)
      {
        CString reason;

        if (ValidUSNReason(UsnRecord->Reason, reason))
        {
          CString fileName;

          if (UsnRecord->Reason & (USN_REASON_RENAME_OLD_NAME | USN_REASON_FILE_DELETE))
          {
            fileName.Format(L"%.*s", UsnRecord->FileNameLength, UsnRecord->FileName);
            fileName = GetFilePathFromFileReferenceNumber(UsnRecord->ParentFileReferenceNumber, UsnRecord->Usn, fileName);
          }
          else
          {
            fileName = GetFilePathFromFileReferenceNumber(UsnRecord->FileReferenceNumber);
          }

#ifdef _DEBUG
          if (!fileName.IsEmpty())
          {
            OutputDebugString(fileName);
            OutputDebugString(L" - ");
            OutputDebugString(reason);
            OutputDebugString(L"\n");
          }
#endif
        }

        remainingBytes -= UsnRecord->RecordLength;

        // Find the next record
        UsnRecord = reinterpret_cast<PUSN_RECORD>(reinterpret_cast<PCHAR>(UsnRecord) + UsnRecord->RecordLength);
      }

      // Update starting USN for next call
      ReadData.StartUsn = *reinterpret_cast<USN *>(&buffer);
    }
  }

private:
  bool IsDiskNTFS(WCHAR szDisk)
  {
    bool diskIsNTFS = false;

    CString szDrivePath(szDisk);

    szDrivePath.Append(L":\\");

    WCHAR szFileSystemName[MAX_PATH + 1] = {};

    if (GetVolumeInformation(szDrivePath, nullptr, 0, nullptr, nullptr, nullptr, szFileSystemName, MAX_PATH + 1))
    {
      diskIsNTFS = memcmp(szFileSystemName, NTFS, sizeof(NTFS)) == 0;
    }

    return diskIsNTFS;
  }

  bool OpenDisk(WCHAR szDisk)
  {
    if (_disk && _disk != INVALID_HANDLE_VALUE)
      return true;

    CString szNTDrivePath(L"\\\\.\\");
    szNTDrivePath.AppendFormat(L"%c:", szDisk);

    _disk = CreateFile(szNTDrivePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, NULL);

    return _disk != INVALID_HANDLE_VALUE;
  }

  bool CreateJournal()
  {
    bool success = false;

    ZeroMemory(&_usnJournalData, sizeof(_usnJournalData));

    CREATE_USN_JOURNAL_DATA usnJournal = { UNIT_100_MB, UNIT_1_MB };

    DWORD dwBytesReturned = 0;

    if (DeviceIoControl(_disk, FSCTL_CREATE_USN_JOURNAL, &usnJournal, sizeof(CREATE_USN_JOURNAL_DATA),
      nullptr, 0, &dwBytesReturned, nullptr))
    {
      success = QueryJournal();

      _startUSN = _usnJournalData.NextUsn;
    }

    return success;
  }

  BOOL QueryJournal()
  {
    DWORD dwBytesReturned = 0;

    return DeviceIoControl(_disk, FSCTL_QUERY_USN_JOURNAL, nullptr, 0, &_usnJournalData, sizeof(USN_JOURNAL_DATA_V0), &dwBytesReturned, nullptr);
  }

  void DeleteJournal()
  {
    DELETE_USN_JOURNAL_DATA deleteJournal = { _usnJournalData.UsnJournalID, USN_DELETE_FLAG_DELETE };

    DWORD dwBytesReturned = 0;

    DeviceIoControl(_disk, FSCTL_CREATE_USN_JOURNAL, &deleteJournal, sizeof(DELETE_USN_JOURNAL_DATA), nullptr, 0, &dwBytesReturned, nullptr);
  }

  CString GetFilePathFromFileReferenceNumber(DWORDLONG fileRefernceNumber)
  {
    CString filePath;

    static PFN_NTQUERYINFORMATIONFILE NtQueryInformationFile = nullptr;

    if (!NtQueryInformationFile)
    {
      HINSTANCE hNtDll = LoadLibrary(_T("ntdll.dll"));
      NtQueryInformationFile = (PFN_NTQUERYINFORMATIONFILE)GetProcAddress(hNtDll, "NtQueryInformationFile");
    }

    if (NtQueryInformationFile)
    {
      NTSTATUS status = 0;

      HANDLE file = NULL;

      UNICODE_STRING objName = { 8, 8, (PWSTR)&fileRefernceNumber };

      OBJECT_ATTRIBUTES objAttrs = {};

      InitializeObjectAttributes(&objAttrs, &objName, OBJ_CASE_INSENSITIVE, _disk, nullptr);

      IO_STATUS_BLOCK ioStatusBlock = {};

      status = NtCreateFile(&file, FILE_GENERIC_READ, &objAttrs, &ioStatusBlock, nullptr, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN,
        FILE_OPEN_BY_FILE_ID, nullptr, 0);

      if (NT_SUCCESS(status))
      {
        BYTE buffer[512] = {};

        status = NtQueryInformationFile(file, &ioStatusBlock, buffer, 512, (FILE_INFORMATION_CLASS)9);

        if (NT_SUCCESS(status))
        {
          FILE_NAME_INFORMATION* pFileNameInformation = (FILE_NAME_INFORMATION*)buffer;

          filePath.Format(L"%c:%.*s", _driveLetter, pFileNameInformation->FileNameLength, pFileNameInformation->FileName);
        }

        NtClose(file);
      }
    }

    return filePath;
  }

  CString GetFilePathFromFileReferenceNumber(DWORDLONG fileReferenceNumber, USN usn, CString& fileName)
  {
    CString fullPath;

    PUSN_RECORD usnRecord = {};

    BYTE buffer[UNIT_PAGE_SIZE] = {};

    MFT_ENUM_DATA_V0 enumData = { fileReferenceNumber, 0, usn };

    DWORD bytesRead = 0;

    if (DeviceIoControl(_disk, FSCTL_ENUM_USN_DATA, &enumData, sizeof(enumData), &buffer, UNIT_PAGE_SIZE, &bytesRead, nullptr))
    {
      usnRecord = reinterpret_cast<PUSN_RECORD>(reinterpret_cast<PUCHAR>(&buffer) + sizeof(USN));

      if (usnRecord->ParentFileReferenceNumber != fileReferenceNumber)
      {
        CString recordfileName;
        recordfileName.Format(L"%.*s", usnRecord->FileNameLength, usnRecord->FileName);

        if (recordfileName.Compare(L"$RmMetadata") != 0)
        {
          fullPath.Format(L"%s\\%s", recordfileName, fileName);

          if (usnRecord->ParentFileReferenceNumber)
          {
            fullPath = GetFilePathFromFileReferenceNumber(usnRecord->ParentFileReferenceNumber, usnRecord->Usn, fullPath);
          }
        }
        else
        {
          fullPath = fileName;

          fullPath.Insert(0, L'\\');
          fullPath.Insert(0, L':');
          fullPath.Insert(0, _driveLetter);
        }
      }
    }

    return fullPath;
  }

  bool ValidUSNReason(DWORD usnReason, CString& reason)
  {
    if (usnReason & USN_REASON_DATA_EXTEND) return false;
    if ((usnReason & (USN_REASON_CLOSE | USN_REASON_RENAME_NEW_NAME)) == (USN_REASON_CLOSE | USN_REASON_RENAME_NEW_NAME)) return false;
    if ((usnReason & (USN_REASON_CLOSE | USN_REASON_FILE_CREATE)) == (USN_REASON_CLOSE | USN_REASON_FILE_CREATE)) return false;
    if ((usnReason & (USN_REASON_CLOSE | USN_REASON_DATA_OVERWRITE)) == (USN_REASON_CLOSE | USN_REASON_DATA_OVERWRITE)) return false;

    if (usnReason & USN_REASON_BASIC_INFO_CHANGE) return false;
    if (usnReason & USN_REASON_OBJECT_ID_CHANGE) return false;

    if (usnReason & USN_REASON_CLOSE)                 { reason.Append(L" CLOSE"); }
    if (usnReason & USN_REASON_COMPRESSION_CHANGE)    { reason.Append(L" COMPRESSION_CHANGE"); }
    if (usnReason & USN_REASON_DATA_EXTEND)           { reason.Append(L" DATA_EXTEND"); }
    if (usnReason & USN_REASON_DATA_OVERWRITE)        { reason.Append(L" DATA_OVERWRITE"); }
    if (usnReason & USN_REASON_DATA_TRUNCATION)       {  reason.Append(L" DATA_TRUNCATION"); }
    if (usnReason & USN_REASON_EA_CHANGE)             { reason.Append(L" EA_CHANGE"); }
    if (usnReason & USN_REASON_ENCRYPTION_CHANGE)     { reason.Append(L" ENCRYPTION_CHANGE"); }
    if (usnReason & USN_REASON_FILE_CREATE)           { reason.Append(L" FILE_CREATE"); }
    if (usnReason & USN_REASON_FILE_DELETE)           { reason.Append(L" FILE_DELETE"); }
    if (usnReason & USN_REASON_HARD_LINK_CHANGE)      { reason.Append(L" HARD_LINK_CHANGE"); }
    if (usnReason & USN_REASON_INDEXABLE_CHANGE)      { reason.Append(L" INDEXABLE_CHANGE"); }
    if (usnReason & USN_REASON_INTEGRITY_CHANGE)      { reason.Append(L" INTEGRITY_CHANGE"); }
    if (usnReason & USN_REASON_NAMED_DATA_EXTEND)     { reason.Append(L" NAMED_DATA_EXTEND"); }
    if (usnReason & USN_REASON_NAMED_DATA_OVERWRITE)  { reason.Append(L" NAMED_DATA_OVERWRITE"); }
    if (usnReason & USN_REASON_NAMED_DATA_TRUNCATION) { reason.Append(L" NAMED_DATA_TRUNCATION"); }
    if (usnReason & USN_REASON_RENAME_NEW_NAME)       { reason.Append(L" RENAME_NEW_NAME"); }
    if (usnReason & USN_REASON_RENAME_OLD_NAME)       { reason.Append(L" RENAME_OLD_NAME"); }
    if (usnReason & USN_REASON_REPARSE_POINT_CHANGE)  { reason.Append(L" REPARSE_POINT_CHANGE"); }
    if (usnReason & USN_REASON_SECURITY_CHANGE)       { reason.Append(L" SECURITY_CHANGE"); }
    if (usnReason & USN_REASON_STREAM_CHANGE)         { reason.Append(L" STREAM_CHANGE"); }

    return true;
  }

  HANDLE _disk;
  USN_JOURNAL_DATA_V0 _usnJournalData;
  WCHAR _driveLetter;
  USN _startUSN;
};

void InstallApplication()
{
  CreateDirectory(L"C:\\journal", nullptr);
  const HANDLE file = CreateFile(L"C:\\journal\\a.txt", GENERIC_ALL, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  DWORD bytesWritten = 0;
  WriteFile(file, NTFS, sizeof(NTFS), &bytesWritten, nullptr);
  CloseHandle(file);

  CopyFile(L"C:\\journal\\a.txt", L"C:\\journal\\b.txt", FALSE);
  CopyFile(L"C:\\journal\\a.txt", L"C:\\journal\\c.txt", FALSE);
  CopyFile(L"C:\\journal\\a.txt", L"C:\\journal\\d.txt", FALSE);
  CopyFile(L"C:\\journal\\a.txt", L"C:\\journal\\e.txt", FALSE);

  MoveFile(L"C:\\journal\\d.txt", L"C:\\journal\\dd.txt");
  DeleteFile(L"C:\\journal\\c.txt");
}

void UninstallApplication()
{
  DeleteFile(L"C:\\journal\\a.txt");
  DeleteFile(L"C:\\journal\\b.txt");
  DeleteFile(L"C:\\journal\\dd.txt");
  DeleteFile(L"C:\\journal\\e.txt");

  RemoveDirectory(L"C:\\journal");
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
  _In_opt_ HINSTANCE hPrevInstance,
  _In_ LPWSTR    lpCmdLine,
  _In_ int       nCmdShow)
{
  UNREFERENCED_PARAMETER(hInstance);
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(lpCmdLine);
  UNREFERENCED_PARAMETER(nCmdShow);

  DWORD dwTime = 0;
  USNJournal usnJournal;

  if (usnJournal.Start(L'C'))
  {
    InstallApplication();

    MessageBox(NULL, L"Install application", L"hghhh", MB_OK);
    DWORD dwStart = GetTickCount();

    usnJournal.Stop();

    dwTime = GetTickCount() - dwStart;

    UninstallApplication();
  }


  CString ttt;
  ttt.Format(L"Time taken = %dms, %ds\n", dwTime, dwTime / 1000);
  OutputDebugString(ttt);

  MessageBox(NULL, ttt, L"snapshot difference time", MB_OK);

  return 0;
}
