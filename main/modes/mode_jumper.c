//==============================================================================
// Includes
//==============================================================================
#include <stdlib.h>
#include <string.h> 
#include <math.h>

#include "swadgeMode.h"
#include "mode_jumper.h"

#include "esp_log.h"
#include "display.h"
#include "bresenham.h" // ???
 

//==============================================================================
// structs
//==============================================================================

typedef enum
{
    JUMPER_STATE_MENU,
    JUMPER_STATE_PREGAME,
    JUMPER_STATE_INGAME,
    JUMPER_STATE_POSTGAME
} jumperGameState_t;


typedef enum
{
    STANDING,
    JUMPING,
    LANDING,
    DEAD
} playerState_t;

typedef struct
{
    uint8_t x;
    uint8_t y;
    int8_t cellX;
    int8_t cellY;

    uint8_t sourceXLocation;
    uint8_t sourceYLocation;
    uint8_t destinationXLocation;
    uint8_t destinationYLocation;

    uint8_t characterHeight;
    uint8_t characterWidth;
    uint8_t characterXOffset;
    uint8_t characterYOffset;

    uint16_t characterTimer;

    playerState_t state;

    wsg_t sprite;
    uint8_t direction; //This can be optimized but leave it alone
} jumperJumper_t; //There has to be a better name

typedef struct
{
    uint8_t colorIndex;
} jumperStage_t;

typedef struct
{
    wsg_t block[8]; //Block 7 is the white toggle block
    jumperStage_t grid[30]; //A nice 6x5 grid 
    uint16_t blockXDistance;
    uint16_t blockYDistance;
    int8_t blockNextLineHorizontalOffset;
    uint8_t blockHeight;
    uint8_t blockWidth;

    uint16_t jumpSpeed;

    uint8_t stageOffsetX;
    uint8_t stageOffsetY;

    uint8_t level;
    uint8_t maxLevel;
    int16_t levelTimer;

    display_t * disp;
    font_t ibm_vga8;

    //post game data
    uint16_t flickerRate;
    uint8_t flickerMax; // There should be 20
    uint8_t flickerCount;
    uint8_t eolToggle;
    //uint16_t ;

} jumper_t;

//==============================================================================
// Funktions Prototypes
//==============================================================================
void jumperEnterMode(display_t * disp);
void jumperExitMode(void);
void jumperMainLoop(int64_t elapsedUs);
void jumperButtonCallback(buttonEvt_t* evt);
void jumperMenuButtonCallback(buttonEvt_t * evt);
void jumperInGameButtonCallback(buttonEvt_t * evt);

void jumperDraw(void);
void jumperInput(void);
void jumperProcess(int64_t elapsedUs);

void jumperDrawMenu(void);
void jumperDrawInGame(void);
void jumperStartNewLevel(void);
void jumperOnJumpComplete(jumperJumper_t* jumpGuy);
void jumperInGameInput(void);
void jumperPregameProcess(int64_t elapsedUs);
void jumperIngameProcess(int64_t elapsedUs);
void jumperPostgameProcess(int64_t elapsedUs);
void jumperSetJumpDestination(uint8_t x, uint8_t y);

//TODO: Remove?
void drawBlock(int sx, int sy, paletteColor_t color);

//==============================================================================
// Constants
//==============================================================================

//==============================================================================
// Variables
//==============================================================================

jumperJumper_t player;
jumper_t * jumper;
jumperGameState_t currentState;

swadgeMode modeJumper = 
{
    .modeName = "Jumper",
    .fnEnterMode = jumperEnterMode, // OnStart
    .fnExitMode = jumperExitMode,
    .fnMainLoop = jumperMainLoop,
    .fnButtonCallback = jumperButtonCallback,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL
};

//Run through initialization so there's little to no cruft floating around after the game is closed
void jumperEnterMode(display_t * disp)
{
    jumper = (jumper_t *)malloc(sizeof(jumper_t));
    memset(jumper, 0, sizeof(jumper_t));

    jumper->disp = disp; 
    jumper->blockXDistance = 6;
    jumper->blockYDistance = 26;
    jumper->blockNextLineHorizontalOffset = 0;
    jumper->jumpSpeed = 500;

    jumper->blockWidth = 28;
    jumper->blockHeight = 26;
    jumper->stageOffsetY = 90;

    jumper->level = 1;
    jumper->maxLevel = 2;

    jumper->flickerRate = 250; //Quarter of a second between toggles;
    jumper->eolToggle = 1;
    jumper->flickerCount = 0;
    jumper->flickerMax = 20;

    jumperStartNewLevel();

    loadWsg("fakeplayer.wsg",&player.sprite);

    //initialize stage
    loadWsg("block_0.wsg",&jumper->block[0]);
    loadWsg("block_1.wsg",&jumper->block[1]);
    loadWsg("block_2.wsg",&jumper->block[2]);
    loadWsg("block_0.wsg",&jumper->block[3]);
    loadWsg("block_0.wsg",&jumper->block[4]);
    loadWsg("block_0.wsg",&jumper->block[5]);
    loadWsg("block_0.wsg",&jumper->block[6]);
    loadWsg("block_7.wsg",&jumper->block[7]);

    // for(int i = 0; i < (int)sizeof(jumper->grid); i++)
    for(int i = 0; i < 30; i++) //Harcoded til I figure this out
    {
        jumper->grid[i].colorIndex = 0;
    }

    //TODO: Title screen

    currentState = JUMPER_STATE_MENU;

    loadFont("ibm_vga8.font", &jumper->ibm_vga8);
}

void jumperMainLoop(int64_t elapsedUs)
{
    //Clear screen before drawing?
    jumper->disp->clearPx();
    
   jumperInput();
   jumperProcess(elapsedUs); //Always process and collision before you draw!
   jumperDraw();

   //Jumper frame cleanup
}

void jumperDraw()
{
    char textBuffer[8];
    switch (currentState)
    {
       case JUMPER_STATE_MENU:
            jumperDrawMenu();
            break;
        case JUMPER_STATE_PREGAME:
            jumperDrawInGame();
            sprintf(textBuffer, "LEVEL %d", jumper->level);
            drawText(jumper->disp, &(jumper->ibm_vga8), c555, textBuffer, ((jumper->disp->h - jumper->ibm_vga8.h) /2) - 28,  (jumper->disp->h - jumper->ibm_vga8.h) / 2);
            break;
        case JUMPER_STATE_INGAME:
        case JUMPER_STATE_POSTGAME:
            jumperDrawInGame();
            break;
        default:
            break;
   }
}


//This is different than the event. This is needed so the player can hold a button and continue to move in that direction
void jumperInput()
{
    switch (currentState)
    {
       case JUMPER_STATE_MENU:
            break;
        case JUMPER_STATE_INGAME:
            jumperInGameInput();
            break;
        case JUMPER_STATE_PREGAME:
        case JUMPER_STATE_POSTGAME:
        default:
            break;
   }
}

void jumperProcess(int64_t elapsedUs)
{
    //Does
    switch (currentState)
    {
        case JUMPER_STATE_MENU:
            //Update title animations
            break;
        case JUMPER_STATE_PREGAME:
            jumperPregameProcess(elapsedUs);
            break;
        case JUMPER_STATE_INGAME:
            jumperIngameProcess(elapsedUs);
            break;
        case JUMPER_STATE_POSTGAME:
            //flash the level and the such for a few seconds
            jumperPostgameProcess(elapsedUs);
            break;
        default:
            break;
    }
}

void jumperButtonCallback(buttonEvt_t* evt)
{
    switch (currentState)
    {
        case JUMPER_STATE_MENU:
            jumperMenuButtonCallback(evt);
            break;
        case JUMPER_STATE_PREGAME:
            break;
        case JUMPER_STATE_INGAME:
            jumperInGameButtonCallback(evt);
            break;
        case JUMPER_STATE_POSTGAME:
            break;
        default:
            break;
    }
}

void jumperExitMode()
{
    //TODO: Add cleanup
    freeFont(&jumper->ibm_vga8);
    freeWsg(&player.sprite);

    //clear stage
    freeWsg(&jumper->block[0]);
    freeWsg(&jumper->block[1]);
    freeWsg(&jumper->block[2]);
    freeWsg(&jumper->block[3]);
    freeWsg(&jumper->block[4]);
    freeWsg(&jumper->block[5]);
    freeWsg(&jumper->block[6]);

    free(jumper);
}

//==============================================================================
// Game Menu
//==============================================================================

void drawBlock(int sx, int sy, paletteColor_t color)
{  
    plotLine(jumper->disp, sx+2, sy, sx + jumper->blockWidth - 2, sy, color);  
    for (int i = 1; i < 11; i++)
    {
        plotLine(jumper->disp, sx+2, sy+i, sx + jumper->blockWidth, sy+i, color);
    }
    
    for (int i = 11; i < 26; i++)
    {
        plotLine(jumper->disp, sx+2, sy+i, sx + jumper->blockWidth, sy+i, c012);
    }
    
    plotLine(jumper->disp, sx+1, sy+1, sx+1, sy+23, c111  );
    plotLine(jumper->disp, sx, sy+1, sx, sy+21, c111  );
}

void jumperDrawMenu()
{
    plotLine(jumper->disp, 2, 2, 238, 238, c050);    
    drawText(jumper->disp, &jumper->ibm_vga8, c500, "TODO: MENU", 32, 64);
}

void jumperMenuButtonCallback(buttonEvt_t * evt)
{
    if (evt->down)
    {
        switch (evt->button)
        {
            case UP:
            {
                // ESP_LOGI("JUMPER","UP");
                break;
            }

            case BTN_A:
            case BTN_B:
            case START:
                currentState = JUMPER_STATE_PREGAME;
                break;   
            case RIGHT:
            case LEFT:
            case SELECT:
            case DOWN:   
            default:
                break;
        }
    }
}


//==============================================================================
// In Game
//==============================================================================
void jumperStartNewLevel()
{
    for(int i = 0; i < 30; i++) //TODO: Fix grid check
    {
        jumper->grid[i].colorIndex = 0;             
    }

    jumper->levelTimer = 3000; //Three seconds to start new level
    
    //initialize player
    player.x = 26;
    player.y = 85;
    player.characterTimer = 0;
    player.cellX = 0;
    player.cellY = 0;    
    player.characterXOffset = 0;
    player.characterYOffset = 12;
    player.direction = 0;
    player.state = STANDING;
    player.sourceXLocation = player.x;
    player.sourceYLocation = player.y;
    player.destinationXLocation = player.x;
    player.destinationYLocation = player.y;
}

void jumperDrawInGame()
{
    char tempStr[128];
    sprintf(tempStr,"%d , %d", player.cellX, jumper->grid[0].colorIndex);
    drawText(jumper->disp, &jumper->ibm_vga8, c200, "M*Boort", 94, 8);
    drawText(jumper->disp, &jumper->ibm_vga8, c500, "M*Boort", 96, 9);

    //Draw layer 1 first... the background
    for (int i = 0; i < 30; i++)
    {
        uint8_t colorIndex = (uint8_t)jumper->grid[i].colorIndex;

        drawWsg(jumper->disp, &jumper->block[colorIndex], (((i % 6)+1) * (jumper->blockWidth+ jumper->blockXDistance)) - 16 + ((i / 6) * jumper->blockNextLineHorizontalOffset), ((i/6) * jumper->blockYDistance) + jumper->stageOffsetY);
    }

    //Use character sorted list to draw characters from top to bottom
    drawWsg(jumper->disp, &player.sprite, player.x, player.y);

   // drawText(jumper->disp, &jumper->ibm_vga8, c555, tempStr, 32, 28);
}

void jumperSetJumpDestination(uint8_t x, uint8_t y)
{
    player.sourceXLocation = player.x;
    player.sourceYLocation = player.y;
    player.destinationXLocation = player.x + x;
    player.destinationYLocation = player.y + y;
    player.characterTimer = 0;
    player.state = JUMPING;
}

void jumperInGameInput()
{
    switch(player.state)
    {
        case STANDING:
            //If player is standing check directions and keep moving
            switch(player.direction)
            {
                case UP:
                    if (player.cellY == 0)
                    {
                        //Don't let them jump off maybe?
                        return;
                    }
                    jumperSetJumpDestination(-jumper->blockNextLineHorizontalOffset, -jumper->blockYDistance);

                    break;   
                case RIGHT:
                    if (player.cellX == 5)
                    {
                        //Don't let them jump off, maybe?
                        return;
                    }
                    jumperSetJumpDestination(jumper->blockWidth+jumper->blockXDistance, 0);
                    break;

                case DOWN:
                    if (player.cellY == 4)
                    {
                        //Don't let them jump off, maybe?
                        return;
                    }          
                    jumperSetJumpDestination(jumper->blockNextLineHorizontalOffset, jumper->blockYDistance);
                    break;   
                case LEFT:
                
                    if (player.cellX == 0)
                    {
                        //Don't let them jump off, maybe?
                        return;
                    }
                    jumperSetJumpDestination(-jumper->blockWidth - jumper->blockXDistance, 0);
                    break;    
                case 0:
                default:
                    //stand there and boogie
                    break;
            }
            break;
        case JUMPING:
            //If jumping make sure they're not trying to use non directional buttons
            break;
        case LANDING:            
            break;
        case DEAD:
            //Keep waiting till the timer runs out then respawn
            break;
        default:
            break;
    }
}

void jumperPregameProcess(int64_t elapsedUs)
{
    jumper->levelTimer -= (int)((float)elapsedUs / 1000);
    
    if (jumper->levelTimer <= 0)
    {
        currentState = JUMPER_STATE_INGAME;
    }

}


//TODO: Properly label Postgame Process
void jumperPostgameProcess(int64_t elapsedUs)
{
    jumper->levelTimer -= (int)((float)elapsedUs / 1000);
    
    if (jumper->levelTimer <= 0)
    {
        jumper->levelTimer += jumper->flickerRate;
        jumper->eolToggle = (jumper->eolToggle + 1) % 2;
        jumper->flickerCount++;

        for(int i = 0; i < 30; i++) //TODO: Fix grid check
        {
            jumper->grid[i].colorIndex = (jumper->eolToggle == 0) ? jumper-> level - 1 : 7;             
        }

        if (jumper->flickerCount > jumper->flickerMax)
        {
            if (jumper->level >= jumper->maxLevel)
                currentState = JUMPER_STATE_MENU;
            else
            {
                jumperStartNewLevel();
                currentState = JUMPER_STATE_PREGAME;    
            }
        }
    }
}

void jumperIngameProcess(int64_t elapsedUs)
{
    float per = 0;
    switch (player.state)
    {
        case DEAD:
            //update death timer
            break;
        case STANDING:
            //update idle timer, when timer runs out, boogie
            break;
        case JUMPING:
            //update jump timer/distance make sure player doesn't go past
            player.characterTimer += (elapsedUs/jumper->jumpSpeed);

            if (player.characterTimer >= jumper->jumpSpeed)
            {
                //Done jumping
                player.state = LANDING;
                player.characterTimer = jumper->jumpSpeed;
                player.x =  player.sourceXLocation = player.destinationXLocation;
                player.y = player.sourceYLocation = player.destinationYLocation;
                jumperOnJumpComplete(&player);
                return;
            }
            per =  (float)player.characterTimer/jumper->jumpSpeed;
            player.x = (player.sourceXLocation + (player.destinationXLocation - player.sourceXLocation) *   (float)player.characterTimer/jumper->jumpSpeed) ;
            player.y = (player.sourceYLocation + (player.destinationYLocation - player.sourceYLocation) *   (float)player.characterTimer/jumper->jumpSpeed) - (sin(per * 3.14) * 8);
            break;
        case LANDING:
            player.characterTimer += (elapsedUs/jumper->jumpSpeed);
            if (player.characterTimer > 1000)
            {
                player.state = STANDING;
            }
            break;
        default:
            break;
    }
}

void jumperOnJumpComplete(jumperJumper_t * jumpGuy)
{
    bool gameWin = true;
    jumpGuy->cellX = (jumpGuy->characterXOffset + jumpGuy->x - jumper->stageOffsetX)/(float)(jumper->blockXDistance+jumper->blockWidth);
    jumpGuy->cellY = (jumpGuy->characterYOffset + jumpGuy->y - jumper->stageOffsetY)/(float)jumper->blockYDistance;

    ESP_LOGI("JUMPER", "Grid: %d %d %d", jumpGuy->cellX, jumpGuy->cellY, (jumpGuy->cellX + (jumpGuy->cellY * 6)));
    
    if(jumper->grid[(jumpGuy->cellX + (jumpGuy->cellY * 6))].colorIndex < jumper->level)
        jumper->grid[(jumpGuy->cellX + (jumpGuy->cellY * 6))].colorIndex++;

    //Check to make sure the game isn't complete
    for(int i = 0; i < 30; i++) //TODO: Fix grid check
    {
        if (jumper->grid[i].colorIndex != jumper->level)
        {
            gameWin = false;
            break;
        }
    }

    if (gameWin)
    {
        jumper->level++;
        jumper->levelTimer = 0;
        currentState = JUMPER_STATE_POSTGAME;
        jumper->flickerCount = 0;
    }
}

void jumperInGameButtonCallback(buttonEvt_t * evt)
{
    //ESP_LOGI("JUMPER", "Time: %d %d %d",player.sourceYLocation,  player.y, player.destinationYLocation);
    
    if (evt->down)
    {
        switch (evt->button)
        {
            case UP:
            case RIGHT:
            case DOWN:
            case LEFT:
                player.direction = evt->button;
                break;
            case BTN_A:
            case BTN_B:
            case START:
            case SELECT:
            default:
                break;
        }
    }
    else //Button was released
    {
        //There is a cleaner way to write this, I'm just tired
        if (evt->button == player.direction) player.direction = 0;
    }
}


