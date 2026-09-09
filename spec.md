a android app that allows to wriute and run ARM64 assembly

UI:
*   fitting to the system theme (light / dark mode)
*   funt size config for docs and editor

code editor:
*   a default basic hello world program on startup
*   syntax highlighting and lint (invallid instruction names are high lighted red)
*   code suggetion while typing for example if i type "ld" it suggests "ldr   [register], [<address>]", obviously in a darker tone to seperate from written code and cutt off properly
*   a help/info button to quickly pull up the instruction and have them sorted by purpose (math, branching, comparison, ...) and type (int64, int32, float, ...)
*   a way to save the code

code execution UI:
*   a 320x240 framebuffer
*   a virtual keyboard
*   touchscreen support for the vm but just 1 position (no fancy multi touch)
*   a debugger with breakpoints that you can set in the code editor like in most IDEs
*   a tab to inspect memory

VM:
*   **no** real linux syscalls/ calls to the OS
*   native ARM code execution
*   defined and deterministic memory map
*   custom syscalls to set the graphics mode (text with ANSI colors, RGB565, 32bit RGB)
*   memory mapped registers for the pressed keys on the virtual keyboard and touch location (x,y) on the virtual screen

ETC:
*   multiaple example projects, hello world, graphics, touch
*   in app documentation with a full memory map

programming language used to build the app: mostly C++

important consideration: safe handling of bad code (out of bounds writes, loops, ...), the app shouldn't crash