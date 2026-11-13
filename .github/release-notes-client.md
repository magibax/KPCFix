Built from the exact tagged source after binary reproduction and checksum verification. Follow README.md for installation.

## Uninstall

1. Close the game.
2. Optionally, restore the configuration files changed by the first launch. This returns them to how they were before KPCFix, so any later changes to those files are lost:
   ```
   powershell -ExecutionPolicy Bypass -File Setup-KPCFix.ps1 -Restore
   ```
3. Delete the files KPCFix added: `dinput.dll`, `ddraw.dll`, `KPCFix.ini`, `KPCFix.log`, `DDrawCompat-CLIENT.ini`, `Launch-KPCFix.cmd`, `Setup-KPCFix.ps1`, `Check-KPCVersion.cmd`, `Check-KPCVersion.ps1`, `SUPPORTED_BUILDS.json`, `SHA256SUMS`, `README.md`, `LICENSE`, the `LICENSES` folder and `KPCFix-Display-Preset.applied`. If you skipped step 2, also delete `KPCFix-Display-Backups.json` and any `.kpcfix-display.bak` files.
4. Restore any DLLs or INIs you backed up during install.
