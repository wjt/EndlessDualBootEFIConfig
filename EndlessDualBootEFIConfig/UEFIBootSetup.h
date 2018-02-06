#pragma once

enum UEFIBootSetupFlags {
	SetBootNext = 1 << 0,
	ReplaceWindowsEntry = 1 << 1
};

bool EFIRequireNeededPrivileges();
bool EFICreateNewEntry(const wchar_t *drive, wchar_t *path, const wchar_t *desc, int flags = 0);
bool EFIRemoveEntry(const wchar_t *desc, bool &found_entry);

int print_entries(void);