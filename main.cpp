#include <iostream>
#include <random>
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <chrono>
#include "FastNoiseLite.h"

// Configure terminal raw mode for instant key reading
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

int main()
{
    const int width = 80;        // 80 meters
    const int height = 25;       // 25 meters
    const float tileSize = 1.0f; // 1 m² per tile
    const float speed = 1.5f;    // 1.5 meters per second

    // Generate terrain using Perlin noise
    FastNoiseLite noise;
    std::mt19937 rng(std::random_device{}());
    noise.SetSeed(static_cast<int>(rng()));
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetFrequency(0.05f);

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

    // Build map
    std::vector<std::vector<char>> map(height, std::vector<char>(width));
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            map[y][x] = getSymbol(noise.GetNoise((float)x, (float)y));

    // Cursor position in meters (float for smooth motion)
    float cx = width / 2.0f;
    float cy = height / 2.0f;

    setRawMode(true);

    char key = 0;
    auto lastTime = std::chrono::steady_clock::now();

    while (key != 'q')
    { // press 'q' to quit
        // Measure elapsed time
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> deltaTime = now - lastTime;
        lastTime = now;
        float dt = deltaTime.count();

        // Non-blocking input
        read(STDIN_FILENO, &key, 1);

        // Move based on time and speed
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

        // Clamp position to map bounds
        if (cx < 0)
            cx = 0;
        if (cy < 0)
            cy = 0;
        if (cx > width - 1)
            cx = width - 1;
        if (cy > height - 1)
            cy = height - 1;

        // Clear screen
        std::cout << "\033[H\033[J";

        // Render
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

        // Small delay to reduce flicker
        usleep(16000); // ~60 FPS
    }

    setRawMode(false);
    std::cout << "\033[H\033[J";
    return 0;
}
