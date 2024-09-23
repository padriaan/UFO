# UFO
Remake of Odyssey 2 UFO / Philips Videopac Satellite Attack
===========================================================

Remake of Philips Videopac    34 Satellite Attack
          Magnavox Osyssey 2  34 UFO!
          Originally released in 1981, programmed by Ed Averett.

Created with SDL 1.2 in C.          
Requirements (devel + libs):
- SDL 1.2 
- SDL_mixer
- SDL_ttf
- Images (bmp), sounds (wav), Font (o2.ttf modified)

Created by Peter Adriaanse july 2024.
- Version 0.9  SDL 1.2 (Linux + Windows)
- Version 0.91 Full screen support voor 1140p, 1080p en 768p
               (may not work for multi-monitor set-ups)

Press 1, 2 or 3 to start game.

Controls: Joystick or cursor keys + Ctrl for fire.
          Use Keypad - and Keypad + or [ and ] or 9 and 0 to change
          windows size. Use 8 to toggle full-screen on/off.
          Esc to quit from game. Esc in start-screen to quit all.
          Character keys for entering high score name. Return to complete.

Compile and link in Linux:
$ gcc -o ufo ufo.c -I/usr/include/SDL -lSDLmain -lSDL -lSDL_mixer -lSDL_ttf -lm

Windows (using MinGW):
gcc -o ufo.exe ufo.c -Lc:\MinGW\include\SDL  -lmingw32 -lSDLmain -lSDL -lSDL_mixer -lSDL_ttf

