/*******************************************************************************************
ufo.c 

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

***********************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#ifdef _WIN32
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>
#include <SDL/SDL_ttf.h>
#endif

#ifndef _WIN32
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#endif

/* constants */
#define DATA_PREFIX "../data/"
#define MAX_STAR_SIZE 4
#define SECTOR_SIZE 20            // for stars
#define NUM_IMAGES 72

#define SHIP_W 8                  // ship width in pixels (factor 1)
#define SHIP_H 4                  // ship height in pixels (factor 1)
#define SHIELD_BITS 15
#define SHIELD_BIT_TIMER 40
#define SHIP_EXPLOSIONS 115       // nr of animations in exploding ship sequence
#define MAX_BULLETS 3

#define ASTEROID_SHAPE_TIMER 6    // must be an even integer for displaying image  
                                  // during multiple frames
#define MAX_MINI_EXPLOSIONS 10    // should be enough
#define VIDEOPAC_RES_W 200        // original console screen resolution width
#define VIDEOPAC_RES_H 160        // original console screen resolution height
#define JOYSTICK_DEAD_RANGE 8000  // dead-range - and + for analog joystick

#define NUM_SOUNDS 11

/* globals used for difficulty levels */
int difficulty = 1;               // 1=normal, 2=hard, 3=insane
int MAX_UFOS = 1;                 // normal difficulty
int MAX_LASERS = 1;               // normal difficulty, 1 ufo 1 laser
int MAX_ASTEROIDS = 15;           // both normal and magnetic on screen
                                  // normal difficulty
int UFO_RANDOMNESS = 250;         // how often an ufo will spawn (lower is more frequent)


/* typedef for shield (15 pixels) 
   status: 0=off (black), 1=off and recharching (grey), 2=on (blue) 
   timer:  ticks from off to on (recharching time) */
typedef struct shield_bit_type {
  int status, timer_bit, img_grey, img_blue, img_white, gun;
} shield_bit_type;

/* typedef for bullets/explosion bits (3 x 1 pixel) */
typedef struct bullet_type {
  int alive, timer, x, y; 
  float xm, ym;
} bullet_type;

/* typedef voor astroids/explosions (6 x 5 pixel) */
typedef struct asteroid_type {
  int colour,            // colour 1=cyan, 2=magenta, 3=blue, 4=green, 
                         //        5=yellow, 6=grey/white, 7=red
      status,            // 0 = not active, 1 = normal, 2 = magnetic, 3 is exploding
      shape_timer,       // timer in frames for duration of shape (x, + or cirkel)
                         // 1 ... ASTEROID_SHAPE_TIMER/2 for first shape
                         // ASTEROID_SHAPE_TIMER/2 + 1 .. ASTEROID_SHAPE_TIMER for second
      magnetic_timer;    // for displaying alternate + en x in megnetic asteroid (0..5)
  float x, y, xm, ym;    // coordinates x,y ; speed is included in xm,ym
} asteroid_type;

/* typedef for mini explosion */
typedef struct mini_explosion_type {
  int alive, timer, x, y;
} mini_explosion_type;

/* typedef for animation ship explosions */
typedef struct ship_explosion_type {
  int img_nr, ship_nr, color_nr;
} ship_explosion_type;

/* typedef voor ufo (8w x 2h pixel) */
typedef struct ufo_type {
  int colour,             // colour 1=cyan, 2=magenta, 3=blue, 4=green, 
                         //        5=yellow, 6=grey/white, 7=red
      shape_timer,       // timer in frames for duration of explosion
      status;            // 0 = not active, 1 = active, 3 is exploding
  float x, y, xm, ym;    // coordinates x,y ; speed is included in xm,ym
} ufo_type;

/* typedef for laser for ufo (7w x 8h pixels) */
typedef struct laser_type {
  int alive, fired_by_ufo, x, y, xm, ym;        // fired_by_ufo : ufo id 
} laser_type;


/* global variables, initialized in setup() */
int screen_width  = 1000;        // initial factor 5 (Videopac 5x200)
int screen_height = 800;         // initial factor 5 (Videopac 5x160)
int factor = 5;                  // resize factor (relative to 200x160 screen resolution)
int full_screen = 0;
const SDL_VideoInfo * d_monitor; // pointer to current monitor details

int use_joystick;
int num_joysticks;
int joy_left, joy_right, joy_up, joy_down;

int ship_x ;                // ship x-coordinate
int ship_y;                 // ship y-coordinate
int speed;                  // ship ship in pixels per frame
int gun_bit;                // starting gun_bit (range 0..14)
int ship_window_step;       // animation step of ship's window 1..12
int ship_explosion_nr;      // nr of active ship explosion sequence 0..8
int ship_dying;             // ship is hit and dying  0=no, 1=yes
int ship_destroyed;         // ship destroyed  0=no, 1=yes
int bullet_frame;           // frame no at wich last bullet was fired
int recharge_active;        // 0=not, 1= recharging
int recharge_sound_delay;   // to delay sound of recharging (respawn shield)

int high_score_broken;      // 0-not, 1 = true
int high_score_registration; // 0: not possible, 1: active
int high_score_character_pos;  // 0...6 active to enter
int score, high_score;
char high_score_name[7];    // max length is 6 charachters! 
int flash_high_score_timer; // 0..150 frames reverse

int frame;
int ufo_start_delay;        // used to delay first ufo on screen

shield_bit_type shield_bits[SHIELD_BITS];
bullet_type bullets[MAX_BULLETS];

mini_explosion_type mini_explosions[MAX_MINI_EXPLOSIONS];
ship_explosion_type ship_explosions[SHIP_EXPLOSIONS];

ufo_type ufo[3];             // MAX depending on difficulty selection
laser_type laser[3];         // MAX depending on difficulty selection
asteroid_type asteroids[35]; // MAX depending on difficulty selection

int vol_effects, vol_music;
Mix_Chunk * sounds[NUM_SOUNDS];

SDL_Joystick *js;

/* DATA_PREFIX (defined in Makefile)   do not change the order/name/location of images
                                     add new images at end                     */
const char * image_names[NUM_IMAGES] = {
  DATA_PREFIX "images/asteroids/asteroid_x_cyan.bmp",       // entry = 0  not used
  DATA_PREFIX "images/asteroids/asteroid_plus_cyan.bmp",    // not used
  DATA_PREFIX "images/asteroids/asteroid_ball_cyan.bmp",    // not used
  DATA_PREFIX "images/ship/dummy.bmp",                      // not used
  DATA_PREFIX "images/ship/ship3.bmp",                      // not used
  DATA_PREFIX "images/ship/ship3_factor",                   // ship        
  DATA_PREFIX "images/ship/ship3_factor5.bmp",              // not used
  DATA_PREFIX "images/pixels/blue_pixels.bmp",              // 7: shield, max 9x9 pixels
  DATA_PREFIX "images/pixels/black_pixels.bmp",             // 8: shield, max 9x9 pixels
  DATA_PREFIX "images/pixels/grey_pixels.bmp",              // 9: shield, max 9x9 pixels
  DATA_PREFIX "images/pixels/white_pixels.bmp",             // 10: shield or bullet, max 9x9 pixels
  DATA_PREFIX "images/asteroids/asteroid_x_cyan_factor",    // 11
  DATA_PREFIX "images/asteroids/asteroid_plus_cyan_factor",
  DATA_PREFIX "images/asteroids/asteroid_ball_cyan_factor",
  DATA_PREFIX "images/asteroids/asteroid_x_magenta_factor", 
  DATA_PREFIX "images/asteroids/asteroid_plus_magenta_factor",
  DATA_PREFIX "images/asteroids/asteroid_ball_magenta_factor",  
  DATA_PREFIX "images/asteroids/asteroid_x_blue_factor", 
  DATA_PREFIX "images/asteroids/asteroid_plus_blue_factor",
  DATA_PREFIX "images/asteroids/asteroid_ball_blue_factor",  
  DATA_PREFIX "images/asteroids/asteroid_x_green_factor", 
  DATA_PREFIX "images/asteroids/asteroid_plus_green_factor",
  DATA_PREFIX "images/asteroids/asteroid_ball_green_factor",  
  DATA_PREFIX "images/asteroids/asteroid_x_yellow_factor", 
  DATA_PREFIX "images/asteroids/asteroid_plus_yellow_factor",
  DATA_PREFIX "images/asteroids/asteroid_ball_yellow_factor",  
  DATA_PREFIX "images/asteroids/asteroid_x_grey_factor", 
  DATA_PREFIX "images/asteroids/asteroid_plus_grey_factor",
  DATA_PREFIX "images/asteroids/asteroid_ball_grey_factor",  
  DATA_PREFIX "images/asteroids/asteroid_x_red_factor", 
  DATA_PREFIX "images/asteroids/asteroid_plus_red_factor",
  DATA_PREFIX "images/asteroids/asteroid_ball_red_factor",           //31
  DATA_PREFIX "images/explosions/asteroid_explosion1_grey_factor",   //32
  DATA_PREFIX "images/explosions/asteroid_explosion2_grey_factor",
  DATA_PREFIX "images/explosions/asteroid_explosion3_grey_factor",
  DATA_PREFIX "images/explosions/asteroid_explosion4_grey_factor",
  DATA_PREFIX "images/explosions/asteroid_explosion5_grey_factor",  
  DATA_PREFIX "images/explosions/mini_explosion_yellow_factor",    // 37
  DATA_PREFIX "images/explosions/mini_explosion_grey_factor", 
  DATA_PREFIX "images/explosions/ship_explosion1_green_factor",    //39 
  DATA_PREFIX "images/explosions/ship_explosion2_green_factor",
  DATA_PREFIX "images/explosions/ship_explosion3_green_factor",
  DATA_PREFIX "images/explosions/ship_explosion4_green_factor", 
  DATA_PREFIX "images/explosions/ship_explosion1_blue_factor",      //43
  DATA_PREFIX "images/explosions/ship_explosion2_blue_factor",
  DATA_PREFIX "images/explosions/ship_explosion3_blue_factor",
  DATA_PREFIX "images/explosions/ship_explosion4_blue_factor",
  DATA_PREFIX "images/explosions/ship_explosion1_cyan_factor",      //47
  DATA_PREFIX "images/explosions/ship_explosion2_cyan_factor",
  DATA_PREFIX "images/explosions/ship_explosion3_cyan_factor",
  DATA_PREFIX "images/explosions/ship_explosion4_cyan_factor",
  DATA_PREFIX "images/explosions/ship_explosion1_grey_factor",      //51
  DATA_PREFIX "images/explosions/ship_explosion2_grey_factor",
  DATA_PREFIX "images/explosions/ship_explosion3_grey_factor",
  DATA_PREFIX "images/explosions/ship_explosion4_grey_factor",
  DATA_PREFIX "images/ship/ship3_green_factor",              // 55
  DATA_PREFIX "images/ship/ship3_blue_factor",               // 56
  DATA_PREFIX "images/ship/ship3_cyan_factor",               // 57
  DATA_PREFIX "images/ship/ship3_grey_factor",               // 58
  DATA_PREFIX "images/pixels/green_bits.bmp",  // 59: bits ship expl. max 9x9 pixels
  DATA_PREFIX "images/pixels/blue_bits.bmp",   // 60: bits ship expl. max 9x9 pixels
  DATA_PREFIX "images/pixels/cyan_bits.bmp",   // 61: bits ship expl. max 9x9 pixels
  DATA_PREFIX "images/pixels/grey_bits.bmp",   // 62: bits ship expl. max 9x9 pixels  
  DATA_PREFIX "images/ufo/ufo_grey_factor",    // 63: ufo
  DATA_PREFIX "images/ufo/ufo_magenta_factor",      
  DATA_PREFIX "images/ufo/ufo_green_factor",      
  DATA_PREFIX "images/ufo/ufo_blue_factor",      
  DATA_PREFIX "images/ufo/ufo_red_factor",      
  DATA_PREFIX "images/ufo/ufo_cyan_factor",      
  DATA_PREFIX "images/ufo/ufo_yellow_factor",      
  DATA_PREFIX "images/lasers/laser_left_factor",   // 70: laser
  DATA_PREFIX "images/lasers/laser_right_factor"   // 71
};  

const char * sound_names[NUM_SOUNDS] = {
  DATA_PREFIX "sounds/respawn.wav",        // 0
  DATA_PREFIX "sounds/gun_rotate.wav",
  DATA_PREFIX "sounds/bullet.wav",
  DATA_PREFIX "sounds/explosion.wav",
  DATA_PREFIX "sounds/mini_explosion.wav",
  DATA_PREFIX "sounds/ship_explosion.wav",
  DATA_PREFIX "sounds/score.wav",
  DATA_PREFIX "sounds/character_beep.wav",  // 7 
  DATA_PREFIX "sounds/laser.wav",
  DATA_PREFIX "sounds/ufo.wav",
  DATA_PREFIX "sounds/select_game.wav"                 // 10
};

SDL_Surface * screen;
SDL_Surface * images[NUM_IMAGES];

/* global font variables */
SDL_Surface * text;
TTF_Font * font_large;
TTF_Font * font_small;
int font_size;

/* forward declarations of functions/procedures */
void title_screen();
void display_select_game(int x, int y);
void display_instructions(int scroll_x, int scroll_y);
int game(int mode);
void setup(void);
void setup_joystick();
void start_new_game();
void setup_ship_explosions();
void load_images();
int get_user_input();
void cleanup();
void handle_screen_resize(int mode);
void draw_stars();

void draw_ship();
void draw_shield_bits();
void setup_shield_bits();
void handle_shield_bits();

void add_bullet(int xx, int yy);
void handle_bullets();
void draw_bullets();
void rotate_gun_bit();

void add_laser(int xx, int yy, int xxm, int yym, int ufo);
void handle_lasers();
void draw_lasers();
void check_laser_hit();

void add_asteroid();
void handle_asteroids();
void draw_asteroids();
void check_ship_collision();
void check_bullet_hit();
void check_colliding_asteroids();
void check_asteroid_positions();
void draw_mini_explosions();
void add_mini_explosion(int x, int y);

void draw_ufo();
void add_ufo();
void handle_ufo();

void draw_score_line();
void flash_high_score_name();
void print_high_score_char(int character);
void draw_ship_explosions();
int getStarColor(int);
void play_sound(int snd, int chan);


/* ------------ 
   -   MAIN   - 
   ------------ */

int main(int argc, char * argv[])
{
  int mode, quit;
  printf("Start\n");

  /* Stop any music: */
  Mix_HaltMusic();       

  setup();
  setup_ship_explosions();

  /* Call the cleanup function when the program exits */
  atexit(cleanup);


  /* Main loop */
  do
  {  
    title_screen();
    quit = game(mode);
  }  
  while (quit == 0);

  SDL_Quit();
  exit(0);
}


int game(int mode)
{
  int done, quit;
  Uint32 last_time;
   
  frame = 0;
  bullet_frame = 0;
  done = 0;
  quit = 0;
  ship_dying = 0;
  ship_destroyed = 0;
  high_score_broken = 0;
  high_score_registration = 0;

  start_new_game();
  recharge_sound_delay = 0;  // initially off; no recharge delay

  /* ------------------
     - Main game loop -
     ------------------ */
  do
  {
      last_time = SDL_GetTicks();
      frame++;

      SDL_Flip(screen);

      /* restart_game after death */
      if (ship_destroyed == 1 && flash_high_score_timer == 0) {
        if (high_score_broken == 1) {
            high_score_registration = 1;   // new name can be entered
            high_score_character_pos = 0;
            //printf("Name can be entered\n");
        }   
        start_new_game();
      }  

      done = get_user_input();   
      SDL_FillRect(screen, NULL, 0);  /* Blank the screen */
    
      draw_stars();
      if (ship_dying == 0) draw_ship();
      if (ship_dying == 1 && ship_destroyed == 0) draw_ship_explosions();
      if (ship_dying == 0) handle_shield_bits();
      if (ship_dying == 0) draw_shield_bits();
      handle_bullets();
      draw_bullets();
      handle_lasers();
      draw_lasers();
      handle_asteroids();
      if (rand() % 20 == 1 ) add_asteroid(); 
      draw_asteroids();

      handle_ufo();
      draw_ufo();

      if (ship_dying == 0) check_bullet_hit();    
      if (ship_dying == 0) check_ship_collision();
      if (ship_dying ==0 ) check_laser_hit();
      draw_mini_explosions();
      if (ship_dying == 0) check_colliding_asteroids();
      check_asteroid_positions();
      draw_score_line(); 

      /* Pause till next frame: */
      //printf("Delay: %d - \n", last_time + 33 - SDL_GetTicks());
      if (SDL_GetTicks() < last_time + 33)
          SDL_Delay(last_time + 33 - SDL_GetTicks());
    }
  while (!done && !quit);
  
  return(0);
}


void setup(void)
{
  int i;
  char title_string[100];

  /* Init SDL Video: */
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
      fprintf(stderr,
              "\nError: I could not initialize video!\n"
              "The Simple DirectMedia error that occured was:\n"
              "%s\n\n", SDL_GetError());
      exit(1);
    }

  /* enable console output in Windows */
  #ifdef _WIN32
  freopen("CON", "w", stdout); // redirects stdout
  freopen("CON", "w", stderr); // redirects stderr
  #endif


  /* get monitor resolution */
  d_monitor = SDL_GetVideoInfo();
  printf("Max monitor resolution w, h: %d, %d \n",d_monitor->current_w, d_monitor->current_h);
   
  /* define factor */
  if (d_monitor->current_h >= 1080) factor = 5;   // 1080p and higer
  if (d_monitor->current_h <= 768)  factor = 3;   // 768p and lower
  printf("Factor is: %d \n", factor);
  screen_width  = VIDEOPAC_RES_W * factor;
  screen_height = VIDEOPAC_RES_H * factor;

  /* Open display: */
      screen = SDL_SetVideoMode(screen_width, screen_height, 0, SDL_HWPALETTE | SDL_ANYFORMAT);
      if (screen == NULL)
        {
          fprintf(stderr,
                  "\nWarning: I could not set up video for "
                  "1000x800 mode.\n"
                  "The Simple DirectMedia error that occured was:\n"
                  "%s\n\n", SDL_GetError());
        } 


   /* Init True Type Font */
   if (TTF_Init() < 0) 
   { 
      fprintf(stderr, "Impossible to initialize SDL_TTF: %s\n",SDL_GetError() );
      exit(1);
   }
   font_size = 12 * factor;
   font_large = TTF_OpenFont("O2.ttf", font_size);
   if (!font_large)
      fprintf(stderr, "Cannot load font name O2.ttf large: %s\n", SDL_GetError());

   font_small = TTF_OpenFont("O2.ttf", font_size/2);
   if (!font_small)
      fprintf(stderr, "Cannot load font name O2.ttf small: %s\n", SDL_GetError());

   high_score = 0; 
   strcpy(high_score_name, "??????");


  setup_joystick();

  /* Set window manager stuff: */
  sprintf(title_string, "UFO - factor: %d - difficulty: %d", factor, difficulty);
  SDL_WM_SetCaption(title_string, "UFO");

  load_images();


  /* Open sound */

  if (Mix_OpenAudio(22050, AUDIO_S16, 1, 1024) < 0) {
         fprintf(stderr,
          "\nWarning: I could not set up audio for 22050 Hz "
          "16-bit stereo.\n"
          "The Simple DirectMedia error that occured was:\n"
          "%s\n\n", SDL_GetError());
          exit(1);
  }
        
  vol_effects = 5;
  vol_music = 5;
  
  Mix_Volume(-1, vol_effects * (MIX_MAX_VOLUME / 5));
  Mix_VolumeMusic(vol_music * (MIX_MAX_VOLUME / 5));

  Mix_AllocateChannels(32);

 /* Load sounds */
      
 for (i = 0; i < NUM_SOUNDS; i++) {
    sounds[i] = Mix_LoadWAV(sound_names[i]);
    if (sounds[i] == NULL)
      {
        fprintf(stderr,
          "\nError: I could not load the sound file:\n"
          "%s\n"
          "The Simple DirectMedia error that occured was:\n"
          "%s\n\n", sound_names[i], SDL_GetError());
        exit(1);
      }
    }  
}   


void setup_joystick()
{
  use_joystick = 1;
  num_joysticks = 0;

  if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
      fprintf(stderr,
        "\nWarning: I could not initialize joystick.\n"
        "The Simple DirectMedia error that occured was:\n"
        "%s\n\n", SDL_GetError());
      
      use_joystick = 0;
  }
  else
    {
      /* Look for joysticks */
      
      num_joysticks = SDL_NumJoysticks();
      if (num_joysticks <= 0) {
         fprintf(stderr, "\nWarning: No joysticks available.\n");
      use_joystick = 0;
      }
      else
  {
    /* Open joystick */
    
    js = SDL_JoystickOpen(0);
    if (js == NULL) {
        fprintf(stderr,
          "\nWarning: Could not open joystick 1.\n"
          "The Simple DirectMedia error that occured was:\n"
          "%s\n\n", SDL_GetError());
         use_joystick = 0;
      }
    else
      {
        /* Check for proper stick configuration: */
        
        if (SDL_JoystickNumAxes(js) < 2) {
      fprintf(stderr, "\nWarning: Joystick doesn't have enough axes!\n");
      use_joystick = 0;
    }
        else
    {
      if (SDL_JoystickNumButtons(js) < 2) {
          fprintf(stderr,
            "\nWarning: Joystick doesn't have enough "
            "buttons!\n");
          use_joystick = 0;
        }
    }
    }
  }
  }

}


void start_new_game() 
{
  int i;

  ship_destroyed = 0;
  ship_dying = 0;
  score = 0;
  high_score_broken = 0;


  ship_x = screen_width / 2;
  ship_y = screen_height / 2;
  speed = 10;
  joy_left = 0;
  joy_right = 0;
  joy_up = 0;
  joy_down = 0;
  
  recharge_active = 1;       // initially on at startup
  recharge_sound_delay = 1;  // initially off; no recharge delay

  gun_bit = 4;                    // initial gun bit position
  ship_window_step = 1;

  setup_shield_bits();

  /* init bullets off */
  for (i = 0; i < MAX_BULLETS; i++)
      bullets[i].alive = 0;

  /* init asteroids off */
  for (i = 0; i < MAX_ASTEROIDS; i++)
      asteroids[i].status = 0;

  /* init mini_explosions off */
  for (i = 0; i < MAX_MINI_EXPLOSIONS; i++)
      mini_explosions[i].alive = 0;

  /* init ufo off */
  for (i = 0; i < MAX_UFOS; i++)
      ufo[i].status = 0;

  /* init ufo laser off */
  for (i = 0; i < MAX_LASERS; i++) {
      laser[i].alive = 0;
      laser[i].fired_by_ufo = -1;
  }    
  ufo_start_delay = frame;

  printf("New game\n");
}


void setup_ship_explosions()
{
  int i, j;

  for (j = 0; j < 4; j++) {  // 4/8: first 4 color cycls, 
                             // green, blue, cyan, grey, groen, blauw, cyan grey
                             //    1    2      3    4     5      6      7    8


    for (i = 0; i < 9; i++) {  // first 9 animation sets 1 color
       if (i < 4) { ship_explosions[i+(j*9)].img_nr = (39 + i) + (j * 4);
       } else {     ship_explosions[i+(j*9)].img_nr = 0; }

       ship_explosions[i+(j*9)].ship_nr  = 55 + j;         
       ship_explosions[i+(j*9)].color_nr = j + 1 ;     // green
     }    
  }


  for (j = 0; j < 4; j++) {  // 4/8: last 4 color cycls, 
                             // green, blue, cyan, grey, groen, blauw, cyan grey 
                             //    1    2      3    4     5      6      7    8


    for (i = 0; i < 9; i++) {  // first 9 animation sets 1 color
       if (i < 4) { ship_explosions[i+(j*9) + 36].img_nr = (39 + i) + (j * 4);
       } else {     ship_explosions[i+(j*9) + 36].img_nr = 0; }

       ship_explosions[i+(j*9) + 36].ship_nr  = 55 + j;         
       ship_explosions[i+(j*9) + 36].color_nr = j + 1 ;     // green
     }    
  }

  /* make duplicate of all above explosions */

  for (i = 0; i < 36; i++) {
     ship_explosions[72 + i].img_nr   = ship_explosions[i].img_nr;
     ship_explosions[72 + i].ship_nr  = ship_explosions[i].ship_nr;
     ship_explosions[72 + i].color_nr = ship_explosions[i].color_nr;
  }

  /* last animation, no ship, no explosion, only bullets (grey) */
  for (i = 108; i < 115; i++) {  
     ship_explosions[i].img_nr   = 0;
     ship_explosions[i].ship_nr  = 0;         
     ship_explosions[i].color_nr = 4;     // grey
  }    
}


void load_images(void)
{
  int i;
  char image_string[200];
  char temp_string[200];
  SDL_Surface * image;


  for (i = 0; i <  NUM_IMAGES; i++)  
  {
    if (i == 5                             // ship image
         || (i >= 11 && i <= 38)           // asteroids + mini exp.
         || (i >= 55 && i <= 58)           // ship color
         || (i >= 39 && i <= 54)           // ship explosions 
         || (i >= 63 && i <= 71)) {        // ufo, lasers
      
      // load factor image                    + ship explosion
      sprintf(image_string, "%s", image_names[i] );
      sprintf(temp_string, "%d", factor);
      strcat(image_string, temp_string);
      strcat(image_string, ".bmp");

      image = SDL_LoadBMP(image_string);
    } else {  
        strcpy(image_string, image_names[i] );
        image = SDL_LoadBMP(image_string);
    }

    if (image == NULL)
    {
      fprintf(stderr,
        "\nError: I couldn't load a graphics file:\n"
        "%s\n"
        "The Simple DirectMedia error that occured was:\n"
        "%s\n\n", image_string, SDL_GetError());
      exit(1);
    }
      
    /* Convert to display format: */
    images[i] = SDL_DisplayFormat(image);
    if (images[i] == NULL)
    {
        fprintf(stderr,
                "\nError: I couldn't convert a file to the display format:\n"
                "%s\n"
                "The Simple DirectMedia error that occured was:\n"
                "%s\n\n", image_names[i], SDL_GetError());
        exit(1);
    }

    /* Set transparency: */
    if (i != 10)       // do not set transparency for white gun bit and bullets
    {  
       if (SDL_SetColorKey(images[i], (SDL_SRCCOLORKEY | SDL_RLEACCEL),
               SDL_MapRGB(images[i] -> format,
               0xFF, 0xFF, 0xFF)) == -1)
        {
           fprintf(stderr,
             "\nError: I could not set the color key for the file:\n"
             "%s\n"
             "The Simple DirectMedia error that occured was:\n"
             "%s\n\n", image_names[i], SDL_GetError());
           exit(1);
        }
    }

  }  // end for loop
}  


int get_user_input()
{
  SDL_Event event;
    Uint8* keystate = SDL_GetKeyState(NULL);
    int rotate_gun = 0;
    int window_size_changed = 0;
    char title_string[100];
    int i;

    // Handle decrease / enlarge window (keypad + -)
    if (full_screen == 0) {
       
       if ( (keystate[SDLK_KP_MINUS] || keystate[SDLK_LEFTBRACKET] || keystate[57]) && ship_dying != 1) { // decrease windows size
         printf("Reageer op smaller screen\n");
         if (factor > 1) {
           window_size_changed = 1;
           factor --;
           }
       }
       if ( (keystate[SDLK_KP_PLUS] || keystate[SDLK_RIGHTBRACKET] || keystate[48]) && ship_dying != 1) { // increase windows size
          printf("Reageer op groter screen\n");
         if (factor < 9) {
           window_size_changed = 1;
           factor ++;
           }
       }
     }
     if ( keystate[56] && ship_dying != 1) { // toggle full_screen: 8 key
        window_size_changed = 1;
        if (full_screen == 1) {
           full_screen = 0;
        } else { 
           full_screen = 1;
        }
        if (d_monitor->current_h >= 1080) factor = 5;   // 1080p and higer
        if (d_monitor->current_h <= 768)  factor = 3;   // 768p and lower
        screen_width  =  factor * VIDEOPAC_RES_W;           
        screen_height =  factor * VIDEOPAC_RES_H;
     }

     if (window_size_changed == 1) {
           handle_screen_resize(2);
     };


  /* Loop through waiting messages and process them */
  
  while (SDL_PollEvent(&event))
  {
    switch (event.type)
    {
      /* Closing the Window will exit the program */
      case SDL_QUIT:   // close window
        exit(0);
      break;

      case SDL_KEYDOWN:

        if (event.key.keysym.sym == SDLK_ESCAPE ) {
            printf("--key escape\n");   // return to instructions
            start_new_game();           // clear all objects
            return(1);
        } else {
          if ( (event.key.keysym.sym >= 97 && event.key.keysym.sym <= 122)
                 || event.key.keysym.sym == 32 || event.key.keysym.sym == 13) {    // spatie, return
            if (high_score_registration == 1) {
              print_high_score_char(event.key.keysym.sym);
            } 
          }
        }
      break;

      case SDL_JOYBUTTONDOWN:
          if (  (event.jbutton.button == 0 || event.jbutton.button == 1)
                && ship_dying != 1
             ) {
             //printf("Fire button presed\n");
             if (frame - bullet_frame >= 5) {
                 add_bullet (ship_x, ship_y);
                 bullet_frame = frame;
             }  
          }
      break;

      case SDL_JOYAXISMOTION:
        //If joystick 0 has moved
        if ( event.jaxis.which == 0 ) {
            //If the X axis changed
            if ( event.jaxis.axis == 0 ) {
                //If the X axis is neutral
                if ( ( event.jaxis.value > -8000 ) && ( event.jaxis.value < 8000 ) ) {
                     joy_left = 0;
                     joy_right = 0;
                } else {
                    //Adjust the velocity
                    if ( event.jaxis.value < 0 ) {
                        joy_left = 1;
                        joy_right = 0;
                    } else {
                        joy_left = 0;
                        joy_right = 1;
                    }
                }
            }
            //If the Y axis changed
            else if ( event.jaxis.axis == 1 ) {
                //If the Y axis is neutral
                if ( ( event.jaxis.value > -8000 ) && ( event.jaxis.value < 8000 ) ) {
                     joy_up = 0;
                     joy_down = 0;
                } else {
                    //Adjust the velocity
                    if ( event.jaxis.value < 0 ) {
                     joy_up = 1;
                     joy_down = 0;
                    } else {
                        joy_up = 0;
                        joy_down = 1;
                    }
                }
            }
        }   // if event.jaxis.which == 0      
      break;      

    }  // end switch
  }    // end while


   /* Check continuous-response keys , works even for diagonals ! */
   if (keystate[SDLK_LEFT] || joy_left == 1) {
       if (ship_dying == 0) ship_x = ship_x - speed; 
       rotate_gun = 1;
   }
   if (keystate[SDLK_RIGHT] || joy_right == 1) { 
       if (ship_dying == 0) ship_x = ship_x + speed; 
       rotate_gun = 1;
   }
   if (keystate[SDLK_UP] || joy_up == 1) { 
       if (ship_dying == 0) ship_y = ship_y - speed; 
       rotate_gun = 1;
   }    
   if (keystate[SDLK_DOWN] || joy_down == 1) { 
       if (ship_dying == 0) ship_y = ship_y + speed; 
       rotate_gun = 1;
   }

   if (rotate_gun == 1 && ship_dying != 1) { rotate_gun_bit();}

   if (ship_x < 0) { ship_x = 0; }
   if (ship_y < 0) { ship_y = 0; }
   if (ship_x + SHIP_W * factor > screen_width)  
      { ship_x = screen_width - SHIP_W * factor; }   // depending on screen size and ship width
   if (ship_y + SHIP_H * factor > screen_height) 
      { ship_y = screen_height - SHIP_H * factor; }  // depending on screen size and ship height

   /* handle fire key */
   if ( (keystate[SDLK_LCTRL] || keystate[SDLK_RCTRL])  && ship_dying != 1 ) {
       // prevent bullets fired to soon after each other
       if (frame - bullet_frame >= 5) {
         add_bullet (ship_x, ship_y);
         bullet_frame = frame;
       }  
    }
 return(0);

}


void cleanup()
{
  /* Shut down SDL */
  printf("Exit game, cleaning up\n");
  Mix_HaltMusic();
  Mix_HaltChannel(-1);
  if (use_joystick == 1) SDL_JoystickClose(js);
  TTF_CloseFont(font_large);
  TTF_CloseFont(font_small);
  SDL_FreeSurface(text);
  SDL_FreeSurface(screen);
  SDL_Quit();
}


void handle_screen_resize(int mode)
{
  // mode == 1: in instructions screen, mode == 2: during game play
  int i;
  char title_string[100];

  screen_width  =  factor * VIDEOPAC_RES_W;           
  screen_height =  factor * VIDEOPAC_RES_H;
  printf("Window resize in handle resize: full_screen=%d, width=%d, height=%d, factor=%d\n",
          full_screen, screen_width, screen_height, factor);
  if (full_screen == 0) 
         screen = SDL_SetVideoMode(screen_width, screen_height, 0, SDL_HWPALETTE | SDL_ANYFORMAT);
  else        
         screen = SDL_SetVideoMode(screen_width, screen_height, 0, SDL_HWPALETTE | SDL_ANYFORMAT | SDL_FULLSCREEN);
  if (screen == NULL) {
      fprintf(stderr, "\nWarning: I could not set up video for "
              "larger/smaller mode.\n"
              "The Simple DirectMedia error that occured was:\n"
              "%s\n\n", SDL_GetError());
  }

  /* adjust ship speed depending on screen size (factor) */
  if (factor <= 2) {
     speed = 2; } 
  else if (factor > 2 && factor <= 5) {
     speed = 7; } 
  else if (factor > 5 && factor <= 7) {
     speed = 10; } 
  else { speed = 14;}   

  /* clear all asteroids, during game mode */
  if (mode == 2) {
    for (i = 0; i < MAX_ASTEROIDS; i++)
       asteroids[i].status = 0;
  } 

  /* clear all ufos during game mode */
  if (mode == 2) {
    for (i = 0; i < MAX_UFOS; i++)
       ufo[i].status = 0;
  }

  /* clear all lasers */
  for (i = 0; i < MAX_LASERS; i++) {
     laser[i].alive = 0;
     laser[i].fired_by_ufo = -1;
  }

  /* init bullets off */
  for (i = 0; i < MAX_BULLETS; i++)
      bullets[i].alive = 0;
  
  /* init mini_explosions off */
  for (i = 0; i < MAX_MINI_EXPLOSIONS; i++)
      mini_explosions[i].alive = 0;


  /* reset ship to center of screen when in game mode*/ 
  if (mode == 2) { 
    ship_x = screen_width  / 2;
    ship_y = screen_height / 2;
  }

  printf("Window factor %d\n", factor);
  load_images();

  font_size = factor * 12;
  TTF_CloseFont(font_large);
  font_large = TTF_OpenFont("O2.ttf", font_size);
  //printf("font size changed\n");
     if (!font_large)
        fprintf(stderr, "Cannot load font name O2.ttf large: %s\n", SDL_GetError());
  TTF_CloseFont(font_small);
  font_small = TTF_OpenFont("O2.ttf", font_size/2);
     if (!font_small)
        fprintf(stderr, "Cannot load font name O2.ttf small: %s\n", SDL_GetError());

  sprintf(title_string, "UFO - factor: %d - difficulty: %d", factor, difficulty);
  SDL_WM_SetCaption(title_string, "UFO");

  SDL_Flip(screen);
  SDL_Delay(500);
}


void draw_stars()
{
  int x,y;
  SDL_Rect rect;         // image desc rectangle (w and h are ignored)

  /* screen top left = (0,0), bottom-right = (screen_width, screen_height))  */

  for (x=0; x < screen_width - SECTOR_SIZE; x = x + SECTOR_SIZE)
  {
    for (y=0; y < screen_height - SECTOR_SIZE; y = y + SECTOR_SIZE)
    {
      //srand( (x+offset_x) * (y+offset_y) );    /* seed randomizer based on x/y position */
      srand( x * y );    /* seed randomizer based on x/y position */
      /* in 1/20 of draw a star */
      if (rand() % 20 == 1)
      {
        rect.x = x;
        rect.y = y;
        rect.w = (rand() % MAX_STAR_SIZE) + 1;
        rect.h = rect.w;

        SDL_FillRect(screen, &rect, getStarColor( rand() % 15) );
      }    
   }
  }
  /* randomize randomizer for subsequent uses */
  srand(SDL_GetTicks() * frame);
}


void draw_ship()
{
  SDL_Rect src_rect;     // image source rectangle
  SDL_Rect rect;         // image desc rectangle (w and h are ignored)

  /* draw ship satellite attack */
  src_rect.x = 0;  // left
  src_rect.y = 0;  // up
  src_rect.w = SHIP_W * factor;   
  src_rect.h = SHIP_H * factor;   

  rect.x = ship_x;   // x position  on screen
  rect.y = ship_y;   // y postition on scherm
  rect.w = SHIP_W * factor;    // ignored!
  rect.h = SHIP_H * factor;    // ignored!

  SDL_BlitSurface(images[5], &src_rect, screen, &rect);


  /* draw "window" on ship : 1 black pixel */

  src_rect.x = 0;  // left
  src_rect.y = 0;  // up
  src_rect.w = factor;   
  src_rect.h = factor;   

  rect.w = SHIP_W * factor;    // ignored!
  rect.h = SHIP_H * factor;    // ignored!
  rect.y = ship_y + (2 * factor);


  switch (ship_window_step) {
  case 1:
     rect.x = ship_x + (3 * factor);
     break;
  case 2:
     rect.x = ship_x + (4 * factor);
     break;
  case 3:
     rect.x = ship_x + (5 * factor);
     break;
  case 4:
     rect.x = ship_x + (6 * factor);
     break;
  case 5:
     rect.x = ship_x + (7 * factor);
     break;
  case 6:
     rect.x = ship_x + (-1 * factor);   // "behind" ship
     break;
  case 7:
     rect.x = ship_x + (-1 * factor);
     break;
  case 8:
     rect.x = ship_x + (-1 * factor);
     break;
  case 9:
     rect.x = ship_x + (0 * factor);
     break;
  case 10:
     rect.x = ship_x + (1 * factor);
     break;
  case 11:
     rect.x = ship_x + (2 * factor);
     break;
  }

  if (frame % 2 == 0) { ship_window_step ++; }
  if (ship_window_step > 11) { ship_window_step = 1;}

  SDL_BlitSurface(images[8], &src_rect, screen, &rect);
}


void setup_shield_bits()
{
  int i;
  
  for (i = 0; i < SHIELD_BITS ; i++)  
  {
      shield_bits[i].status = 1;                     // initial recharching
      shield_bits[i].timer_bit = SHIELD_BIT_TIMER;   // recharching timer
      shield_bits[i].img_white = 10;                 // initial pixel bit color
      shield_bits[i].img_grey = 9;                   // initial pixel bit color
      shield_bits[i].img_blue = 7;                   // initial pixel bit color
      shield_bits[i].gun = 0;                        // gun bit
  }
  shield_bits[gun_bit].gun = 1;   // gun bit
  play_sound(0, 0);
}


void handle_shield_bits()
{
  int i, j, found;
  int active_shield_bits, sound_needed;
  
  // recharge sound if delay is set
  if (recharge_sound_delay == 1) {
     play_sound(0,0);
     recharge_sound_delay = 0;
  }

  if (recharge_sound_delay >= 1) recharge_sound_delay--;

  /* recharching of shield bit only if no bullets alive */
  /* Find a slot: */
  found = -1;
  for (i = 0; i < MAX_BULLETS && found == -1; i++) {
     if (bullets[i].alive == 1)
        found = i;
  }

  if (found != -1) {
    ; //printf("-- bullets alive\n");    // no recharching possible 
  } else {
      // printf("-- no bullets alive, check if recharching needed\n"); 
      sound_needed = 0;
      // loop shield bits
      active_shield_bits = 0;
      for (j = 0; j < SHIELD_BITS; j++)
      {
         if (shield_bits[j].status == 0) {
            sound_needed = 1;
            // change from 0 to 1
            shield_bits[j].status = 1;
            shield_bits[j].timer_bit = SHIELD_BIT_TIMER;
         } else if (shield_bits[j].status == 1) {

            ; // handle recharching timer
            shield_bits[j].timer_bit--;
            if (shield_bits[j].timer_bit == 0) {
              // timer 0, set status to 2
              shield_bits[j].status = 2;
            }
         } else {
            // status == 2, count if all bits are on (active)
            active_shield_bits++;
         }   
      }  // end loop shield bits   
      if (sound_needed == 1 ) {  // activate recharge and start delay time for respawn sound
          recharge_sound_delay = 10; 
          recharge_active = 1;// start time for respawn sound
          //printf("recharge activated\n");
      }
      if (active_shield_bits == 15)   // all bits active, recharge complete
             recharge_active = 0;
  }  // end found != -1

}


 void draw_shield_bits()
 {   
   int i;  
   SDL_Rect src_rect;     // image source rectangle
   SDL_Rect rect;         // image desc rectangle (w and h are ignored)

   for (i = 0; i < SHIELD_BITS; i++)
   {
    if (shield_bits[i].status >= 1) {  // only display for recharching and active bits

      src_rect.x = 0;
      src_rect.y = 0;
      src_rect.w = factor;         // width of bit      
      src_rect.h = factor;         // height of bit     

      /* position shield bits is relative to top/left corner of ship
         no problem if displayed off-screen */

      switch (i) {
        case 0:
          rect.x = ship_x + (3 * factor);
          rect.y = ship_y - (4 * factor);
          break;
        case 1:
          rect.x = ship_x + (5 * factor);
          rect.y = ship_y - (3 * factor);
          break;
        case 2:
          rect.x = ship_x + (7 * factor);
          rect.y = ship_y - (2 * factor);
          break;
        case 3:
          rect.x = ship_x + (9 * factor);
          rect.y = ship_y - (1 * factor);
          break;
        case 4:
          rect.x = ship_x + (10 * factor);
          rect.y = ship_y + (1 * factor);
          break;
        case 5:   
          rect.x = ship_x + (9 * factor);
          rect.y = ship_y + (3 * factor);
          break;
        case 6:
          rect.x = ship_x + (7 * factor);
          rect.y = ship_y + (4 * factor);
          break;
        case 7:
          rect.x = ship_x + (5 * factor);
          rect.y = ship_y + (5 * factor);
          break;
        case 8:
          rect.x = ship_x + (2 * factor);
          rect.y = ship_y + (5 * factor);
          break;
        case 9:
          rect.x = ship_x - (1 * factor);
          rect.y = ship_y + (4 * factor);
          break;
        case 10:
          rect.x = ship_x - (3 * factor);
          rect.y = ship_y + (3 * factor);
          break;
        case 11:
          rect.x = ship_x - (4 * factor);
          rect.y = ship_y + (1 * factor);
          break;
        case 12:
          rect.x = ship_x - (3 * factor);
          rect.y = ship_y - (1 * factor);
          break;
        case 13:
          rect.x = ship_x - (1 * factor);
          rect.y = ship_y - (2 * factor);
          break;
        case 14:
          rect.x = ship_x + (1 * factor);
          rect.y = ship_y - (3 * factor);
          break;
        default:   // wil never happen
          rect.x = ship_x + (10 * factor);
          rect.y = ship_y + (1 * factor);
      }

      rect.w = 4;     // ignored
      rect.h = 4;     // ignored
      
      /* display 3 bits at a time (in 5 sets) 
         global variable 'frame' determines which set is displayed */
      if ( 
           ((frame % 5 == 4) && (i == 2 || i == 9 || i == 13))
           || 
           ((frame % 5 == 3) && (i == 5 || i == 11 || i == 0))
           || 
           ((frame % 5 == 2) && (i == 8 || i == 14 || i == 4))
           || 
           ((frame % 5 == 1) && (i == 10 || i == 1 || i == 7))
           || 
           ((frame % 5 == 0) && (i == 6 || i == 12 || i == 3))
         ) {
           if (shield_bits[i].status == 1) {
             SDL_BlitSurface(images[shield_bits[i].img_grey],
                             &src_rect, screen, &rect);
           } else {  // status == 2
             SDL_BlitSurface(images[shield_bits[i].img_blue],
                             &src_rect, screen, &rect);
           }  

        }
        
        /* always display gun pixel */
        if (shield_bits[i].gun == 1){
           SDL_BlitSurface(images[shield_bits[i].img_white],
                           &src_rect, screen, &rect);
        }
    }
  }  // next i in loop
}


void rotate_gun_bit()
{
  int i, found;
  if (frame % 3 == 0) {
       shield_bits[gun_bit].gun = 0;      // current gun bit off
       gun_bit++;
       
       /* gun bit sound only if no bullets active and no recharching*/
       /* Find a slot: */
       if (recharge_active == 0) {
          found = -1;
          for (i = 0; i < MAX_BULLETS && found == -1; i++) {
             if (bullets[i].alive == 1)
              found = i;
          }
          if (found == -1) play_sound(1, -1);   
       }
       if (gun_bit == 15) {gun_bit = 0;}  // recycle bits
       shield_bits[gun_bit].gun = 1;      // nextgun bit on
  }
}


void add_bullet(int xx, int yy)
{
  int i, j, found;
  
  /* Find a slot: */
  found = -1;
  for (i = 0; i < MAX_BULLETS && found == -1; i++) {
     if (bullets[i].alive == 0)
        found = i;
  }
  
  /* Turn the bullet on: */
  if (found != -1)
    { 
      bullets[found].alive = 1;
      bullets[found].timer = 15;  // 15 frames
                                  // 10 frames = 1/3 seconds = 333 ms
      /* start point and direction of bullet */   
      if (gun_bit + 1 == 1) {
          bullets[found].x = xx + (3 * factor);
          bullets[found].y = yy - (4 * factor);
          bullets[found].xm = 0;
          bullets[found].ym = (-40.74074 / 10) * factor;  
      } else if (gun_bit + 1 == 2) {
          bullets[found].x = xx + (5 * factor);
          bullets[found].y = yy - (3 * factor);
          bullets[found].xm = (20.0 / 10) * factor;
          bullets[found].ym = (-34.81 / 10) * factor;  
      } else if (gun_bit + 1 == 3) {
          bullets[found].x = xx + (7 * factor);
          bullets[found].y = yy - (2 * factor);
          bullets[found].xm = (28.80805 / 10) * factor;
          bullets[found].ym = (-28.80805 / 10) * factor;  
      } else if (gun_bit + 1 == 4) {
          bullets[found].x = xx + (9 * factor);
          bullets[found].y = yy - (1 * factor);
          bullets[found].xm = (34.81805 / 10) * factor;
          bullets[found].ym = (-20.0 / 10) * factor;  
      } else if (gun_bit + 1 == 5) {
          bullets[found].x = xx + (10 * factor);
          bullets[found].y = yy + (1 * factor);
          bullets[found].xm = (40.74074 / 10) * factor;
          bullets[found].ym = 0;  
      } else if (gun_bit + 1 == 6) {
          bullets[found].x = xx + (9 * factor);
          bullets[found].y = yy + (3 * factor);
          bullets[found].xm = (34.81805 / 10) * factor;
          bullets[found].ym = (20.0 / 10) * factor;  
      } else if (gun_bit + 1 == 7) {
          bullets[found].x = xx + (7 * factor);
          bullets[found].y = yy + (4 * factor);
          bullets[found].xm = (28.80805 / 10) * factor;
          bullets[found].ym = (28.80805 / 10) * factor;  
      } else if (gun_bit + 1 == 8) {
          bullets[found].x = xx + (5 * factor);
          bullets[found].y = yy + (5 * factor);
          bullets[found].xm = 0;
          bullets[found].ym = (40.74074 / 10) * factor;  
      } else if (gun_bit + 1 == 9) {
          bullets[found].x = xx + (2 * factor);
          bullets[found].y = yy + (5 * factor);
          bullets[found].xm = 0;
          bullets[found].ym = (40.74074 / 10) * factor;  
      } else if (gun_bit + 1 == 10) {
          bullets[found].x = xx - (1 * factor);
          bullets[found].y = yy + (4 * factor);
          bullets[found].xm = (-28.80805 / 10) * factor;
          bullets[found].ym = (28.80805 / 10) * factor;  
      } else if (gun_bit + 1 == 11) {
          bullets[found].x = xx - (3 * factor);
          bullets[found].y = yy + (3 * factor);
          bullets[found].xm = (-34.81805 / 10) * factor;
          bullets[found].ym = (20.0 / 10) * factor;  
      } else if (gun_bit + 1 == 12) {
          bullets[found].x = xx - (4 * factor);
          bullets[found].y = yy + (1 * factor);
          bullets[found].xm = (-40.74074 / 10) * factor;
          bullets[found].ym = 0;  
      } else if (gun_bit + 1 == 13) {
          bullets[found].x = xx - (3 * factor);
          bullets[found].y = yy - (1 * factor);
          bullets[found].xm = (-40.74074 / 10) * factor;
          bullets[found].ym = (-20.0 / 10) * factor;  
      } else if (gun_bit + 1 == 14) {
          bullets[found].x = xx - (1 * factor);
          bullets[found].y = yy - (2 * factor);
          bullets[found].xm = (-28.80805 / 10) * factor;
          bullets[found].ym = (-28.80805 / 10) * factor;
      } else if (gun_bit + 1 == 15) {
          bullets[found].x = xx + (1 * factor);
          bullets[found].y = yy - (3 * factor);
          bullets[found].xm = (-20.0 / 10) * factor;
          bullets[found].ym = (-34.81805 / 10) * factor;
      }
      //printf("Bullet added from gun_bit %d, met xm,ym: %f, %f\n", gun_bit + 1, bullets[found].xm, bullets[found].ym); 

      /* disable shield */
      for (j = 0; j < SHIELD_BITS; j++) {
        shield_bits[j].status = 0;
      } 
      play_sound(2, -1);   

    }  // if (found != -1)
}


void handle_bullets()
{      
    int i;
    for (i = 0; i < MAX_BULLETS; i++)
    {
      if (bullets[i].alive == 1)
        {
          /* Move: */
          
          bullets[i].x = bullets[i].x + bullets[i].xm;
          bullets[i].y = bullets[i].y + bullets[i].ym;
          
          /* Count down: */
          bullets[i].timer--;

          /* Die? */
          if (bullets[i].y < 0 || bullets[i].y >= screen_height ||
              bullets[i].x < 0 || bullets[i].x >= screen_width  ||
              bullets[i].timer <= 0)
                  bullets[i].alive = 0;
     }
   }
}     


void draw_bullets()
{
  int i;
  SDL_Rect src_rect;     // image source rectangle
  SDL_Rect rect;         // image desc rectangle (w and h are ignored)

  for (i = 0; i < MAX_BULLETS; i++)
  {
    if (bullets[i].alive == 1) {
      src_rect.x = 0;            // left
      src_rect.y = 0;            // up
      src_rect.w = factor;       // 1 factor pixel
      src_rect.h = factor;       // 1 factor pixel

      rect.x = bullets[i].x;
      rect.y = bullets[i].y;
      rect.w = 8;                // ignored!
      rect.h = 8;                // ignored!
      
      if (ship_dying == 0) {   // white bullet/explosion bit
         SDL_BlitSurface(images[10], &src_rect, screen, &rect);
      } else {                 // green, blue, cytan or grey explosion bit
         // pick correct color
         SDL_BlitSurface(images[ship_explosions[ship_explosion_nr].color_nr + 58], &src_rect, screen, &rect);
      }
    }  // if bullet alive
  }    // for loop
}


void add_laser(int xx, int yy, int xxm, int yym, int ufo)
{
  ;
  int i, found;
  
  /* Find a slot: */
  found = -1;
  for (i = 0; i < MAX_LASERS && found == -1; i++) {
     if (laser[i].alive == 0)
        found = i;
  }
  
  /* Turn the laser on: */
  if (found != -1)
    { 
      laser[found].alive = 1;
      laser[found].fired_by_ufo = ufo;
      /* start point and direction of laser */   
      laser[found].x = xx;
      laser[found].y = yy;
      laser[found].xm = xxm * 4 * factor;  
      laser[found].ym = yym * 4 * factor;  

      play_sound(8, -1);   
    }  // if (found != -1)

}


void handle_lasers()
{
   int i;
    for (i = 0; i < MAX_LASERS; i++)
    {
      if (laser[i].alive == 1)
        {
          /* Move: */
          
          laser[i].x = laser[i].x + laser[i].xm;
          laser[i].y = laser[i].y + laser[i].ym;
          
          /* Die? */
          if (laser[i].y < 0 || laser[i].y >= screen_height ||
              laser[i].x < 0 || laser[i].x >= screen_width  ) {
                  laser[i].alive = 0;
                  laser[i].fired_by_ufo = -1;
          }      

     }
   }  
}


void draw_lasers()
{
  int i;
  SDL_Rect src_rect;     // image source rectangle
  SDL_Rect rect;         // image desc rectangle (w and h are ignored)

  for (i = 0; i < MAX_LASERS; i++)
  {
    if (laser[i].alive == 1) {
      src_rect.x = 0;                // left
      src_rect.y = 0;                // up
      src_rect.w = 7 * factor;       // 1 factor pixel
      src_rect.h = 8 * factor;       // 1 factor pixel

      rect.x = laser[i].x;
      rect.y = laser[i].y;
      rect.w = 8;                // ignored!
      rect.h = 8;                // ignored!
      
      // left or right laser
      if ( (laser[i].xm < 0 && laser[i].ym < 0) ||
           (laser[i].xm > 0 && laser[i].ym > 0) ) {
                SDL_BlitSurface(images[70], &src_rect, screen, &rect);  // \ laser  
      }  else { SDL_BlitSurface(images[71], &src_rect, screen, &rect);  // / laser
    }
      
    }  // if laser alive
  }    // for loop
}


void check_laser_hit()
{
  int i, j, k, l, found, found_asteroid;
  int a_x, a_y, a_xr, a_yb;       // top-left and bottom-right coordinates of asteroid
  int b_x, b_y, b_xr, b_yb;       // top-left and bottom-right coordinates of laser
  
  /* check if an active laser hits an active asteroid or ship */

  for (j = 0; j < MAX_LASERS; j++) {
     if (laser[j].alive == 1) {

         b_xr = laser[j].x + 8 * factor;   // width  laser factor pixel
         b_yb = laser[j].y + 7 * factor;   // height laser factor pixel
         b_x  = laser[j].x;
         b_y  = laser[j].y;

         // check if active laser has a hit an asteroid

         /* loop active astroids */
         for (i = 0; i < MAX_ASTEROIDS; i++)
         {
           if (asteroids[i].status == 1 || asteroids[i].status == 2) {
              a_xr = (asteroids[i].x + 6 * factor);   // width  factor pixel
              a_yb = (asteroids[i].y + 5 * factor);   // height factor pixel
              a_x  = asteroids[i].x;
              a_y  = asteroids[i].y;

              /* check overlap of bullet and asteroid */
              if (b_xr  > a_x   &&
                  b_x   < a_xr  &&
                  b_yb  > a_y   &&
                  b_y   < a_yb) {

                    //printf("laser hit on astroid color %d\n", asteroids[i].colour);  
                    laser[j].alive = 0;
                    laser[j].fired_by_ufo = -1;
                    add_mini_explosion(laser[j].x , laser[j].y);     

                    if (asteroids[i].status == 1) {
                      asteroids[i].status = 2;          // make magnetic if asteroid hit by laser
                    }                    
                  
              }

           }  // if status = 1
         }    // end loop active asteroids 


         /* check if laser hits ship */
         /* check overlap of laser and ship */
         if ( (ship_x + SHIP_W * factor)        > b_x   &&
               ship_x                           < b_xr  &&
              (ship_y + SHIP_H * factor)        > b_y   &&
               ship_y                           < b_yb) {

             // is shield down?
             found = -1;
             for (k = 0; k < SHIELD_BITS && found == -1; k++) {
                if (shield_bits[k].status != 2) found = 1;   // shield is down
             }

             if (found == 1) {
                printf("HIT BY LASER, SHIELD WAS DOWN\n");
                ship_dying = 1;
                ship_explosion_nr = 0;
                play_sound(5, 5); 
             } else {
                printf("HIT BY LASER, SHIELD WAS UP\n");

                /* create ship explosion (= asteroid object with status = 3)
                   find a slot: */
                found_asteroid = -1;
                for (l = 0; l < MAX_ASTEROIDS && found_asteroid == -1; l++) {
                   if (asteroids[l].status == 0)
                      found_asteroid = l;
                }

                /* Turn the asteroid/explosion on. */
                /* (there is a small change no slots where free) */
                if (found_asteroid != -1) {
                    asteroids[found_asteroid].status      = 3;    // eploding
                    asteroids[found_asteroid].colour      = 6;    // grey
                    asteroids[found_asteroid].shape_timer = 0;    // for explosion start = 0

                    asteroids[found_asteroid].x  = ship_x + (3 * factor);  // halfway the ship
                    asteroids[found_asteroid].y  = ship_y + (2 * factor);  // halfway the ship
                    asteroids[found_asteroid].xm = 0;
                    asteroids[found_asteroid].ym = 0; 
                    //printf("explosion created for shield ship\n");
                 } else { ; //printf("explosion NOT created for shield ship\n"); 
                 }
                 // end found_asteroid
              
                /* disable all bullets (if any) */
                for (k = 0; k < MAX_BULLETS; k++)
                     bullets[k].alive = 0;

                /* create 3 new bullets (=explosion bits) */
                for (k = 0; k < MAX_BULLETS; k++) {
                  bullets[k].alive = 1;
                  bullets[k].timer = 15;  
                  bullets[k].x = ship_x + (3 * factor);  // halfway the ship 
                  bullets[k].y = ship_y + (2 * factor);  // halfway the ship 
                }      
                bullets[0].xm = 0;
                bullets[0].ym = ( 40.74074 / 15) * factor; // slower and less far than normal bullet
                bullets[1].xm = ( 28.80805 / 15) * factor;
                bullets[1].ym = (-28.80805 / 15) * factor;  
                bullets[2].xm = (-28.80805 / 15) * factor;
                bullets[2].ym = (-28.80805 / 15) * factor;  
          
                // disable shield
                for (k = 0; k < SHIELD_BITS; k++)
                    shield_bits[k].status = 0;
              
             } // end shield is up/down (found ==1)

             // disable laser after hitting ship 
             laser[j].alive = 0;
             laser[j].fired_by_ufo = -1;

         }  // end check overlap laser and ship

      }   // end if laser alive

  } //end active lasers
}


void add_asteroid()
{
 int i, found, direction;
  
  /* Find a slot: */
  found = -1;
  for (i = 0; i < MAX_ASTEROIDS && found == -1; i++) {
     if (asteroids[i].status == 0)
        found = i;
  }
  
  /* Turn the asteroid on: */
  /* (size is 6 pixels wide and 5 pixels tall) */
  if (found != -1) {
      asteroids[found].status      = 1;                      // normal, not magnetic
      asteroids[found].colour      = (rand() % 7) + 1;       // random 1-7
      asteroids[found].shape_timer = ASTEROID_SHAPE_TIMER;   // count_down timer for shape

      /* random 1 of 4 starting positions */
      direction = rand() %4 + 1; 
        switch (direction) {
        case (1):
          /* spawn from top */
          asteroids[found].x  = ( (rand() % (screen_width - 20 * factor)) + 10 * factor );
          asteroids[found].y  = -5 * factor;                   // height of asteroid out of screen
          asteroids[found].xm = ( rand() % 5) - 2;             // values -2, -1, 0, 1 or 2) 
          asteroids[found].ym = ( rand() % 2) + 1;             // values 1 or 2) 
          break;
        case (2):
          /* spawn from bottom */
          asteroids[found].x  = ( (rand() % (screen_width - 20 * factor)) + 10 * factor );
          asteroids[found].y  = screen_height + (factor * 5);  // height of asteroid out of screen
          asteroids[found].xm = ( rand() % 5) - 2;             // values -2, -1, 0, 1 or 2) 
          asteroids[found].ym = ( rand() % 2) - 2;             // values -1 or -2) 
          break;
        case (3):
          /* spawn from left */
          asteroids[found].x  = -6 * factor;                     // width of asteroid out of screen
          asteroids[found].y  = ( (rand() % (screen_height - 20 * factor)) + 10 * factor );
          asteroids[found].xm = ( rand() % 2) + 1;               // values 1 or 2) 
          asteroids[found].ym = ( rand() % 5) - 2;               // values -2, -1, 0, 1 or 2) 
          break;
        case (4):
          /* spawn from right */
          asteroids[found].x  = screen_width + (factor * 6);      // width of asteroid out of screen
          asteroids[found].y  = ( (rand() % (screen_height - 20 * factor)) + 10 * factor );
          asteroids[found].xm = ( rand() % 2) - 2;                // values -1 or -2) 
          asteroids[found].ym = ( rand() % 5) - 2;                // values -2, -1, 0, 1 or 2) 
          break;
        }
      /*  adjust speed for current factor */
      asteroids[found].xm = asteroids[found].xm * (factor / 5.0);
      asteroids[found].ym = asteroids[found].ym * (factor / 5.0);

  }

  /* debug */
  //found = 0;
  //for (i = 0; i < MAX_ASTEROIDS; i++) {
  //   if (asteroids[i].status > 0)
  //      found++;
  //}
  //printf("Active asteroids: %d \n", found);
}


void handle_asteroids()
{
    int i;
    for (i = 0; i < MAX_ASTEROIDS; i++)
    {
      if (asteroids[i].status >= 1 && asteroids[i].status <= 2)
        {
          /* Move: */
          if (asteroids[i].status == 1) {     // normal asteroid
             asteroids[i].x = asteroids[i].x + asteroids[i].xm;
             asteroids[i].y = asteroids[i].y + asteroids[i].ym;

          } else {  // magnetic, move towards ship
            
              if (ship_dying == 0) {   // only when ship is still alive
                 if (asteroids[i].x > ship_x)
                   asteroids[i].x = asteroids[i].x - (factor / 5.0);
                 else asteroids[i].x = asteroids[i].x + (factor / 5.0);
                 if (asteroids[i].y > ship_y)
                   asteroids[i].y = asteroids[i].y - (factor / 5.0) ;
                 else asteroids[i].y = asteroids[i].y + (factor / 5.0);
              }  
          }

          asteroids[i].shape_timer--;;          
          if (asteroids[i].shape_timer == 0)
              asteroids[i].shape_timer = ASTEROID_SHAPE_TIMER ;
          
          /* Off screen? */
          if (asteroids[i].x < -6 * factor || asteroids[i].x >= screen_width + 6 * factor ||
              asteroids[i].y < -5 * factor || asteroids[i].y >= screen_height + 5 * factor) {

              asteroids[i].status = 0;
              //printf("-- asteroid off screen: removed...\n");
          }
     }
   }

}


void draw_asteroids()
{
  int i;
  SDL_Rect src_rect;     // image source rectangle
  SDL_Rect rect;         // image desc rectangle (w and h are ignored)

  for (i = 0; i < MAX_ASTEROIDS; i++)
  {
    if (asteroids[i].status == 1 || asteroids[i].status == 2) {   // normal or magnetic
      src_rect.x = 0;            // left
      src_rect.y = 0;            // up
      src_rect.w = 6 * factor;   // width factor pixel
      src_rect.h = 5 * factor;   // heightfactor pixel

      rect.x = asteroids[i].x;
      rect.y = asteroids[i].y;
      rect.w = 8;                // ignored!
      rect.h = 8;                // ignored!

      // too complex code below for displaying asteroid images per frame

      if (asteroids[i].status == 1) {  // display alternating + en x for normal asteroid, 3 frames per image
                                       // example +++xxx+++xxx+++xxx
         if (asteroids[i].shape_timer >= 1 && asteroids[i].shape_timer <= ASTEROID_SHAPE_TIMER/2) {
              SDL_BlitSurface(images[ 8 + (asteroids[i].colour * 3 ) ], &src_rect, screen, &rect);   // x
         } else {  
              SDL_BlitSurface(images[ 9 + (asteroids[i].colour * 3 ) ], &src_rect, screen, &rect);  // plus
      }

      } else if (asteroids[i].status == 2) {  // display alternating O, + en x for normal asteroid, 3 frames per image
                                       
         if (asteroids[i].shape_timer >= 1 && asteroids[i].shape_timer <= ASTEROID_SHAPE_TIMER/2) {
              SDL_BlitSurface(images[10 + (asteroids[i].colour * 3 ) ], &src_rect, screen, &rect);   // O
              //printf("O\n");
         } else { if (asteroids[i].magnetic_timer%2 == 0) {
                     SDL_BlitSurface(images[9 + (asteroids[i].colour * 3 ) ], &src_rect, screen, &rect);    // plus
                     //printf("+ timer=%d\n", asteroids[i].magnetic_timer); 
                  } else {
                     SDL_BlitSurface(images[8 + (asteroids[i].colour * 3 ) ], &src_rect, screen, &rect);    // x
                     //printf("x timer=%d\n", asteroids[i].magnetic_timer); 
                  }
                  asteroids[i].magnetic_timer++; 
                  if (asteroids[i].magnetic_timer > 5)
                  asteroids[i].magnetic_timer = 0; 
                  
                }
       }  // if asteroids[i].status == 2
      
 
       } else if (asteroids[i].status == 3) {    // exploding 5 images
                //printf("exploding asteroid \n");
                src_rect.x = 0;            // left
                src_rect.y = 0;            // up
                src_rect.w = 16 * factor;   // width factor pixel
                src_rect.h = 13 * factor;   // heightfactor pixel

                rect.x = asteroids[i].x - 5 * factor;
                rect.y = asteroids[i].y - 4 * factor;
                rect.w = 8;                // ignored!
                rect.h = 8;                // ignored!

                SDL_BlitSurface(images[asteroids[i].shape_timer + 31], &src_rect, screen, &rect); 
                asteroids[i].shape_timer++;
                if (asteroids[i].shape_timer == 6) {
                  //  after explosion disable asteroid completely
                  asteroids[i].status = 0;
                  //printf("asteroid completely disabled \n");
                }
      
    }  // if asteroid[i].status 
  }     // end for loop
}


void check_ship_collision()
{
  int i, k, found;
  int a_x, a_y, a_xr, a_yb;       // top-left and bottom-right coordinates of asteroid
  ;
  /* check if ship collides with an asteroid/ufo while shield is down
     (shield is down when at least one shield-bit is not recharched */
  
  found = -1;
  for (i = 0; i < SHIELD_BITS && found == -1; i++ ) {
       if (shield_bits[i].status != 2) 
            found = 1;  // shield is down
  }

  /* check if an asteroid or ufo is colliding with ship */
  if (found != -1) {  // shield is down
    
    /* loop active astroids */
    for (i = 0; i < MAX_ASTEROIDS; i++)
    {
      if (asteroids[i].status == 1 || asteroids[i].status == 2) {
         a_xr = (asteroids[i].x + 6 * factor) ;   // width  factor pixel
         a_yb = (asteroids[i].y + 5 * factor) ;   // height factor pixel
         a_x  = asteroids[i].x;
         a_y  = asteroids[i].y;

         /* check overlap of astroid and ship */
         if ( (ship_x + SHIP_W * factor)        > a_x   &&
               ship_x                           < a_xr  &&
              (ship_y + SHIP_H * factor)        > a_y   &&
               ship_y                           < a_yb) {
         printf("DEADLY COLLISION WITH ASTEROID!\n");
         ship_dying = 1;
         ship_explosion_nr = 0;
         play_sound(5, 5); 
         }
      }
    }  // end loop active asteroids

    /* loop active ufos */
    for (i = 0; i < MAX_UFOS; i++)
    {
      if (ufo[i].status == 1) {
         a_xr = (ufo[i].x + 8 * factor) ;   // width  factor pixel
         a_yb = (ufo[i].y + 2 * factor) ;   // height factor pixel
         a_x  = ufo[i].x;
         a_y  = ufo[i].y;

         /* check overlap of astroid and ship */
         if ( (ship_x + SHIP_W * factor)        > a_x   &&
               ship_x                           < a_xr  &&
              (ship_y + SHIP_H * factor)        > a_y   &&
               ship_y                           < a_yb) {
         printf("DEADLY COLLISION WITH UFO!\n");
         ship_dying = 1;
         ship_explosion_nr = 0;
         play_sound(5, 5); 
         }
      }
    }    // end loop active ufos



  } else {   // shield is active, check on hitting an asteroid or ufo

      /* loop active astroids */
      for (i = 0; i < MAX_ASTEROIDS; i++)
      {
        if (asteroids[i].status == 1 || asteroids[i].status == 2) {
           a_xr = (asteroids[i].x + 6 * factor) ;   // width  factor pixel
           a_yb = (asteroids[i].y + 5 * factor) ;   // height factor pixel
           a_x  = asteroids[i].x;
           a_y  = asteroids[i].y;
           
           /* check overlap of astroid and ship */
           /* (ship size is larger when shield is active) */
           if ( ((ship_x - 3) + (SHIP_W + 5) * factor)        > a_x   &&
                 (ship_x - 3)                                 < a_xr  &&
                ((ship_y - 3) + (SHIP_H + 3) * factor)        > a_y   &&
                 (ship_y - 3)                                 < a_yb) {
              //printf("hit asteroid with ship\n");

              // disable asteroid: set status = 3 exploding
              play_sound(3, 3);

              /* increase score */
              if (asteroids[i].status == 1) score++;
              if (asteroids[i].status == 2) score = score + 3;
              if (score > high_score) {
                    high_score = score;
                    high_score_broken = 1;
              }      

              asteroids[i].status = 3;
              asteroids[i].shape_timer = 1;   // explosion takes 5 images

              /* disable all bullets */
              for (k = 0; k < MAX_BULLETS; k++)
                   bullets[k].alive = 0;

              /* create 3 new bullets (=explosion bits) */
              for (k = 0; k < MAX_BULLETS; k++) {
                     bullets[k].alive = 1;
                     bullets[k].timer = 15; 
                     bullets[k].x = asteroids[i].x + (3 * factor);  // halfway asteroid
                     bullets[k].y = asteroids[i].y + (2 * factor);  // halfway asteroid
              }      
              bullets[0].xm = 0;
              bullets[0].ym = ( 40.74074 / 15) * factor; // slower and less far than normal bullet
              bullets[1].xm = ( 28.80805 / 15) * factor;
              bullets[1].ym = (-28.80805 / 15) * factor;  
              bullets[2].xm = (-28.80805 / 15) * factor;
              bullets[2].ym = (-28.80805 / 15) * factor;  

              // disable shield
              for (k = 0; k < SHIELD_BITS; k++)
                  shield_bits[k].status = 0;

           }  // end if bump asteroid with ship
        }  
      } // end loop active asteroids

      /* loop active ufo */
      for (i = 0; i < MAX_UFOS; i++)
      {
        if (ufo[i].status == 1) {
           a_xr = (ufo[i].x + 8 * factor) ;   // width  factor pixel
           a_yb = (ufo[i].y + 2 * factor) ;   // height factor pixel
           a_x  = ufo[i].x;
           a_y  = ufo[i].y;
           
           /* check overlap of ufo and ship */
           /* (ship size is larger when shield is active) */
           if ( ((ship_x - 3) + (SHIP_W + 5) * factor)        > a_x   &&
                 (ship_x - 3)                                 < a_xr  &&
                ((ship_y - 3) + (SHIP_H + 3) * factor)        > a_y   &&
                 (ship_y - 3)                                 < a_yb) {
              printf("hit ufo with ship\n");
            
              /* increase score */
              score = score + 10;
              if (score > high_score) {
                    high_score = score;
                    high_score_broken = 1;
              }      

              // disable ufo: set status = 3 exploding
              play_sound(3, 3);

              ufo[i].status = 3;
              ufo[i].shape_timer = 1;   // explosion takes 5 images

              /* disable all bullets */
              for (k = 0; k < MAX_BULLETS; k++)
                   bullets[k].alive = 0;

              /* create 3 new bullets (=explosion bits) */
              for (k = 0; k < MAX_BULLETS; k++) {
                     bullets[k].alive = 1;
                     bullets[k].timer = 15; 
                     bullets[k].x = ufo[i].x + (4 * factor);  // halfway ufo
                     bullets[k].y = ufo[i].y + (1 * factor);  // halfway ufo
              }      
              bullets[0].xm = 0;
              bullets[0].ym = ( 40.74074 / 15) * factor; // slower and less far than normal bullet
              bullets[1].xm = ( 28.80805 / 15) * factor;
              bullets[1].ym = (-28.80805 / 15) * factor;  
              bullets[2].xm = (-28.80805 / 15) * factor;
              bullets[2].ym = (-28.80805 / 15) * factor;  

              // disable shield
              for (k = 0; k < SHIELD_BITS; k++)
                  shield_bits[k].status = 0;

              // Disable laser if fired from ufo (to prevent ship hit again)
              for (k = 0; k < MAX_LASERS; k++) {
                 if (laser[k].alive == 1 && laser[k].fired_by_ufo == i) {  
                   printf("-- disable laser from ufo which is hit by ship \n");
                   laser[k].alive = 0;
                 }
              }    

           }  // end if bump ufo with ship and active shield
        }  
      } // end loop active ufos

  }  // end found != -1 (shield is down)
}


void check_bullet_hit()
{
  int i, j, k;
  int a_x, a_y, a_xr, a_yb;       // top-left and bottom-right coordinates of asteroid
  int b_x, b_y, b_xr, b_yb;       // top-left and bottom-right coordinates of bullet
  
  /* check if an active bullet hits an active asteroid or ufo*/

  for (j = 0; j < MAX_BULLETS; j++) {
     if (bullets[j].alive == 1) {

         b_xr = bullets[j].x + factor;   // width  factor pixel
         b_yb = bullets[j].y + factor;   // height factor pixel
         b_x  = bullets[j].x;
         b_y  = bullets[j].y;

         // check if active bullet has a hit 

         /* loop active astroids */
         for (i = 0; i < MAX_ASTEROIDS; i++)
         {
           if (asteroids[i].status == 1 || asteroids[i].status == 2) {
              a_xr = (asteroids[i].x + 6 * factor);   // width  factor pixel
              a_yb = (asteroids[i].y + 5 * factor);   // height factor pixel
              a_x  = asteroids[i].x;
              a_y  = asteroids[i].y;

              /* check overlap of bullet and asteroid */
              if (b_xr  > a_x   &&
                  b_x   < a_xr  &&
                  b_yb  > a_y   &&
                  b_y   < a_yb) {

                    // disable asteroid: set status = 3 exploding
                    play_sound(3, 3);

                    /* increase score */
                    if (asteroids[i].status == 1) score++;
                    if (asteroids[i].status == 2) score = score + 3;
                    if (score > high_score) {
                      high_score = score;
                      high_score_broken = 1;
                    }  

                    asteroids[i].status = 3;
                    asteroids[i].shape_timer = 1;   // explosion takes 5 images

                    /* disable all bullets */
                    for (k = 0; k < MAX_BULLETS; k++)
                         bullets[k].alive = 0;

                    /* create 3 new bullets (=explosion bits) */
                    for (k = 0; k < MAX_BULLETS; k++) {
                           bullets[k].alive = 1;
                           bullets[k].timer = 15; 
                           bullets[k].x = asteroids[i].x + (3 * factor);  // halfway asteroid
                           bullets[k].y = asteroids[i].y + (2 * factor);  // halfway asteroid
                     }      
                     bullets[0].xm = 0;
                     bullets[0].ym = ( 40.74074 / 15) * factor; // slower and less far than normal bullet
                     bullets[1].xm = ( 28.80805 / 15) * factor;
                     bullets[1].ym = (-28.80805 / 15) * factor;  
                     bullets[2].xm = (-28.80805 / 15) * factor;
                     bullets[2].ym = (-28.80805 / 15) * factor;  

              }
              // disable shield
              for (k = 0; k < SHIELD_BITS; k++)
                  shield_bits[k].status = 0;

           }  // if status = 1 or 2
         }    // end loop active asteroids 



         /* loop active ufos */
         for (i = 0; i < MAX_UFOS; i++)
         {
           if (ufo[i].status == 1) {
              a_xr = (ufo[i].x + 8 * factor);   // width  factor pixel
              a_yb = (ufo[i].y + 2 * factor);   // height factor pixel
              a_x  = ufo[i].x;
              a_y  = ufo[i].y;

              /* check overlap of bullet and ufo */
              if (b_xr  > a_x   &&
                  b_x   < a_xr  &&
                  b_yb  > a_y   &&
                  b_y   < a_yb) {

                    // disable ufo: set status = 3 exploding
                    play_sound(3, 3);

                    /* increase score */
                    score = score + 10;
                    if (score > high_score) {
                      high_score = score;
                      high_score_broken = 1;
                    }  

                    ufo[i].status = 3;
                    ufo[i].shape_timer = 1;   // explosion takes 5 images

                    /* disable all bullets */
                    for (k = 0; k < MAX_BULLETS; k++)
                         bullets[k].alive = 0;

                    /* create 3 new bullets (=explosion bits) */
                    for (k = 0; k < MAX_BULLETS; k++) {
                           bullets[k].alive = 1;
                           bullets[k].timer = 15;  
                           bullets[k].x = ufo[i].x + (4 * factor);  // halfway ufo
                           bullets[k].y = ufo[i].y + (1 * factor);  // halfway ufo
                     }      
                     bullets[0].xm = 0;
                     bullets[0].ym = ( 40.74074 / 15) * factor; // slower and less far than normal bullet
                     bullets[1].xm = ( 28.80805 / 15) * factor;
                     bullets[1].ym = (-28.80805 / 15) * factor;  
                     bullets[2].xm = (-28.80805 / 15) * factor;
                     bullets[2].ym = (-28.80805 / 15) * factor;  

              }
              // disable shield
              for (k = 0; k < SHIELD_BITS; k++)
                  shield_bits[k].status = 0;

           }  // if status = 1 
         }    // end loop active ufos

       }      // end if bullet alive

  } // end loop active bullets
}


void check_colliding_asteroids()
{
  int i, j;
  int a_x, a_y, a_xr, a_yb;       // top-left and bottom-right coordinates of asteroid
  int b_x, b_y, b_xr, b_yb;       // top-left and bottom-right coordinates of asteroid

  /* handle colliding asteroid with other asteroid and
     colliding asteroid with ufos (ufo alway loses) */

  /* loop active astroids (magnetic and non-magnetic) */
  for (i = 0; i < MAX_ASTEROIDS; i++)
  {
    if (asteroids[i].status == 1 || asteroids[i].status == 2) {
       a_xr = (asteroids[i].x + 6 * factor);   // width  factor pixel
       a_yb = (asteroids[i].y + 5 * factor);   // height factor pixel
       a_x  = asteroids[i].x;
       a_y  = asteroids[i].y;

       /* loop active astroids (magnetic and non-magnetic) */
       for (j = 0; j < MAX_ASTEROIDS; j++)  
       {
          if ((asteroids[j].status == 1 || asteroids[i].status == 2) && i != j) {   // not itself
             b_xr = (asteroids[j].x + 6 * factor);    // width  factor pixel
             b_yb = (asteroids[j].y + 5 * factor);    // height factor pixel
             b_x  = asteroids[j].x;
             b_y  = asteroids[j].y;

           /* check overlap of asteroids */
           if (b_xr  > a_x   &&
               b_x   < a_xr  &&
               b_yb  > a_y   &&
               b_y   < a_yb) {
                  //printf("-- asteroids overlap: %d and %d \n", asteroids[i].colour, asteroids[j].colour );
                  /* create magnetic 1 out of 10 */
                  if (rand() % 10 == 0) {

                       /* determine which asteroid is non-magnetic */
                       if (asteroids[i].status == 1) {
                          asteroids[i].status = 2;   // make magnetic, keep colour //
                          asteroids[i].magnetic_timer = 0;

                          // kill the other one (only if this one is non-magnetic)
                          if (asteroids[j].status == 1) {
                            asteroids[j].status = 0;
                            add_mini_explosion(asteroids[j].x , asteroids[j].y);                          
                          }                            
                       } else {
                          // asteroids[i] is magnetic, check if asteroids[j] is normal 
                          // if so, make kill j
                          if (asteroids[j].status == 1) {
                             //printf("Magnetic (j) created!\n");
                             // kill the other one
                             asteroids[j].status = 0;
                             add_mini_explosion(asteroids[j].x , asteroids[j].y);                          
                          } else {
                               // both asteroids are magnetic
                               //printf("collision of 2 magnetic asteroid!\n");
                               // kill the second one
                               asteroids[j].status = 0;
                               add_mini_explosion(asteroids[j].x , asteroids[j].y);                          
                          }
                       }

                  }
            }
           } // end if asteroids[j].status == 1

        }  // end j-loop asteroids


       /* loop active ufos */
       for (j = 0; j < MAX_UFOS; j++) { 
       if (ufo[j].status == 1) {  
             b_xr = (ufo[j].x + 8 * factor);    // width  factor pixel
             b_yb = (ufo[j].y + 2 * factor);    // height factor pixel
             b_x  = ufo[j].x;
             b_y  = ufo[j].y;

           /* check overlap of asteroid[i] with ufo */
           if (b_xr  > a_x   &&
               b_x   < a_xr  &&
               b_yb  > a_y   &&
               b_y   < a_yb) {
                  // kill ufo, keep asteroid (do not make magnetic, there will be too many)
                  ufo[j].status = 0;
                  if (Mix_Playing(6)) Mix_HaltChannel(6);    // stop audio for ufo               
                  add_mini_explosion(ufo[j].x , ufo[j].y);                          
            } // check overlap

           } // end if ufo[j].status == 1

        }  // end j-loop ufos


      }    // if asteroids[i].status == 1
   }       // end i-loop asteroids
}   


void check_asteroid_positions()
{
  /* check of asteroids are about to collide
     if so, try to avoid that (but collisions may still happen) */
  int i, j;
  float a_mx, a_my, b_mx, b_my, distance; 

  /* loop alle active astroids */
  for (i = 0; i < MAX_ASTEROIDS; i++)
  {
    if (asteroids[i].status == 1 || asteroids[i].status == 2) {
       a_mx = (asteroids[i].x + 6/2 * factor);   // center x coordinate
       a_my = (asteroids[i].y + 5/2 * factor);   // center y coordinate

       /* loop active astroids */
       for (j = 0; j < MAX_ASTEROIDS; j++)  
       {
          if ((asteroids[j].status == 1 || asteroids[j].status == 2) && i != j) {   // not itself
             b_mx = (asteroids[j].x + 6/2 * factor);  // center x coordinate
             b_my = (asteroids[j].y + 5/2 * factor);  // center y coordinate

           /* check distance of asteroids: Pythagoras ! */
           distance = (fabs(b_mx - a_mx)) * (fabs(b_mx - a_mx)); // x^2
           distance = distance + ( (fabs(b_my - a_my)) * (fabs(b_my - a_my)) ); // + y^2
           distance = sqrtf(distance);  // sqrt

           if ( distance < factor * 10 ) {
                  //printf("-- asteroids too near %d, %d \n", asteroids[i].colour, asteroids[j].colour);
                  if (rand() % 10 == 1) {    // 1 out of 10, when larger increases chance
                                             // of collision (= magnetic asteroid)
                       //printf("avoiding started!\n");
                       // change x-direction
                       if ( fabs(b_mx - a_mx) < factor * 10) {
                           asteroids[i].xm = -1 * asteroids[i].xm;
                           asteroids[j].xm = -1 * asteroids[i].xm;
                       }
                       // change y-direction
                       if ( fabs(b_my - a_my) < factor * 10) {
                           asteroids[i].ym = -1 * asteroids[i].ym;
                           asteroids[j].ym = -1 * asteroids[i].ym;
                       }

                  }
            }

           } // end if asteroids[j].status == 1 || 2

        }  // end j-loop asteroids

      }    // if asteroids[i].status == 1 || 2
   }       // end i-loop asteroids
}


void draw_mini_explosions()
{
  int i;
  SDL_Rect src_rect;     // image source rectangle
  SDL_Rect rect;         // image desc rectangle (w and h are ignored)

  for (i = 0; i < MAX_MINI_EXPLOSIONS; i++)
  {
    if (mini_explosions[i].alive == 1) {
      src_rect.x = 0;            // left
      src_rect.y = 0;            // up
      src_rect.w = factor * 8 ;  // 1 factor pixel x w
      src_rect.h = factor * 8 ;  // 1 factor pixel x h

      rect.x = mini_explosions[i].x;
      rect.y = mini_explosions[i].y;
      rect.w = 8;                // ignored!
      rect.h = 8;                // ignored!
      
      mini_explosions[i].timer--;
      if (mini_explosions[i].timer == 0) {
          //printf("Mini explosion timed oud\n");
          mini_explosions[i].alive = 0;
      } else {
          if (mini_explosions[i].timer > 5) {   
            SDL_BlitSurface(images[37], &src_rect, screen, &rect);   // yello
          }
          else {
            SDL_BlitSurface(images[38], &src_rect, screen, &rect);   // grey
          } 
      }
    }   // alive == 1
  }     // end for loop 
}


void add_mini_explosion(int x, int y)
{
  int i, found;
  
  /* Find a slot: */
  found = -1;
  for (i = 0; i < MAX_MINI_EXPLOSIONS && found == -1; i++) {
     if (mini_explosions[i].alive == 0)
        found = i;
  }
  
  /* Turn the explosion on: */
  if (found != -1)
    { 
      mini_explosions[found].alive = 1;
      mini_explosions[found].timer = 15;  // 15 frames; 
                                          // 10 frames = 1/3 seconds = 333 ms
      mini_explosions[found].x = x;
      mini_explosions[found].y = y;
      
      //printf("Mini explosion added x,y: %d, %d \n", x, y); 
      play_sound(4, 4);   

    }  // if (found != -1)

}


void draw_ufo()
{
  int i;
  SDL_Rect src_rect;     // image source rectangle
  SDL_Rect rect;         // image desc rectangle (w and h are ignored)

  for (i = 0; i < MAX_UFOS; i++)
  {
    if (ufo[i].status == 1 || ufo[i].status == 3) {   // active or exploding
      src_rect.x = 0;            // left
      src_rect.y = 0;            // up
      src_rect.w = 8 * factor;   // width  factor pixel
      src_rect.h = 2 * factor;   // height factor pixel

      rect.x = ufo[i].x;
      rect.y = ufo[i].y;
      rect.w = 8;                // ignored!
      rect.h = 8;                // ignored!

      if (ufo[i].status == 1) {  // 
              SDL_BlitSurface(images[62 + (ufo[i].colour) ], &src_rect, screen, &rect);  // colour 1-7
      } else if (ufo[i].status == 3) {    // exploding 5 images
                //printf("exploding ufo \n");
                src_rect.x = 0;            // left
                src_rect.y = 0;            // up
                src_rect.w = 16 * factor;   // width factor pixel of explosion
                src_rect.h = 13 * factor;   // heightfactor pixel of explosion

                rect.x = ufo[i].x - 5 * factor;
                rect.y = ufo[i].y - 4 * factor;
                rect.w = 8;                // ignored!
                rect.h = 8;                // ignored!

                SDL_BlitSurface(images[ufo[i].shape_timer + 31], &src_rect, screen, &rect); 
                
                ufo[i].shape_timer++;
                if (ufo[i].shape_timer == 6) {
                  //  after explosion disable ufo completely
                  ufo[i].status = 0;
                  //printf("ufo completely disabled \n");
                }
           }  
    }   // if ufo[i].status 
  }     // end for loop
}


void add_ufo()
{
 int i, found, direction;
  
  /* Find a slot: */
  found = -1;
  for (i = 0; i < MAX_UFOS && found == -1; i++) {
     if (ufo[i].status == 0)
        found = i;
  }
  
  /* Turn the ufo on: */
  /* (size is 8 pixels wide and 2 pixels tall) */
  if (found != -1) {
      //printf("UFO created\n");  
      ufo[found].status      = 1;                      // active
      ufo[found].colour      = (rand() % 7) + 1;       // random 1-7
      ufo[found].shape_timer = ASTEROID_SHAPE_TIMER;   // count_down timer for explosion

      /* random 1 of 4 starting positions 
         but always moving diagonal and speed 3 */
      direction = rand() %4 + 1; 
        switch (direction) {
        case (1):
          /* spawn from top */
          //ufo[found].x  = ( (rand() % (screen_width - 20 * factor)) + 10 * factor );
          ufo[found].x  = ( (rand() % (screen_width - 80 * factor)) + 40 * factor );
          ufo[found].y  = -2 * factor;                    // height of ufo out of screen
          ufo[found].xm = (-6 * (rand()%2))  + 3;         // values -3 or 3) 
          ufo[found].ym = 2;    
          break;
        case (2):
          /* spawn from bottom */
          //ufo[found].x  = ( (rand() % (screen_width - 20 * factor)) + 10 * factor );
          ufo[found].x  = ( (rand() % (screen_width - 80 * factor)) + 40 * factor );
          ufo[found].y  = screen_height + (factor * 2);   // height of ufo out of screen
          ufo[found].xm = (-6 * (rand()%2))  + 3;         // values -3 or 3) 
          ufo[found].ym = -2;
          break;
        case (3):
          /* spawn from left */
          ufo[found].x  = -8 * factor;                     // width of ufo out of screen
          //ufo[found].y  = ( (rand() % (screen_height - 20 * factor)) + 10 * factor );
          ufo[found].y  = ( (rand() % (screen_height - 80 * factor)) + 20 * factor );
          ufo[found].xm = 2;
          ufo[found].ym = ( rand() % 5) - 2;               // values -2, -1, 0, 1 or 2) 
          ufo[found].ym = (-6 * (rand()%2))  + 3;          // values -3 or 3) 
          break;
        case (4):
          /* spawn from right */
          ufo[found].x  = screen_width + (factor * 8);      // width of asteroid out of screen
          //ufo[found].y  = ( (rand() % (screen_height - 20 * factor)) + 10 * factor );
          ufo[found].y  = ( (rand() % (screen_height - 80 * factor)) + 20 * factor );
          ufo[found].xm = -2;
          ufo[found].ym = (-6 * (rand()%2))  + 3;          // values -3 or 3) 
          break;
        }
      /*  adjust speed for current factor */
      ufo[found].xm = ufo[found].xm * (factor / 5.0);
      ufo[found].ym = ufo[found].ym * (factor / 5.0);

      //printf("ufo i=%d added\n", found);
      play_sound(9,6);

  }  // if not found 
}


void handle_ufo()
{
   int i, j, k, lx, ly, xm, ym;
   int laser_for_ufo_added;
   int laser_type;    // 1 = backward down     3 = forward down
                      // 2 = backward up       4 = forward up

   for (i = 0; i < MAX_UFOS; i++)
   {

     if (ufo[i].status == 1)
     {
       /* Move: */
        ufo[i].x = ufo[i].x + ufo[i].xm;
        ufo[i].y = ufo[i].y + ufo[i].ym;
        ufo[i].shape_timer--;;          
        
        // timer for ufo explosion
        if (ufo[i].shape_timer == 0) {
            ufo[i].shape_timer = ASTEROID_SHAPE_TIMER ;
        }

        /* Is ufo off-screen? */
        if (ufo[i].x < -8 * factor || ufo[i].x >= screen_width  + (8 * factor) ||
            ufo[i].y < -2 * factor || ufo[i].y >= screen_height + (2 * factor)) {
            ufo[i].status = 0;
            if (Mix_Playing(6)) Mix_HaltChannel(6);
            //printf("-- ufo off screen: removed...\n");
        }

        /* If possible fire a laser
           First check if UFO already has an active laser, if so, don't fire 
           a possible second laser for this ufo */
        laser_for_ufo_added = -1;
        for (k = 0; k < MAX_LASERS && laser_for_ufo_added == -1; k++) {
          if (laser[k].fired_by_ufo == i) {
            laser_for_ufo_added = j;
          }
        } // end loop j


        for (j = 0; j < MAX_LASERS && laser_for_ufo_added == -1; j++)  
        {
          if ( laser[j].alive == 1) {   // skip active lasers
              ; 
          } else {
              /* fire laser if ship is in range */
              lx = ufo[i].x + (2 * factor);    // possible laser x starting point
              ly = ufo[i].y;                   // possible laser x starting point

              // check if backward or forward laser can hit ship 
              laser_type = 0;
                if ( abs( ((ship_x + 2*factor) - (lx-ly))  -  ((ship_y + 2*factor)) ) <= 2*factor ) {   // middle of ship +- 2        
                 if (ship_y > ufo[i].y + 4 * factor) {
                   laser_type = 1;
                 } else {   
                   laser_type = 2;
                 }
              }
                if ( abs( (lx - (ship_x - 2*factor)) - ((ship_y + 2*factor) - ly) ) <= 2*factor ) {   // middle of ship +- 2
                 if (ship_y > ufo[i].y + 4 * factor) {
                   laser_type = 3;
                 } else {   
                   laser_type = 4;
                 }
              }

              if (laser_type != 0) {
                  switch (laser_type) {
                    case 1:
                      xm = 1;
                      ym = 1;
                      break;
                    case 2:
                      xm = -1;
                      ym = -1;
                      break;
                    case 3:
                      xm = -1;
                      ym = 1;
                      break;
                    case 4:
                      xm = 1;
                      ym = -1;
                      break;
                  }    
               add_laser(ufo[i].x + (2 * factor), ufo[i].y, xm, ym, i );
               laser_for_ufo_added = i;
              }    
           }  // end laser[j].alive == 1 && laser[j].fired_by_ufo == i   
        }     // end j-loop

     }
   }  // end for i-loop
  
   /* create new ufo if possible (and randomnes and new game at least 15 frame/30  sec active)  */
   if (rand() % UFO_RANDOMNESS == 1 && ship_dying == 0 && (frame - ufo_start_delay > 10*30)) {
      add_ufo(); 
   }
}


void draw_score_line() 
{
  SDL_Color fgColor_green  = {0,182,0};   
  SDL_Color fgColor_red    = {182,0,0};   
  SDL_Color fgColor_grey   = {182,182,182};   // grey/white
  SDL_Rect text_position;  
  char text_line[20]; 

  // highscore in green
  sprintf(text_line, "%04d", high_score);
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_green);
  text_position.x = 24 * factor;
  text_position.y = 145 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  // arrow sign in grey/white
  sprintf(text_line, "%c", 124); // arrow-char in modified o2 font
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_grey);
  text_position.x = (24 * factor) + (3 * 12 * factor); // skip 3 chars
  text_position.y = 145 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  // highscore name in green
  if (ship_destroyed == 1) {
    flash_high_score_name();
  } else {  
    sprintf(text_line, "%s ", high_score_name);
    text = TTF_RenderText_Solid(font_large, text_line, fgColor_green);
    text_position.x = (24 * factor) + (4 * 12 * factor); // skip 4 chars
    text_position.y = 145 * factor;
    SDL_BlitSurface(text, NULL , screen, &text_position);
  }

  // current score in red
  sprintf(text_line, " %04d", score);
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_red);
  text_position.x = (24 * factor) + (9 * 12 * factor); // skip 9 chars
  text_position.y = 145 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

}


void flash_high_score_name()
{
  SDL_Color fgColor_green  = {0,182,0};   
  SDL_Rect text_position;  
  char text_line[6];

  if (high_score_broken == 1) strcpy(high_score_name, "??????");

  // highscore name in green
  sprintf(text_line, "%s ", high_score_name);
  
  text_line[flash_high_score_timer%6] = ' ';
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_green);
  text_position.x = (24 * factor) + (4 * 12 * factor); // skip 4 chars
  text_position.y = 145 * factor;
  
  SDL_BlitSurface(text, NULL , screen, &text_position);
  
  if (frame%3 == 0) {
    flash_high_score_timer--;
  }  
  if (flash_high_score_timer < 0) flash_high_score_timer = 150;
}


void print_high_score_char(int character)
{
  SDL_Color fgColor_green = {0,182,0};   
  SDL_Color fgColor_red    = {182,0,0};   
  SDL_Rect text_position;  
  char text_line[6];
  
  // highscore name in green
  strcpy(text_line, high_score_name);
  
  if (character != 13) text_line[high_score_character_pos] = character;
  strcpy(high_score_name, text_line);

  high_score_character_pos++;
  if (high_score_character_pos > 5 || character == 13) {  // max length or return
    high_score_registration = 0;
    printf("stop registration\n");
  }
    
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_green);
  text_position.x = (24 * factor) + (4 * 12 * factor); // skip 4 chars
  text_position.y = 145 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  // current score in red
  sprintf(text_line, " %04d", score);
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_red);
  text_position.x = (24 * factor) + (9 * 12 * factor); // skip 9 chars
  text_position.y = 145 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  play_sound(7, -1);
}


void draw_ship_explosions()
{
  SDL_Rect src_rect;     // image source rectangle
  SDL_Rect rect;         // image desc rectangle (w and h are ignored)
  int k;

  /* draw ship explosion */
  src_rect.x = 0;  // left
  src_rect.y = 0;  // up
  src_rect.w = 16 * factor;   
  src_rect.h = 13 * factor;   

  rect.x = ship_x - 3*factor;    // x  
  rect.y = ship_y - 4*factor;    // y 
  rect.w = SHIP_W * factor;      // ignored!
  rect.h = SHIP_H * factor;      // ignored!

  if (ship_explosions[ship_explosion_nr].img_nr != 0) {     // draw explosion
     SDL_BlitSurface(images[ship_explosions[ship_explosion_nr].img_nr], &src_rect, screen, &rect);
  }  

  if (ship_explosions[ship_explosion_nr].ship_nr != 0) {     // draw ship */
    src_rect.w = SHIP_W * factor;   
    src_rect.h = SHIP_H * factor;   
    rect.x = ship_x;   // x 
    rect.y = ship_y;   // y 
    SDL_BlitSurface(images[ship_explosions[ship_explosion_nr].ship_nr], &src_rect, screen, &rect);  // ship green
  }

  if (ship_explosions[ship_explosion_nr].img_nr != 0) {     // draw explosion
     SDL_BlitSurface(images[ship_explosions[ship_explosion_nr].img_nr], &src_rect, screen, &rect);
  }  

  if (ship_explosion_nr%8 == 0 ) {     // add 3 bullets (=explosion bits) every 8 images*/
                                       // these are displayed on screen in function
                                       // draw_bullets()
       for (k = 0; k < MAX_BULLETS; k++) {
              bullets[k].alive = 1;
              bullets[k].timer = 15;  
              bullets[k].x = ship_x + (SHIP_W/2 * factor);  // halfway ship
              bullets[k].y = ship_y + (SHIP_H/2 * factor);  // halfway ship
        }      
        bullets[0].xm = 0;
        bullets[0].ym = ( 40.74074 / 15) * factor; // slower and less far than normal bullet
        bullets[1].xm = ( 28.80805 / 15) * factor;
        bullets[1].ym = (-28.80805 / 15) * factor;  
        bullets[2].xm = (-28.80805 / 15) * factor;
        bullets[2].ym = (-28.80805 / 15) * factor;  
   }    

  ship_explosion_nr++;    // every frame 
  if (ship_explosion_nr + 1 == SHIP_EXPLOSIONS) { // end of explosion sequences, ship gone
     printf("GAME OVER\n");
     ship_destroyed = 1;
     flash_high_score_timer = 55;  // +/- 5 seconds : 
     play_sound(6, -1); 
  } 
}


int getStarColor(int speed)
{
  SDL_Color color;
  
  switch (speed)
  {
    case 1:
    case 2:
    case 3:
      color.r = 221;  /* red */
      color.g =  44;
      color.b =   0;
      break;
        
    case 4:
    case 5:
    case 6:
      color.r =  76; /* green */
      color.g = 175;
      color.b =  80;
      break;
        
    case 7:
    case 8:
    case 9:
      color.r = 255; /* yellow */
      color.g = 234;
      color.b =   0;
      break;
      
    default:
      color.r = 255;  /* default white */
      color.g = 255;
      color.b = 255;
      break;
  }
  
  return SDL_MapRGB(screen->format, color.r, color.g, color.b);
}


void play_sound(int snd, int chan)
{
   /* sounds:
         0 : recharge  (always on channel 0)
         1 : gun bit   (on free channel (-1), multiple channels possible)
         2 : bullet    (on free channel (-1), multiple channels possible)
         3 : explosion (always on channel 3, prio on recharge sound) 
         4 : mini explosion (always on channel 4) 
         5 : ship explosion (always on channel 5, prio on others) 
         6 : score              
         7 : character beep (on free channel
         8 : laser fire (on free channel 
         9 : ufo  (always on channel 6 and loop) 
        10 : select game (only in intro on free channel)  */
    //printf("channel: %d, %d channels are now playing\n", chan, Mix_Playing(-1));

    // Some tweaks to improve sounds (SDL_Mixer is not perfect)

    // if expliciet channel is given (for respawn and explosion, then
    // first halt the current channel
    if (Mix_Playing(chan) != 0 && chan != -1) {
          Mix_HaltChannel(chan);
          //printf("channel %d halted\n", chan);
    }
    if (chan == 3) { // stop current sound(s) when new explosion sound is requested
          if (Mix_Playing(0)) Mix_HaltChannel(0);
          if (Mix_Playing(3)) Mix_HaltChannel(3);
          if (Mix_Playing(4)) Mix_HaltChannel(4);
          if (Mix_Playing(6)) Mix_HaltChannel(6);
          //printf("explosion sound!\n");
    }
    if (chan == 5) { // stop current sound(s) when ship explosion sound is requested
          if (Mix_Playing(0)) Mix_HaltChannel(0);
          if (Mix_Playing(3)) Mix_HaltChannel(3);
          if (Mix_Playing(4)) Mix_HaltChannel(4);
          if (Mix_Playing(6)) Mix_HaltChannel(6);
          //printf("explosion sound!\n");
    }
    if (chan == 6) { // ufo
         if (Mix_Playing(0)) Mix_HaltChannel(0);
          if (Mix_Playing(3)) Mix_HaltChannel(3);
          if (Mix_Playing(4)) Mix_HaltChannel(4);
         chan = Mix_PlayChannel(chan, sounds[snd], 3);  // loop 3 times 
    } else {  
         chan = Mix_PlayChannel(chan, sounds[snd], 0);
    }     
}


void title_screen()
{
  int done, x, y, ux, uy;
  int window_size_changed;
  int scroll_x;
  Uint32 last_time;
  SDL_Event event;
  SDLKey key;
  char title_string[100];

  /* title screen loop */
  window_size_changed = 0;
  done = 0;
  ufo_start_delay = frame;
  play_sound(10,-1);    // select game 

  x = (VIDEOPAC_RES_W / 2 * factor) - (12*4*factor);  // center - 5 characters
                                                      // (large font is 12 pixels char)
  y = (VIDEOPAC_RES_H / 2 * factor) - (5 * factor);

  /* 2 asteroids and 1 ufo on title screen */
  asteroids[0].status      = 1;                      // normal, not magnetic
  asteroids[0].colour      = 2;
  asteroids[0].shape_timer = ASTEROID_SHAPE_TIMER;   // count_down timer for shape
  asteroids[0].x  = 36 * factor;
  asteroids[0].y  = 120 * factor;
  asteroids[0].xm = 0;
  asteroids[0].ym = 0;

  asteroids[1].status      = 2;                      // magnetic
  asteroids[1].colour      = 5;
  asteroids[1].shape_timer = ASTEROID_SHAPE_TIMER;   // count_down timer for shape
  asteroids[1].x  = 90 * factor;
  ship_x = asteroids[1].x;                           // to fixate magnetic asteroid
  asteroids[1].y  = 120 * factor;
  ship_y = asteroids[1].y;                           // to fixate magnetic asteroid
  asteroids[1].xm = 0;
  asteroids[1].ym = 0;

  ufo[0].status = 1;
  ufo[0].colour = 6;
  ux = 148 * factor;
  uy = 122 * factor;
  ufo[0].xm = 0;
  ufo[0].ym = 0;
  scroll_x = 0;
    
  do
  {
    last_time = SDL_GetTicks();
      
    /* Check for keypresses: */
    while (SDL_PollEvent(&event))
    {
      if (event.type == SDL_JOYBUTTONDOWN  && (event.jbutton.button == 0 ||
                                               event.jbutton.button == 1)) {     
        //printf("Joystick fire button A or B pressed, start normal game\n");      
        done = 1;
        asteroids[0].status      = 0; 
        asteroids[1].status      = 0; 
        ufo[0].status = 0;
        ship_x = screen_width / 2;
        ship_y = screen_height / 2;
      }   
      
      if (event.type == SDL_KEYDOWN)
        {
          key = event.key.keysym.sym;
          
          if (key == 49 || key == 50 || key == 51)  // keys 1, 2, 3 for difficulty
          {
             done = 1;
             asteroids[0].status      = 0; 
             asteroids[1].status      = 0; 
             ufo[0].status = 0;
             ship_x = screen_width / 2;
             ship_y = screen_height / 2;
             
             switch (key)
             {
             case 49: 
                 difficulty = 1;
                 MAX_UFOS = 1;
                 MAX_LASERS = 1;     
                 MAX_ASTEROIDS = 15;
                 UFO_RANDOMNESS = 250;    
                 break;
             case 50: 
                 difficulty = 2;
                 MAX_UFOS = 2;
                 MAX_LASERS = 2;     
                 MAX_ASTEROIDS = 25;
                 UFO_RANDOMNESS = 100;    
                 break;
             case 51: 
                 difficulty = 3;
                 MAX_UFOS = 3;
                 MAX_LASERS = 3;     
                 MAX_ASTEROIDS = 35;
                 UFO_RANDOMNESS = 50;    
                 break;
              default:
                 break;   
             }  // end switch     
           sprintf(title_string, "UFO - factor: %d - difficulty: %d", factor, difficulty);
           SDL_WM_SetCaption(title_string, "UFO");  
           }  // end key 1, 2, 3


           /* key 8: Toggle full screen */
           if (key == 56) {     
             if (full_screen == 0) {
                screen = SDL_SetVideoMode(screen_width, screen_height, 0, SDL_HWPALETTE | SDL_ANYFORMAT | SDL_FULLSCREEN);
                full_screen = 1;
                if (screen == NULL)
                {
                  fprintf(stderr, "\nWarning: I could not set up video for "
                          "full-screen.\n"
                          "The Simple DirectMedia error that occured was:\n"
                          "%s\n\n", SDL_GetError());
                }
             } else {
               screen_width  =  factor * VIDEOPAC_RES_W;           
               screen_height =  factor * VIDEOPAC_RES_H;
               screen = SDL_SetVideoMode(screen_width, screen_height, 0, SDL_HWPALETTE | SDL_ANYFORMAT);
               full_screen = 0;
               if (screen == NULL)
               {
                  fprintf(stderr, "\nWarning: I could not set up video for "
                          "larger/smaller mode.\n"
                          "The Simple DirectMedia error that occured was:\n"
                          "%s\n\n", SDL_GetError());
                }
              }

           }  // end key = full screen


           /* Handle window/screen resize */
           if (full_screen == 0) {
               window_size_changed = 0;       
               if ( key == SDLK_KP_MINUS || key == SDLK_LEFTBRACKET || key == 57) { // decrease windows size
                 if (factor > 1) {
                   printf("resize -\n");
                   window_size_changed = 1;
                   factor --;
                   }
                }
               if ( key == SDLK_KP_PLUS || key == SDLK_RIGHTBRACKET || key == 48) { // increase windows size
                 if (factor < 9) {
                   printf("resize +\n");
                   window_size_changed = 1;
                   factor ++;
                   }
               }
           }

           if (window_size_changed == 1) {
                handle_screen_resize(1);

                /* recalculate object positions (points) */
                asteroids[0].x  = 36 * factor;
                asteroids[0].y  = 120 * factor;
                asteroids[1].x  = 90 * factor;
                ship_x = asteroids[1].x;                   // to fixate magnetic asteroid
                asteroids[1].y  = 120 * factor;
                ship_y = asteroids[1].y;                   // to fixate magnetic asteroid
                ux = 148 * factor;
                uy = 122 * factor;
                scroll_x = 0;
           }    
          
           if (key == SDLK_ESCAPE)
              exit(0);
        }
      else if (event.type == SDL_QUIT)    // close window pressed
        {
          exit(0);
        }
    }  // end while (SDL_PollEvent(&event))


    /* draw black screen */
    SDL_FillRect(screen, NULL,
    SDL_MapRGB(screen->format, 0x00, 0x00, 0x00));

    frame++;
    if (frame - ufo_start_delay >= 30*3) {   // > 3 seconds delay for 1st ufo to appear
       x =  (VIDEOPAC_RES_W / 2 * factor) - (12*4*factor); 
       y = y - factor;
       if (y < 15*factor) {
          y = y + factor;
          display_instructions(-1 * scroll_x, 40);
          scroll_x++;
          if (scroll_x == 269) scroll_x = 0;
          handle_asteroids();
          draw_asteroids();
          ufo[0].x = ux + ( ( rand() % 6) - 3);
          ufo[0].y = uy + ( ( rand() % 6) - 3);
          draw_ufo();
       }   

    } else {
      // wobbly text!
      x = ( (VIDEOPAC_RES_W / 2 * factor) - (12*4*factor) + (( rand() % 4) - 2));
      y = ( (VIDEOPAC_RES_H / 2 * factor) - (5 * factor)  + (( rand() % 4) - 2));
    }
    display_select_game(x, y);

    SDL_Flip(screen);

    if (SDL_GetTicks() < last_time + 33)
        SDL_Delay(last_time + 33 - SDL_GetTicks());
      
  } // end do
  while (done == 0);
}

void display_select_game(int x, int y)
{
  SDL_Color fgColor_green   = {0,182,0};   
  SDL_Color fgColor_red     = {182,0,0};   
  SDL_Color fgColor_grey    = {182,182,182};  
  SDL_Color fgColor_yellow  = {182,182,0};  
  SDL_Color fgColor_blue    = {0,0,182};  
  SDL_Color fgColor_magenta = {182,0,182};  
  SDL_Color fgColor_cyan    = {0,182,182};  

  SDL_Rect text_position;  
  char text_line[12];    // SELECT GAME

  sprintf(text_line, "%s", "S       A  ");
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_green);
  text_position.x = x;
  text_position.y = y;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", " E       M ");
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_yellow);
  SDL_BlitSurface(text, NULL , screen, &text_position);
    
  sprintf(text_line, "%s", "  L       E");
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_blue);
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "   E       ");
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_magenta);
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "    C      ");
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_cyan);
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "     T     ");
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_grey);
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "       G   ");
  text = TTF_RenderText_Solid(font_large, text_line, fgColor_red);
  SDL_BlitSurface(text, NULL , screen, &text_position);
}


void display_instructions(int scroll_x, int scroll_y)
{
  SDL_Color fgColor_green   = {0,182,0};   
  SDL_Color fgColor_red     = {182,0,0};   
  SDL_Color fgColor_grey    = {182,182,182};  
  SDL_Color fgColor_yellow  = {182,182,0};  
  SDL_Color fgColor_blue    = {0,0,182};  
  SDL_Color fgColor_magenta = {182,0,182};  
  SDL_Color fgColor_cyan    = {0,182,182};  

  SDL_Rect text_position;  
  char text_line[240]; 
  //"        PRESS 1 FOR NORMAL 2 FOR HARD 3 FOR INSANE                PRESS 1 FOR NORMAL 2 FOR HARD 3 FOR INSANE "

  sprintf(text_line, "%s", 
    "          PRESS                                                     PRESS 1                                    ");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_green);
  text_position.x = scroll_x * factor;
  text_position.y = scroll_y * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", 
    "                1 FOR NORMAL                                              1 FOR NORMAL                         ");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_cyan);
  text_position.x = scroll_x * factor;
  text_position.y = scroll_y * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", 
    "                             2 FOR HARD                                                2 FOR HARD              ");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_magenta);
  text_position.x = scroll_x * factor;
  text_position.y = scroll_y * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", 
    "                                        3 FOR INSANE                                              3 FOR INSANE ");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_yellow);
  text_position.x = scroll_x * factor;
  text_position.y = scroll_y * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);



  sprintf(text_line, "%s", "CONTROLS:");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_green);
  text_position.x = 30 * factor;
  text_position.y = 70 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "                8 FULL SCREEN");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_yellow);
  text_position.x = 30 * factor;
  text_position.y = 70 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);


  sprintf(text_line, "%s", "JOYSTICK");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_magenta);
  text_position.x = 30 * factor;
  text_position.y = 80 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "                9 WINDOW SMALLER");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_cyan);
  text_position.x = 30 * factor;
  text_position.y = 80 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);


  sprintf(text_line, "%s", "ARROW KEYS");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_yellow);
  text_position.x = 30 * factor;
  text_position.y = 90 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "                0 WINDOW LARGER");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_grey);
  text_position.x = 30 * factor;
  text_position.y = 90 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "CTRL = FIRE");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_red);
  text_position.x = 30 * factor;
  text_position.y = 100 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "                ESC = QUIT");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_blue);
  text_position.x = 30 * factor;
  text_position.y = 100 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

  sprintf(text_line, "%s", "1 pt       3 pts       10 pts");
  text = TTF_RenderText_Solid(font_small, text_line, fgColor_green);
  text_position.x = 30 * factor;
  text_position.y = 130 * factor;
  SDL_BlitSurface(text, NULL , screen, &text_position);

 }  // ufo.c
