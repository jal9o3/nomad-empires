#include <iostream>
#include <random>
#include <vector>
#include <termios.h>
#include <unistd.h>
#include "FastNoiseLite.h"

// Disable canonical input and echoing
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
    const int width = 80;
    const int height = 25;

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

    // Cursor position
    int cx = width / 2;
    int cy = height / 2;

    setRawMode(true);

    char key = 0;
    while (key != 'q')
    { // Press 'q' to quit
        // Clear screen
        std::cout << "\033[H\033[J";

        // Render terrain with cursor
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                if (x == cx && y == cy)
                    std::cout << 'X'; // cursor marker
                else
                    std::cout << map[y][x];
            }
            std::cout << '\n';
        }

        // Read key press (non-blocking)
        read(STDIN_FILENO, &key, 1);

        // Move cursor based on key
        if (key == 'w' && cy > 0)
            cy--;
        else if (key == 's' && cy < height - 1)
            cy++;
        else if (key == 'a' && cx > 0)
            cx--;
        else if (key == 'd' && cx < width - 1)
            cx++;
    }

    setRawMode(false);
    std::cout << "\033[H\033[J";
    return 0;
}
