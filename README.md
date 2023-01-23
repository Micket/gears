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

## Notes

Controlling EGL device in different frameworks;

```bash
export SDL_HINT_EGL_DEVICE=x
export VTK_EGL_DEVICE_INDEX=x
```

No support of EGLDevice GLFW?

