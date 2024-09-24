# UFO
Remake of 34: Odyssey 2 UFO / Videopac Satellite Attack
===========================================================

Remake of Philips Videopac Satellite Attack / Magnavox Osyssey 2 UFO!  
Originally released in 1981, programmed by Ed Averett.  

![VIDEOPAC34](https://github.com/user-attachments/assets/e59e81c8-26ad-4f07-9313-3080747de6ce)        ![ufo](https://github.com/user-attachments/assets/af2c81f1-2e0d-4c71-8937-58eef53f6a42)



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

Controls:  
- Joystick or cursor keys + Ctrl for fire.  
- Use Keypad - and Keypad + or [ and ] or 9 and 0 to change windows size.  
- Use 8 to toggle full-screen on/off.  
- Esc to quit from game. Esc in start-screen to quit all.  
- Character keys for entering high score name. Return to complete.

![game1](https://github.com/user-attachments/assets/556e0f5d-a472-4883-93a8-048692ff08f6)  ![game2](https://github.com/user-attachments/assets/76f38430-60d0-44a6-b49a-44ea599d2d79)



Compile and link from source
-----------------------------
First install a SDL 1.2 development environment and C-compiler.

Linux:  
$ gcc -o ufo ufo.c -I/usr/include/SDL -lSDLmain -lSDL -lSDL_mixer -lSDL_ttf -lm

Windows (using MinGW):  
gcc -o ufo.exe ufo.c -Lc:\MinGW\include\SDL  -lmingw32 -lSDLmain -lSDL -lSDL_mixer -lSDL_ttf

Run binary
------------
Download src and data folders. Extract data.zip (to get data/images and data/sounds).

Execute in Windows:   
double-click ufo.exe

Execute in Linux:   
$ export LD_LIBRARY_PATH=<folder where ufo/src/linux_libs is located>  
$ cd src  
$ chmod 644 ufo
$ ./ufo   
