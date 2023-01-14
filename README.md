# gears

Quick and ports of glxgears into different frameworks.
Made for testing and learning. 
Rough ports, missing features and things not working.

Compiled with e.g.

```
gcc -O2 -lGL -lm -lEGL -lX11 eglgears.c -o eglgears

gcc -O2 -lGL -lm -lglfw glfwgears.c -o glfwgears

gcc -O2 -lGL -lm -lSDL2 sdl2gears.c -o sdl2gears
```
