/*
* Rufus: The Reliable USB Formatting Utility
* Drive access function calls
* Copyright © 2011-2016 Pete Batard <pete@akeo.ie>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"
#include "drive.h"
#include "GeneralCode.h"


#define safe_free(p) do {free((void*)p); p = NULL;} while(0)
#define safe_min(a, b) min((size_t)(a), (size_t)(b))
#define safe_strcp(dst, dst_max, src, count) do {memcpy(dst, src, safe_min(count, dst_max)); \
	((char*)dst)[safe_min(count, dst_max)-1] = 0;} while(0)
#define safe_strcpy(dst, dst_max, src) safe_strcp(dst, dst_max, src, safe_strlen(src)+1)
#define safe_strncat(dst, dst_max, src, count) strncat(dst, src, safe_min(count, dst_max - safe_strlen(dst) - 1))
#define safe_strcat(dst, dst_max, src) safe_strncat(dst, dst_max, src, safe_strlen(src)+1)
#define safe_strcmp(str1, str2) strcmp(((str1==NULL)?"<NULL>":str1), ((str2==NULL)?"<NULL>":str2))
#define safe_strstr(str1, str2) strstr(((str1==NULL)?"<NULL>":str1), ((str2==NULL)?"<NULL>":str2))
#define safe_stricmp(str1, str2) _stricmp(((str1==NULL)?"<NULL>":str1), ((str2==NULL)?"<NULL>":str2))
#define safe_strncmp(str1, str2, count) strncmp(((str1==NULL)?"<NULL>":str1), ((str2==NULL)?"<NULL>":str2), count)
#define safe_strnicmp(str1, str2, count) _strnicmp(((str1==NULL)?"<NULL>":str1), ((str2==NULL)?"<NULL>":str2), count)
#define safe_closehandle(h) do {if ((h != INVALID_HANDLE_VALUE) && (h != NULL)) {CloseHandle(h); h = INVALID_HANDLE_VALUE;}} while(0)
#define safe_release_dc(hDlg, hDC) do {if ((hDC != INVALID_HANDLE_VALUE) && (hDC != NULL)) {ReleaseDC(hDlg, hDC); hDC = NULL;}} while(0)
#define safe_sprintf(dst, count, ...) do {_snprintf_s(dst, count, __VA_ARGS__); (dst)[(count)-1] = 0; } while(0)
#define static_sprintf(dst, ...) safe_sprintf(dst, sizeof(dst), __VA_ARGS__)
#define safe_strlen(str) ((((char*)str)==NULL)?0:strlen(str))
#define safe_strdup _strdup
#if defined(_MSC_VER)
#define safe_vsnprintf(buf, size, format, arg) _vsnprintf_s(buf, size, _TRUNCATE, format, arg)
#else
#define safe_vsnprintf vsnprintf
#endif

/*
* Working with drive indexes quite risky (left unchecked,inadvertently passing 0 as
* index would return a handle to C:, which we might then proceed to unknowingly
* clear the MBR of!), so we mitigate the risk by forcing our indexes to belong to
* the specific range [DRIVE_INDEX_MIN; DRIVE_INDEX_MAX].
*/
#define CheckDriveIndex(DriveIndex) do { \
	if ((DriveIndex < DRIVE_INDEX_MIN) || (DriveIndex > DRIVE_INDEX_MAX)) { \
		uprintf("ERROR: Bad index value. Please check the code!\n"); \
		goto out; \
	} \
	DriveIndex -= DRIVE_INDEX_MIN; } while (0)


/*
* Open a drive or volume with optional write and lock access
* Return INVALID_HANDLE_VALUE (/!\ which is DIFFERENT from NULL /!\) on failure.
*/
static HANDLE GetHandle(char* Path, BOOL bWriteAccess, BOOL bLockDrive)
{
	int i;
	DWORD size;
	HANDLE hDrive = INVALID_HANDLE_VALUE;

	if (Path == NULL)
		goto out;
	hDrive = CreateFileA(Path, GENERIC_READ | (bWriteAccess ? GENERIC_WRITE : 0),
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hDrive == INVALID_HANDLE_VALUE) {
		uprintf("Could not open drive %s: %d\n", Path, GetLastError());
		goto out;
	}

	if (bWriteAccess) {
		uprintf("Opened drive %s for write access\n", Path);
	}

	if (bLockDrive) {
		if (DeviceIoControl(hDrive, FSCTL_ALLOW_EXTENDED_DASD_IO, NULL, 0, NULL, 0, &size, NULL)) {
			uprintf("I/O boundary checks disabled\n");
		}

		for (i = 0; i < DRIVE_ACCESS_RETRIES; i++) {
			uprintf("Attempting FSCTL_LOCK_VOLUME for drive %s\n", Path);
			if (DeviceIoControl(hDrive, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &size, NULL))
				goto out;
			Sleep(DRIVE_ACCESS_TIMEOUT / DRIVE_ACCESS_RETRIES);
		}
		// If we reached this section, either we didn't manage to get a lock or the user cancelled
		uprintf("Could not get exclusive access to device %s: %d\n", Path, GetLastError());
		safe_closehandle(hDrive);
	}

out:
	return hDrive;
}

/*
* Return the path to access the physical drive, or NULL on error.
* The string is allocated and must be freed (to ensure concurrent access)
*/
char* GetPhysicalName(DWORD DriveIndex)
{
	BOOL success = FALSE;
	char physical_name[24];

	CheckDriveIndex(DriveIndex);
	safe_sprintf(physical_name, sizeof(physical_name), "\\\\.\\PHYSICALDRIVE%d", DriveIndex);
	success = TRUE;
out:
	return (success) ? safe_strdup(physical_name) : NULL;
}

/*
* Who would have thought that Microsoft would make it so unbelievably hard to
* get the frickin' device number for a drive? You have to use TWO different
* methods to have a chance to get it!
*/
int GetDriveNumber(HANDLE hDrive, char* path)
{
	STORAGE_DEVICE_NUMBER DeviceNumber;
	VOLUME_DISK_EXTENTS DiskExtents;
	DWORD size;
	int r = -1;

	if (!DeviceIoControl(hDrive, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0,
		&DiskExtents, sizeof(DiskExtents), &size, NULL) || (size <= 0) || (DiskExtents.NumberOfDiskExtents < 1)) {
		// DiskExtents are NO_GO (which is the case for external USB HDDs...)
		if (!DeviceIoControl(hDrive, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0,
			&DeviceNumber, sizeof(DeviceNumber), &size, NULL) || (size <= 0)) {
			uprintf("Could not get device number for device %s: %d\n", path, GetLastError());
			return -1;
		}
		r = (int)DeviceNumber.DeviceNumber;
	}
	else if (DiskExtents.NumberOfDiskExtents >= 2) {
		uprintf("Ignoring drive '%s' as it spans multiple disks (RAID?)\n", path);
		return -1;
	}
	else {
		r = (int)DiskExtents.Extents[0].DiskNumber;
	}
	if (r >= MAX_DRIVES) {
		uprintf("Device Number for device %s is too big (%d) - ignoring device\n", path, r);
		return -1;
	}
	return r;
}


/*
* Return a handle to the physical drive identified by DriveIndex
*/
HANDLE GetPhysicalHandle(DWORD DriveIndex, BOOL bWriteAccess, BOOL bLockDrive)
{
	HANDLE hPhysical = INVALID_HANDLE_VALUE;
	char* PhysicalPath = GetPhysicalName(DriveIndex);
	hPhysical = GetHandle(PhysicalPath, bWriteAccess, bLockDrive);
	safe_free(PhysicalPath);
	return hPhysical;
}

/*
* Return the next unused drive letter from the system
*/
char GetUnusedDriveLetter(void)
{
	DWORD size;
	char drive_letter = 'Z' + 1, *drive, drives[26 * 4 + 1];	/* "D:\", "E:\", etc., plus one NUL */

	size = GetLogicalDriveStringsA(sizeof(drives), drives);
	if (size == 0) {
		uprintf("GetLogicalDriveStrings failed: %d\n", GetLastError());
		goto out;
	}
	if (size > sizeof(drives)) {
		uprintf("GetLogicalDriveStrings: Buffer too small (required %d vs. %d)\n", size, sizeof(drives));
		goto out;
	}

	for (drive_letter = 'C'; drive_letter < 'Z'; drive_letter++) {
		for (drive = drives; *drive; drive += safe_strlen(drive) + 1) {
			if (!isalpha(*drive))
				continue;
			if (drive_letter == (char)toupper((int)*drive))
				break;
		}
		if (!*drive)
			break;
	}

out:
	return (drive_letter>'Z') ? 0 : drive_letter;
}


/*
* Mount partition #part_nr, residing on the same disk as drive_name (which must be
* of the form "X:" with no trailing slash) to an available
* drive letter. Returns a borrowed pointer to the newly-allocated drive letter of
* the form "Y:", which must not be freed, or NULL on error.
* We need to do this because, for instance, EFI System partitions are not assigned
* Volume GUIDs by the OS, and we need to have a letter assigned, for when we invoke
* bcdtool for Windows To Go. All in all, the process looks like this:
* 1. F: = \Device\HarddiskVolume9 (SINGLE LOOKUP)
* 2. Harddisk5Partition1 = \Device\HarddiskVolume9 (FULL LOOKUP)
* 3. Harddisk5Partition2 = \Device\HarddiskVolume10 (SINGLE LOOKUP)
* 4. DefineDosDevice(letter, \Device\HarddiskVolume10)
*/
char* AltMountVolume(const char* drive_name, uint8_t part_nr)
{
	static char mounted_drive[] = "?:";
	const DWORD bufsize = 65536;
	char *buffer = NULL, *p, target[2][MAX_PATH], *ret = NULL;
	size_t i;

	mounted_drive[0] = GetUnusedDriveLetter();
	if (mounted_drive[0] == 0) {
		uprintf("Could not find an unused drive letter\n");
		goto out;
	}

	target[0][0] = 0;
	// Convert our drive letter to something like "\Device\HarddiskVolume9"
	if (!QueryDosDeviceA(drive_name, target[0], MAX_PATH) || (strlen(target[0]) == 0)) {
		uprintf("Could not get the DOS volume name for '%s': %d\n", drive_name, GetLastError());
		goto out;
	}

	// Now parse the whole DOS device list to find the 'Harddisk#Partition#' that matches the above
	// TODO: realloc if someone ever manages to burst through 64K of DOS devices
	buffer = (char *) malloc(bufsize);
	if (buffer == NULL)
		goto out;

	buffer[0] = 0;
	if (!QueryDosDeviceA(NULL, buffer, bufsize)) {
		uprintf("Could not get the DOS device list: %d\n", GetLastError());
		goto out;
	}

	p = buffer;
	while (strlen(p) != 0) {
		if ((strncmp("Harddisk", p, 8) == 0) && (strstr(&p[9], "Partition") != NULL)) {
			target[1][0] = 0;
			if (QueryDosDeviceA(p, target[1], MAX_PATH) && (strlen(target[1]) != 0))
				if ((strcmp(target[1], target[0]) == 0) && (p[1] != ':'))
					break;
		}
		p += strlen(p) + 1;
	}

	i = strlen(p);
	if (i == 0) {
		uprintf("Could not find partition mapping for %s\n", target[0]);
		goto out;
	}

	while ((--i > 0) && (isdigit(p[i])));
	p[++i] = '0' + part_nr;
	p[++i] = 0;

	target[0][0] = 0;
	if (!QueryDosDeviceA(p, target[0], MAX_PATH) || (strlen(target[0]) == 0)) {
		uprintf("Could not find the DOS volume name for partition '%s': %d\n", p, GetLastError());
		goto out;
	}

	if (!DefineDosDeviceA(DDD_RAW_TARGET_PATH | DDD_NO_BROADCAST_SYSTEM, mounted_drive, target[0])) {
		uprintf("Could not mount '%s' to '%s': %d\n", target[0], mounted_drive, GetLastError());
		goto out;
	}

	uprintf("Successfully mounted '%s' (USB partition %d) as '%s'\n", target[0], part_nr, mounted_drive);
	ret = mounted_drive;

out:
	safe_free(buffer);
	return ret;
}

/*
* Unmount a volume that was mounted by AltmountVolume()
*/
BOOL AltUnmountVolume(const char* drive_name)
{
	if (drive_name == NULL)
		return FALSE;
	if (!DefineDosDeviceA(DDD_REMOVE_DEFINITION | DDD_NO_BROADCAST_SYSTEM, drive_name, NULL)) {
		uprintf("Could not unmount '%s': %d\n", drive_name, GetLastError());
		return FALSE;
	}
	uprintf("Successfully unmounted '%s'\n", drive_name);
	return TRUE;
}
