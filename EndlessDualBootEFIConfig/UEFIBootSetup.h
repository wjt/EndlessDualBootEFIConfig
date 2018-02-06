#pragma once

bool EFIRequireNeededPrivileges();
bool EFICreateNewEntry(const wchar_t *drive, wchar_t *path, const wchar_t *desc);
bool EFIRemoveEntry(const wchar_t *desc, bool &found_entry);

int print_entries(void);