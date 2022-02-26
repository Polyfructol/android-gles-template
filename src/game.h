
typedef struct GameInputs
{
    int displayWidth;
    int displayHeight;

    float deltaTime;

    float touchX;
    float touchY;
} GameInputs;

typedef struct Game Game;
Game* game_Init();
void game_Terminate(Game* game);
void game_LoadGPUData(Game* game);
void game_UnloadGPUData(Game* game);
void game_Update(Game* game, const GameInputs* inputs);
