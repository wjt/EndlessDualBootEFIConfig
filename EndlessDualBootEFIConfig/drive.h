#pragma once

#include "stdint.h"

#define DRIVE_ACCESS_TIMEOUT        15000		// How long we should retry drive access (in ms)
#define DRIVE_ACCESS_RETRIES        60			// How many times we should retry
#define DRIVE_INDEX_MIN             0x00000080
#define DRIVE_INDEX_MAX             0x000000C0
#define MAX_DRIVES                  (DRIVE_INDEX_MAX - DRIVE_INDEX_MIN)

int GetDriveNumber(HANDLE hDrive, char* path);
HANDLE GetPhysicalHandle(DWORD DriveIndex, BOOL bWriteAccess, BOOL bLockDrive);
char* AltMountVolume(const char* drive_name, uint8_t part_nr);
BOOL AltUnmountVolume(const char* drive_name);