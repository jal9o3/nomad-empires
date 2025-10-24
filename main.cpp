#include <iostream>
#include <random>
#include <vector>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <chrono>
#include <cmath>
#include "FastNoiseLite.h"

// Enable/disable raw terminal input
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

// Noise → ASCII terrain symbol
char getSymbol(float v)
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
}

// Patrol entity
struct Patrol
{
    float wx, wy;  // world coordinates
    float stamina; // seconds remaining
    bool active;
};

int main()
{
    const int viewW = 80; // visible width in tiles
    const int viewH = 25; // visible height in tiles
    const float tileSize = 1.0f;
    const float walkSpeed = 1.5f;
    const float runSpeed = 5.0f;
    const float patrolSpeed = 4.2f;    // your updated value
    const float patrolStamina = 20.0f; // your updated value

    // Noise generator
    FastNoiseLite noise;
    noise.SetSeed(std::random_device{}());
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetFrequency(0.05f);

    // Player position (world coordinates)
    float cx = 0.0f;
    float cy = 0.0f;

    // Patrol management
    std::vector<Patrol> patrols;
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> spawnDist(-15.0f, 15.0f);
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
            dy -= speed * dt;
        if (key == 's' || key == 'S')
            dy += speed * dt;
        if (key == 'a' || key == 'A')
            dx -= speed * dt;
        if (key == 'd' || key == 'D')
            dx += speed * dt;

        cx += dx;
        cy += dy;

        // Spawn patrols around player periodically
        spawnTimer += dt;
        if (spawnTimer >= nextSpawnTime)
        {
            spawnTimer = 0;
            nextSpawnTime = timeDist(rng);
            Patrol p;
            p.wx = cx + spawnDist(rng);
            p.wy = cy + spawnDist(rng);
            p.stamina = patrolStamina;
            p.active = true;
            patrols.push_back(p);
        }

        // Update patrols
        for (auto &p : patrols)
        {
            if (!p.active)
                continue;
            if (p.stamina <= 0.0f)
            {
                p.active = false;
                continue;
            }

            float vx = cx - p.wx;
            float vy = cy - p.wy;
            float len = std::sqrt(vx * vx + vy * vy);
            if (len > 0.001f)
            {
                vx /= len;
                vy /= len;
                p.wx += vx * patrolSpeed * dt;
                p.wy += vy * patrolSpeed * dt;
            }
            p.stamina -= dt;
        }

        // Determine camera center
        float camX = cx - viewW / 2.0f;
        float camY = cy - viewH / 2.0f;

        // Render viewport
        std::cout << "\033[H\033[J";
        for (int y = 0; y < viewH; ++y)
        {
            for (int x = 0; x < viewW; ++x)
            {
                float wx = camX + x;
                float wy = camY + y;
                char c = getSymbol(noise.GetNoise(wx, wy));

                bool printed = false;
                // Patrol render
                for (auto &p : patrols)
                {
                    if (!p.active)
                        continue;
                    int px = (int)std::floor(p.wx);
                    int py = (int)std::floor(p.wy);
                    if (px == (int)std::floor(wx) && py == (int)std::floor(wy))
                    {
                        std::cout << 'P';
                        printed = true;
                        break;
                    }
                }

                // Player render
                if (!printed)
                {
                    int px = (int)std::floor(cx);
                    int py = (int)std::floor(cy);
                    if (px == (int)std::floor(wx) && py == (int)std::floor(wy))
                        std::cout << 'X';
                    else
                        std::cout << c;
                }
            }
            std::cout << '\n';
        }

        int activeCount = std::count_if(patrols.begin(), patrols.end(),
                                        [](auto &p)
                                        { return p.active; });

        std::cout << "Pos: (" << cx << ", " << cy << ")  "
                  << "Active patrols: " << activeCount << "\n";

        usleep(16000); // ~60 FPS
    }

    setRawMode(false);
    std::cout << "\033[H\033[J";
    return 0;
}
