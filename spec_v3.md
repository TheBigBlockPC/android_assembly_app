make fast mode run at native speed instead of the current slow run speed (currently running too slow)

add multiaple enviournments
*   default (current)
*   phone (has a taller screen but no keyboard)
*   custom

add the option to add virtual components that are documented but behave more like discoverable components that don't have a fixed adress and can be discovered using some device descriptor table at some adress with it's device type and base adress.

virtual components:
*   Minimal GPU (just a framebuffer or text display, like the current implementation)
*   Basic GPU (tile and sprite graphics)
*   Advanced GPU (has support for 3D graphis with configurable matrices and supports textured polygons and just colored polygons and has a depth buffer, for the interface there should be a command buffer)
*   Timer (gets the current millisecond)
*   Touch input controller (just gets the touch inputs on the screen)
*   Keyboard input controller
*   DMA controller (just a async memcopy from some adreess to another with some size)
*   RNG (it's just a RNG to get random numbers)
*   Non volatile memory (something that allows to read a 64 byte block from a given adress in nv memory)

UX improvements:
*   the UI components to run the code and change the machine shouldn't always be visible because it may just make the code hard to see
*   make the UI less cluttered

Examples:
*   a demo for each virtual component
*   a basic hello world

Major things:
*   the virtual components should be able to be added to the virtual device like building a virtual computer
*   projects should be able to be exported as just the machine, just the code or as a bundled project file