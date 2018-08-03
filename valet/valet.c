// Valet -- http://tinyc.games -- (c) 2016 Jer Wilson
//
// Valet is a game about parking cars with bad or no brakes!

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define SDL_DISABLE_IMMINTRIN_H
#include <SDL.h>
#include <SDL_ttf.h>

#define W 480
#define H 600
#define NCARS 20
#define PI2 (M_PI/2)
#define PI4 (M_PI/4)

enum gamestates {READY, ALIVE, GAMEOVER} gamestate = READY;

struct car {
        float speed;
        float lateral;
        float angle;
        float angv;
        float x;
        float y;
} absolute_cars[2][NCARS], *cars0 = absolute_cars[0], *cars1 = absolute_cars[1];

int brake, turn_left, turn_right;
int cur;
int target_fps = 60;

SDL_Event event;
SDL_Renderer *renderer;
SDL_Surface *surf;
SDL_Texture *background;
SDL_Texture *car[4];
TTF_Font *font;

void setup();
void new_game();
void new_car();
void update_stuff();
void draw_stuff();
void text(char *fstr, int value, int height);
float ang_fix(float angle);
float ang_cmp(float ang0, float ang1);

//the entry point and main game loop
int main()
{
        setup();
        int press;

        for(;;)
        {
                while(SDL_PollEvent(&event)) switch(event.type)
                {
                        case SDL_QUIT:
                                exit(0);
                        case SDL_KEYDOWN:
                                if(event.key.keysym.sym == SDLK_ESCAPE) exit(0);

                                if(gamestate == READY)
                                {
                                        new_game();
                                        break;
                                }

                                //fall thru
                        case SDL_KEYUP:
                                press = (event.type == SDL_KEYDOWN ? 1 : 0);

                                switch(event.key.keysym.sym)
                                {
                                        case SDLK_SPACE:
                                                brake = press;
                                                break;
                                        case SDLK_LEFT:
                                                turn_left = press;
                                                break;
                                        case SDLK_RIGHT:
                                                turn_right = press;
                                                break;
                                        case SDLK_s:
                                                target_fps = 2;
                                                break;
                                        case SDLK_f:
                                                target_fps = 60;
                                                break;
                                }
                }

                update_stuff();
                draw_stuff();
                SDL_Delay(1000 / target_fps);
        }
}

//initial setup to get the window and rendering going
void setup()
{
        srand(time(NULL));

        SDL_Init(SDL_INIT_VIDEO);
        SDL_Window *win = SDL_CreateWindow("Valet",
                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
        if(!renderer) renderer = SDL_CreateRenderer(win, -1, 0);
        if(!renderer) exit(fprintf(stderr, "Could not create SDL renderer\n"));

        surf = SDL_LoadBMP("res/background.bmp");
        background = SDL_CreateTextureFromSurface(renderer, surf);

        for(int i = 0; i < 4; i++)
        {
                char file[80];
                sprintf(file, "res/car-%d.bmp", i);
                surf = SDL_LoadBMP(file);
                SDL_SetColorKey(surf, 1, 0xffff00);
                car[i] = SDL_CreateTextureFromSurface(renderer, surf);
        }

        TTF_Init();
        font = TTF_OpenFont("res/LiberationMono-Regular.ttf", 42);
}

//start a new game
void new_game()
{
        int i;
        for(i = 0; i < NCARS; i++) cars1[i].x = W*2;
        gamestate = ALIVE;
        cur = 0;
        new_car();
}

void new_car()
{
        cars1[cur].speed = 40.0f + rand() % 30;
        cars1[cur].lateral = 0.0f;
        cars1[cur].angle = -M_PI;
        cars1[cur].x = 4*W/5;
        cars1[cur].y = H - 5;
}

//when we hit something
void game_over()
{
        gamestate = GAMEOVER;
}

//update everything that needs to update on its own, without input
void update_stuff()
{
        struct car *c;

        if(gamestate != ALIVE) return;

        /*
        if(cars0 != absolute_cars[0])
        {
                cars0 = absolute_cars[0];
                cars1 = absolute_cars[1];
        }
        else
        {
                cars0 = absolute_cars[1];
                cars1 = absolute_cars[0];
        }
        */

        // movements
        for(int i = 0; i < NCARS; i++)
        {
                c = cars1 + i;

                if(cur == i && c->speed > 0.1f)
                {
                        c->angle += ((turn_left - turn_right) * c->speed)/(M_PI * 150.0f);
                        c->angle = ang_fix(c->angle);
                }

                c->x += sinf(c->angle) * c->speed * 0.1f;
                c->y += cosf(c->angle) * c->speed * 0.1f;

                c->x += sinf(c->angle + PI2) * c->lateral * 0.1f;
                c->y += cosf(c->angle + PI2) * c->lateral * 0.1f;

                // slow down or brake
                float delta = (brake || cur != i) ? 0.5f : 0.1f;
                float latdelta = fabsf(c->lateral) * 0.01f;
                delta += latdelta;
                if(     c->speed >=  delta) c->speed -= delta;
                else if(c->speed <= -delta) c->speed += delta;
                else                              c->speed  = 0.0f;

                // always brake laterally
                float latang = c->speed > 0 ? latdelta : -latdelta;
                if(     c->lateral >=  3.0f) { c->lateral -= 3.0f; c->speed += 2.0f; c->angle += latang * 0.1f; }
                else if(c->lateral <= -3.0f) { c->lateral += 3.0f; c->speed -= 2.0f; c->angle -= latang * 0.1f; }
                else                         { c->lateral  = 0.0f; }

                c->angle = ang_fix(c->angle);
        }

        // car-car collisions
        for(int i = 0; i < NCARS; i++) for(int j = i+1; j < NCARS; j++)
        {
                if(fabsf(cars1[i].x - cars1[j].x) < 20.0f &&
                   fabsf(cars1[i].y - cars1[j].y) < 20.0f)
                {
                        float si = cars1[i].speed;
                        float sj = cars1[j].speed;
                        float li = cars1[i].lateral;
                        float lj = cars1[j].lateral;

                        cars1[i].speed   *= 0.05f;
                        cars1[j].speed   *= 0.05f;
                        cars1[i].lateral *= 0.05f;
                        cars1[j].lateral *= 0.05f;

                        // random angle change on hi speed collision
                        if(fabsf(cars1[i].speed) > 2.0f || cars1[j].speed > 2.0f)
                        {
                                cars1[i].angle += (rand()%1000 - 500) * 0.0001;
                                cars1[j].angle += (rand()%1000 - 500) * 0.0001;
                                cars1[i].angle = ang_fix(cars1[i].angle);
                                cars1[j].angle = ang_fix(cars1[j].angle);
                        }

                        float ai = cars1[i].angle;
                        float aj = cars1[j].angle;
                        float ix = sinf(ai) * si + sinf(ai + PI2) * li;
                        float iy = cosf(ai) * si + cosf(ai + PI2) * li;
                        float jx = sinf(aj) * sj + sinf(aj + PI2) * lj;
                        float jy = cosf(aj) * sj + cosf(aj + PI2) * lj;

                        cars1[i].speed   += (sinf(ai      )*jx + cosf(ai      )*jy) * 0.95f;
                        cars1[j].speed   += (sinf(aj      )*ix + cosf(aj      )*iy) * 0.95f;
                        cars1[i].lateral += (sinf(ai + PI2)*jx + cosf(ai + PI2)*jy) * 0.95f;
                        cars1[j].lateral += (sinf(aj + PI2)*ix + cosf(aj + PI2)*iy) * 0.95f;
                }
        }

        // car-wall collisions
        for(int i = 0; i < NCARS; i++)
        {
                if(fabsf(cars1[i].y - 26) < 10.0f)
                {
                        float si = cars1[i].speed;
                        float li = cars1[i].lateral;

                        cars1[i].speed   *= 0.15f;
                        cars1[i].lateral *= 0.15f;

                        float ai = cars1[i].angle;
                        float wallangle = PI2;

                        float ix = sinf(ai) * si + sinf(ai + PI2) * li;
                        float iy = cosf(ai) * si + cosf(ai + PI2) * li;

                        if(fabsf(ang_cmp(ai, wallangle)) < PI4)
                        {
                                if(fabsf(cars1[i].speed) > 0.01f)
                                        printf("ai=%f wallangle=%f PI4=%f\n", ai, wallangle, PI4);
                                cars1[i].angle = ang_fix((cars1[i].angle*9 + wallangle) / 10.0f);
                                cars1[i].speed *= (iy < 0 ? -0.99f : 0.99f);
                        }
                        else if(fabsf(ang_cmp(ai, wallangle + M_PI)) < PI4)
                        {
                                if(fabsf(cars1[i].speed) > 0.01f)
                                        printf("ai=%f wallangle=%f PI4=%f\n", ai, wallangle + M_PI, PI4);
                                cars1[i].angle = ang_fix((cars1[i].angle*9 + wallangle + M_PI) / 10.0f);
                                cars1[i].speed *= (iy < 0 ? -0.99f : 0.99f);
                        }
                        else
                        {
                                cars1[i].speed *= (iy < 0 ? -0.99f : 0.99f);
                        }
                }
        }

        // has our car stopped or gone oob?
        c = cars1 + cur;
        if(c->speed == 0.0f || c->x < 0 || c->x > W || c->y < 0 || c->y > H)
        {
                if(++cur < NCARS) new_car(); else new_game();
        }
}

//draw everything in the game on the screen
void draw_stuff()
{
        SDL_Rect dest = {0, 0, W, H};
        SDL_RenderCopy(renderer, background, NULL, &dest);

        //draw cars
        int i;
        for(i = 0; i < NCARS; i++)
        {
                SDL_RenderCopyEx(renderer, car[0], NULL,
                        &(SDL_Rect){cars1[i].x, cars1[i].y, 40, 40},
                        -cars1[i].angle*180/M_PI, NULL, 0);
        }

        if(gamestate == READY) text("Press any key", 0, 150);

        SDL_RenderPresent(renderer);
}

void text(char *fstr, int value, int height)
{
        if(!font) return;
        int w, h;
        char msg[80];
        snprintf(msg, 80, fstr, value);
        TTF_SizeText(font, msg, &w, &h);
        SDL_Surface *msgsurf = TTF_RenderText_Blended(font, msg, (SDL_Color){255, 255, 255});
        SDL_Texture *msgtex = SDL_CreateTextureFromSurface(renderer, msgsurf);
        SDL_Rect fromrec = {0, 0, msgsurf->w, msgsurf->h};
        SDL_Rect torec = {(W - w)/2, height, msgsurf->w, msgsurf->h};
        SDL_RenderCopy(renderer, msgtex, &fromrec, &torec);
        SDL_DestroyTexture(msgtex);
        SDL_FreeSurface(msgsurf);
}

float ang_fix(float angle)
{
        angle = fmodf(angle, M_PI*2);
        if(angle < 0)
                angle += M_PI*2;
        return angle;
}

float ang_cmp(float ang0, float ang1)
{
        float diff = ang_fix(ang0 - ang1);
        if(diff > M_PI)
                diff -= M_PI;
        return diff;
}
