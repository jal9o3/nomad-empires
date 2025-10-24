#include <iostream>
#include <random>
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <chrono>
#include "FastNoiseLite.h"

// Terminal raw mode
void setRawMode(bool enable)
{
    static struct termios oldt;
    struct termios newt;
    if (enable)
    {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    }
    else
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

// Generate one chunk of terrain
void generateChunk(std::vector<std::vector<char>> &map,
                   FastNoiseLite &noise,
                   int chunkX, int chunkY)
{
    const int width = map[0].size();
    const int height = map.size();

    auto getSymbol = [](float v)
    {
        if (v < -0.3f)
            return '.';
        if (v < 0.0f)
            return ':';
        if (v < 0.3f)
            return '*';
        if (v < 0.6f)
            return '#';
        return '@';
    };

    // Offset coordinates by chunk position
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            float wx = (chunkX * width + x);
            float wy = (chunkY * height + y);
            map[y][x] = getSymbol(noise.GetNoise(wx, wy));
        }
}

int main()
{
    const int width = 80;  // chunk width in meters
    const int height = 25; // chunk height in meters
    const float tileSize = 1.0f;
    const float speed = 1.5f;

    // Configure noise generator
    FastNoiseLite noise;
    noise.SetSeed(std::random_device{}());
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetFrequency(0.05f);

    std::vector<std::vector<char>> map(height, std::vector<char>(width));
    int chunkX = 0;
    int chunkY = 0;
    generateChunk(map, noise, chunkX, chunkY);

    // Cursor position (local to chunk)
    float cx = width / 2.0f;
    float cy = height / 2.0f;

    setRawMode(true);
    char key = 0;
    auto lastTime = std::chrono::steady_clock::now();

    while (key != 'q')
    {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> deltaTime = now - lastTime;
        lastTime = now;
        float dt = deltaTime.count();

        read(STDIN_FILENO, &key, 1);

        float dx = 0, dy = 0;
        if (key == 'w')
            dy -= speed * dt / tileSize;
        if (key == 's')
            dy += speed * dt / tileSize;
        if (key == 'a')
            dx -= speed * dt / tileSize;
        if (key == 'd')
            dx += speed * dt / tileSize;

        cx += dx;
        cy += dy;

        // Handle chunk transitions
        if (cx < 0)
        {
            chunkX -= 1;
            cx += width;
            generateChunk(map, noise, chunkX, chunkY);
        }
        else if (cx >= width)
        {
            chunkX += 1;
            cx -= width;
            generateChunk(map, noise, chunkX, chunkY);
        }

        if (cy < 0)
        {
            chunkY -= 1;
            cy += height;
            generateChunk(map, noise, chunkX, chunkY);
        }
        else if (cy >= height)
        {
            chunkY += 1;
            cy -= height;
            generateChunk(map, noise, chunkX, chunkY);
        }

        // Render
        std::cout << "\033[H\033[J";
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                if (x == (int)cx && y == (int)cy)
                    std::cout << 'X';
                else
                    std::cout << map[y][x];
            }
            std::cout << '\n';
        }

        std::cout << "Chunk: (" << chunkX << ", " << chunkY << ")\n";
        std::cout << "Position: " << cx << "m, " << cy << "m\n";

        usleep(16000); // ~60 FPS
    }

    setRawMode(false);
    std::cout << "\033[H\033[J";
    return 0;
}
