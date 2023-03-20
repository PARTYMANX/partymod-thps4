PARTYMOD THPS4 1.0

This is a patch for THPS4 to improve its input handling as well as smooth out a few other parts of the PC port.
The patch is designed to keep the game as original as possible, and leave its files unmodified.

Features and Fixes:
- Replaced input system entirely with new, modern system using the SDL2 library
- Added option to use original PS2-style control binds (Note: when not using PS2 controls, nollie and spin left are joined as well as switch and spin right, switch is bound to spine transfer which is just nollie+switch)
- Controller vibration now more consistently works with controllers
- Improved window handling allowing for custom resolutions and configurable windowing
- Fixed aspect ratio to be based on window dimensions
- Added option to disable gamma correction in fullscreen mode to prevent the game from looking unexpectedly dark
- Replaced configuration files with new INI-based system (see partymod.ini)
- Custom configurator program to handle new configuration files
- Fixed ledge warp bugs where the skater is teleported down farther than intended
- Fixed visually incorrect screen flash (I.E. when running into a lion on Zoo)
- Fixed broken skater friction when autokick was enabled
- Added a framerate cap to fix logic depending on a framerate of 60hz (long trick point accrual, special meter depreciation, goal logic like SF Manual Rollercoaster)
- fixes music randomization (introduces entropy *only* for music randomization and consumes the same number of random values to be safe)

INSTALLATION:
1. Make sure THPS4 (English) is installed, remove the widescreen mod if it is installed (delete dinput8.dll)
2. Extract this zip folder into your THPS4 installation directory
3. Run partypatcher.exe to create the new, patched THPS4.exe game executable (this will be used to launch the game from now on) (this only needs to be done once)
4. Optionally (highly recommended), configure the game with partyconfig.exe
5. Launch the game from THPS4.exe

NOTE: if the game is installed into the "Program Files" directory, you may need to run each program as administrator. 
Also, if the game is installed into the "Program Files" directory, save files will be saved in the C:\Users\<name>\AppData\Local\VirtualStore directory.  
For more information, see here: https://answers.microsoft.com/en-us/windows/forum/all/please-explain-virtualstore-for-non-experts/d8912f80-b275-48d7-9ff3-9e9878954227