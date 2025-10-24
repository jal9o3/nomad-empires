#include <iostream>
#include <random>
#include <algorithm>
#include <vector>
#include <termios.h>
#include <unistd.h>
#include <chrono>
#include <cmath>
#include "FastNoiseLite.h"

// Enable/disable raw terminal mode
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
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

// Generate one terrain chunk
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

    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            float wx = (chunkX * width + x);
            float wy = (chunkY * height + y);
            map[y][x] = getSymbol(noise.GetNoise(wx, wy));
        }
}

// Patrolman entity
struct Patrol
{
    float x, y;    // position within current chunk
    float stamina; // seconds of movement remaining
    bool active;
};

int main()
{
    const int width = 80;
    const int height = 25;
    const float tileSize = 1.0f;
    const float walkSpeed = 1.5f;
    const float runSpeed = 5.0f;
    const float patrolSpeed = 1.2f;
    const float patrolStamina = 10.0f; // seconds of chase before stopping

    FastNoiseLite noise;
    noise.SetSeed(std::random_device{}());
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetFrequency(0.05f);

    std::vector<std::vector<char>> map(height, std::vector<char>(width));
    int chunkX = 0, chunkY = 0;
    generateChunk(map, noise, chunkX, chunkY);

    // Player
    float cx = width / 2.0f;
    float cy = height / 2.0f;

    std::vector<Patrol> patrols;
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> spawnDist(-10.0f, 10.0f);
    std::uniform_real_distribution<float> timeDist(5.0f, 12.0f);
    float nextSpawnTime = timeDist(rng);
    float spawnTimer = 0.0f;

    setRawMode(true);
    char key = 0;
    auto lastTime = std::chrono::steady_clock::now();

    while (key != 'q')
    {
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> deltaTime = now - lastTime;
        lastTime = now;
        float dt = deltaTime.count();

        // Input
        read(STDIN_FILENO, &key, 1);
        bool shift = (key == 'W' || key == 'A' || key == 'S' || key == 'D');
        float speed = shift ? runSpeed : walkSpeed;

        float dx = 0, dy = 0;
        if (key == 'w' || key == 'W')
            dy -= speed * dt / tileSize;
        if (key == 's' || key == 'S')
            dy += speed * dt / tileSize;
        if (key == 'a' || key == 'A')
            dx -= speed * dt / tileSize;
        if (key == 'd' || key == 'D')
            dx += speed * dt / tileSize;

        cx += dx;
        cy += dy;

        // Handle chunk transitions
        if (cx < 0)
        {
            chunkX--;
            cx += width;
            generateChunk(map, noise, chunkX, chunkY);
        }
        else if (cx >= width)
        {
            chunkX++;
            cx -= width;
            generateChunk(map, noise, chunkX, chunkY);
        }
        if (cy < 0)
        {
            chunkY--;
            cy += height;
            generateChunk(map, noise, chunkX, chunkY);
        }
        else if (cy >= height)
        {
            chunkY++;
            cy -= height;
            generateChunk(map, noise, chunkX, chunkY);
        }

        // Spawn patrolmen periodically near cursor
        spawnTimer += dt;
        if (spawnTimer >= nextSpawnTime)
        {
            spawnTimer = 0;
            nextSpawnTime = timeDist(rng);
            Patrol p;
            p.x = cx + spawnDist(rng);
            p.y = cy + spawnDist(rng);
            if (p.x < 0)
                p.x = 0;
            if (p.y < 0)
                p.y = 0;
            if (p.x > width - 1)
                p.x = width - 1;
            if (p.y > height - 1)
                p.y = height - 1;
            p.stamina = patrolStamina;
            p.active = true;
            patrols.push_back(p);
        }

        // Update patrolmen
        for (auto &p : patrols)
        {
            if (!p.active)
                continue;
            if (p.stamina <= 0.0f)
            {
                p.active = false;
                continue;
            }

            float vx = cx - p.x;
            float vy = cy - p.y;
            float len = std::sqrt(vx * vx + vy * vy);
            if (len > 0.001f)
            {
                vx /= len;
                vy /= len;
                p.x += vx * patrolSpeed * dt / tileSize;
                p.y += vy * patrolSpeed * dt / tileSize;
            }
            p.stamina -= dt;
        }

        // Render
        std::cout << "\033[H\033[J";
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                bool printed = false;
                for (auto &p : patrols)
                    if (p.active && (int)p.x == x && (int)p.y == y)
                    {
                        std::cout << 'P';
                        printed = true;
                        break;
                    }
                if (!printed)
                {
                    if ((int)cx == x && (int)cy == y)
                        std::cout << 'X';
                    else
                        std::cout << map[y][x];
                }
            }
            std::cout << '\n';
        }

        std::cout << "Chunk: (" << chunkX << "," << chunkY << ") | "
                  << "Patrols active: "
                  << std::count_if(patrols.begin(), patrols.end(), [](auto &p)
                                   { return p.active; })
                  << '\n';

        usleep(16000);
    }

    setRawMode(false);
    std::cout << "\033[H\033[J";
    return 0;
}
