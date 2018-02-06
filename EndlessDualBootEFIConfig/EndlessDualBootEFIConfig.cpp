// EndlessDualBootEFIConfig.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "UEFIBootSetup.h"
#include "GeneralCode.h"
#include "drive.h"

#define MAX_GUID_STRING_LENGTH      40

#if !defined(PARTITION_SYSTEM_GUID)
const GUID PARTITION_SYSTEM_GUID =
{ 0xc12a7328L, 0xf81f, 0x11d2,{ 0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b } };
#endif

char* GuidToString(const GUID* guid)
{
	static char guid_string[MAX_GUID_STRING_LENGTH];

	if (guid == NULL) return NULL;
	sprintf_s(guid_string, MAX_GUID_STRING_LENGTH,
		"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		(unsigned int)guid->Data1, guid->Data2, guid->Data3,
		guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
		guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
	return guid_string;
}

// logical_drive.Format("\\\\.\\%lc:", driveLetter[0]);
HANDLE GetPhysicalFromDriveLetter(char *logical_drive)
{
	FUNCTION_ENTER;

	HANDLE hPhysical = INVALID_HANDLE_VALUE;

	hPhysical = CreateFileA(logical_drive, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	IFFALSE_GOTOERROR(hPhysical != INVALID_HANDLE_VALUE, "CreateFileA call returned invalid handle.");

	int drive_number = GetDriveNumber(hPhysical, logical_drive);
	drive_number += DRIVE_INDEX_MIN;
	safe_closehandle(hPhysical);

	hPhysical = GetPhysicalHandle(drive_number, TRUE, TRUE);
	IFFALSE_GOTOERROR(hPhysical != INVALID_HANDLE_VALUE, "Error on acquiring disk handle.");

error:
	return hPhysical;
}


BOOL MountESPFromDrive(HANDLE hPhysical, const char **espMountLetter)
{
	FUNCTION_ENTER;

	BOOL retResult = FALSE;
	BYTE layout[4096] = { 0 };
	PDRIVE_LAYOUT_INFORMATION_EX DriveLayout = (PDRIVE_LAYOUT_INFORMATION_EX)(void*)layout;
	DWORD size;
	BOOL result;

	// get partition layout
	result = DeviceIoControl(hPhysical, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, layout, sizeof(layout), &size, NULL);
	IFFALSE_GOTOERROR(result != 0 && size > 0, "Error on querying disk layout.");
	IFFALSE_GOTOERROR(DriveLayout->PartitionStyle == PARTITION_STYLE_GPT, "Unexpected partition type. Partition style is not GPT");

	DWORD efiPartitionNumber = -1;
	PARTITION_INFORMATION_EX *partition = NULL;
	for (DWORD index = 0; index < DriveLayout->PartitionCount; index++) {
		partition = &(DriveLayout->PartitionEntry[index]);

		if (partition->Gpt.PartitionType == PARTITION_SYSTEM_GUID) {
			uprintf("Found ESP\r\nPartition %d:\r\n  Type: %s\r\n  Name: '%ls'\r\n ID: %s\n",
				index + 1, GuidToString(&partition->Gpt.PartitionType), partition->Gpt.Name, GuidToString(&partition->Gpt.PartitionId));
			efiPartitionNumber = partition->PartitionNumber;
			break;
		}
	}
	IFFALSE_GOTOERROR(efiPartitionNumber != -1, "ESP not found.");
	// Fail if EFI partition number is bigger than we can fit in the
	// uin8_t that AltMountVolume receives as parameter for partition number
	IFFALSE_GOTOERROR(efiPartitionNumber <= 0xFF, "EFI partition number is bigger than 255.");

	*espMountLetter = AltMountVolume("C:", (uint8_t)efiPartitionNumber);
	IFFALSE_GOTOERROR(*espMountLetter != NULL, "Error assigning a letter to the ESP.");

	retResult = TRUE;
error:
	return retResult;
}

static bool
EnsureDirectoryW(LPCWSTR lpPathName)
{
	FUNCTION_ENTER_FMT("%ls", lpPathName);
	return !!CreateDirectoryW(lpPathName, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

FILE *logFile = NULL;

static wchar_t wszEspMountLetter[4] = { 0 };
static const char *szEspMountLetter = NULL;
static const wchar_t *wszDesc = L"Endless Bootloader Installation Test";

static bool MountESP()
{
	bool ret = false;
	HANDLE hPhysical = INVALID_HANDLE_VALUE;

	uprintf("== Mounting ESP\n");

	hPhysical = GetPhysicalFromDriveLetter("\\\\.\\C:");
	IFFALSE_GOTOERROR(hPhysical != INVALID_HANDLE_VALUE, "Couldn't get handle");
	IFFALSE_GOTOERROR(MountESPFromDrive(hPhysical, &szEspMountLetter), "Mounting ESP failed");
	IFFALSE_GOTOERROR(MultiByteToWideChar(CP_UTF8, 0, szEspMountLetter, -1, wszEspMountLetter, 4), "Widening ESP drive letter failed");

	ret = true;
error:
	safe_closehandle(hPhysical);
	return ret;
}

enum Mode {
	INVALID,
	INSTALL,
	UNINSTALL,
};

Mode GetMode(int argc, char **argv, int &flags)
{
	flags = 0;

	for (int i = 1; i < argc; i++) {
		CStringA flag = argv[i];
		flag.MakeLower();

		if (flag == "/uninstall") {
			return Mode::UNINSTALL;
		} else if (flag == "/setbootnext") {
			flags |= UEFIBootSetupFlags::SetBootNext;
		}
		else if (flag == "/replacewindowsentry") {
			flags |= UEFIBootSetupFlags::ReplaceWindowsEntry;
		}
		else {
			uprintf("Invalid argument %s\n", argv[1]);
			return Mode::INVALID;
		}
	}

	return Mode::INSTALL;
}

int main(int argc, char **argv)
{
	Mode mode;
	int flags = 0;
	CTime time = CTime::GetCurrentTime();
	wchar_t wszPathToSelf[MAX_PATH];
	CStringW logFilePath;
	CStringW grubCfgPath;
	CStringW grubCfgTarget = L"C:\\endless\\grub\\grub.cfg";
	CString wszEspSubdir(L"?:\\EFI\\EndlessTest");
	CString wszShimPath;
	errno_t ret_errno = 0;
	int ret = 1;

	IFFALSE_GOTOERROR(GetModuleFileNameW(NULL, wszPathToSelf, MAX_PATH), "Couldn't determine own path");
	wchar_t *slash = StrRChrW(wszPathToSelf, NULL, L'\\');
	IFFALSE_GOTOERROR(slash != NULL, "No slash in path to self");
	*(slash + 1) = L'\0';

	logFilePath.Format(L"%ls\\EndlessDualBootEFIConfig.%ls.txt",
		wszPathToSelf, time.FormatGmt(L"%Y%m%d_%H%M%S"));
	if (0 != (ret_errno = _wfopen_s(&logFile, logFilePath, L"a"))) {
		uprintf("Couldn't open log file %ls: %d\n", logFilePath.GetBuffer(), ret_errno);
	}

	if ((mode = GetMode(argc, argv, flags)) == Mode::INVALID) {
		goto error;
	}

	IFFALSE_GOTOERROR(EFIRequireNeededPrivileges(), "Failed to get privileges needed to manipulate EFI variables");

	if (mode == Mode::UNINSTALL) {
		uprintf("== Removing BootXXXX from BootOrder\n");
		bool found_entry = false;
		IFFALSE_GOTOERROR(EFIRemoveEntry(wszDesc, found_entry), "Couldn't remove boot entry");
		IFFALSE_GOTOERROR(found_entry, "Boot entry wasn't found");

		// Sorry, I am too lazy to remove C:\endless\grub\grub.cfg and the loader executables from the ESP
	} else {
		grubCfgPath = CStringW(wszPathToSelf) + L"grub.cfg";

		uprintf("== Installing stub GRUB config to %ls\n", grubCfgTarget.GetBuffer());
		IFFALSE_GOTOERROR(EnsureDirectoryW(L"C:\\endless"), "");
		IFFALSE_GOTOERROR(EnsureDirectoryW(L"C:\\endless\\grub"), "");
		IFFALSE_GOTOERROR(CopyFileW(grubCfgPath, grubCfgTarget, FALSE), "Copying grub.cfg failed");

		IFFALSE_GOTOERROR(MountESP(), "Mounting ESP failed");

		wszEspSubdir.SetAt(0, wszEspMountLetter[0]);
		wszShimPath = wszEspSubdir + L"\\shim.efi";

		uprintf("== Mounted ESP at %ls\n", wszEspMountLetter);
		uprintf("== Copying bootloaders to %ls\n", wszEspSubdir.GetBuffer());

		IFFALSE_GOTOERROR(EnsureDirectoryW(wszEspSubdir), "Creating subdirectory in ESP failed");
		IFFALSE_GOTOERROR(CopyFileW(CStringW(wszPathToSelf) + L"shim.efi", wszShimPath, FALSE), "Copying Shim failed");
		IFFALSE_GOTOERROR(CopyFileW(CStringW(wszPathToSelf) + L"grubx64.efi", wszEspSubdir + L"\\grubx64.efi", FALSE), "Copying GRUB failed");

		uprintf("== Adding EFI boot entry for %ls\n", wszShimPath.GetBuffer());

		IFFALSE_GOTOERROR(EFICreateNewEntry(wszEspMountLetter, wszShimPath.Mid(2).GetBuffer(), wszDesc, flags), "Couldn't add boot entry");
	}

	ret = 0;

error:
	if (szEspMountLetter != NULL) {
		IFFALSE_PRINTERROR(AltUnmountVolume(szEspMountLetter), "Unmounting ESP failed");
	}

	if (logFile != NULL) {
		fclose(logFile);
	}

	return ret;
}

