Redux has a built-in shell. It can run basic commands and programms

## Commands
The commands available for the built-in shell are:
help - Shows the available commands.
ver - Shows the current Redux version.
echo <text> - Repeats a line of text back to you. Example. In: echo Hi! Out: Hi!
clear \ cls - Clears the screen.
dir <dir> - Shows the current directory files
cd <dir> - Not implemented as of 0.0.2, but will be added in Alpha 1 Milestone 1 after Ring 3 program loading.
run <file> - Runs a file. When Ring 3 comes out, will be reserved for ring 3. At the moment, runs an ELF file in ring 0.

## Other Shells
If you'd like to use another shell, please wait until Alpha 1.  In Alpha 1, I will be adding a boot.cfg file in the System directory, and that will allow you to set an elf to load, if not set or not found, will boot into the default shell
