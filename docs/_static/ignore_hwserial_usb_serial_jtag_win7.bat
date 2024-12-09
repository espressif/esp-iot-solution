REG ADD HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\UsbFlags\IgnoreHWSerNum303A1001
if %errorlevel% equ 0 (
    echo Registry keys added successfully. Please restart your computer to apply the changes.
) else (
    echo Failed to add the first registry key. Please try again with Administrator privileges.
)
pause
