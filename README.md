This is a standalone version of the EFI boot order manipulation code from [the Endless Installer for Windows](https://github.com/endlessm/rufus), intended for debugging problems on certain systems. It is intended to be run from a command prompt as an administrator, on a system which **does not have Endless OS installed**. It must be placed in the same directory as the files in `data/` in this repository.

It performs the following actions:

* Copies a stub GRUB config to `C:\endless\grub\grub.cfg`
* Mounts the ESP on the same physical disk as the partition mounted at `C:`
* Copies `shim.efi` and `grubx64.efi` to `\EFI\EndlessTest` on the ESP
* Prints the parsed contents of the `BootOrder` EFI variable, and all `BootXXXX` variables listed in it
* Creates a new `BootXXXX` variable pointing to `\EFI\EndlessTest\shim.efi`, and prepends it to `BootOrder`
* Prints `BootOrder` and `BootXXXX` again

At this point, rebooting your system should give you a GRUB menu with two options:

* Microsoft Windows (which boots Windows when selected)
* It worked (which shows a brief message when selected)

If the system boots straight to Windows, our boot order manipulation has not worked. It may be interesting to run the script a second time in this case, to see the post-reboot contents of `BootOrder` and `BootXXXX`.

The best way I have found to capture its output is to run it in Windows PowerShell like this:

```
.\EndlessDualBootEFIConfig.exe | tee output.txt -Append
```
