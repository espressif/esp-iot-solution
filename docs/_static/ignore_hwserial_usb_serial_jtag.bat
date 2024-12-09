REG ADD HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\usbflags\303A10010101 /V IgnoreHWSerNum /t REG_BINARY /d 01 && (
    REG ADD HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\usbflags\303A10010102 /V IgnoreHWSerNum /t REG_BINARY /d 01 && (
        echo Registry keys added successfully. Please restart your computer to apply the changes.
    ) || (
        echo Failed to add the second registry key. Please try again with Administrator privileges.
    )
) || (
    echo Failed to add the first registry key. Please try again with Administrator privileges.
)
pause
