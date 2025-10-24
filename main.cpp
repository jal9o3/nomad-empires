#include <iostream>
#include <random>
#include <vector>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <chrono>
#include <cmath>
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
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

// Get terrain symbol based on noise value
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
    float wx, wy;  // world coordinates (meters)
    float stamina; // seconds left to chase
    bool active;
};

int main()
{
    const int chunkW = 80;
    const int chunkH = 25;
    const float tileSize = 1.0f;
    const float walkSpeed = 1.5f;
    const float runSpeed = 5.0f;
    const float patrolSpeed = 4.2f;
    const float patrolStamina = 20.0f;

    FastNoiseLite noise;
    noise.SetSeed(std::random_device{}());
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetFrequency(0.05f);

    // Player world position
    float cx = 0.0f;
    float cy = 0.0f;

    // Patrol management
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

        // Read key input
        read(STDIN_FILENO, &key, 1);
        bool shift = (key == 'W' || key == 'A' || key == 'S' || key == 'D');
        float speed = shift ? runSpeed : walkSpeed;

        // Player motion
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

        // Spawn patrols periodically
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

        // Determine visible chunk coordinates
        int chunkX = std::floor(cx / chunkW);
        int chunkY = std::floor(cy / chunkH);

        // Render visible area
        std::cout << "\033[H\033[J";
        for (int y = 0; y < chunkH; ++y)
        {
            for (int x = 0; x < chunkW; ++x)
            {
                float wx = chunkX * chunkW + x;
                float wy = chunkY * chunkH + y;
                char c = getSymbol(noise.GetNoise(wx, wy));

                // Patrol rendering
                bool printed = false;
                for (auto &p : patrols)
                {
                    if (p.active)
                    {
                        int px = (int)std::floor(p.wx);
                        int py = (int)std::floor(p.wy);
                        if (px == (int)wx && py == (int)wy)
                        {
                            std::cout << 'P';
                            printed = true;
                            break;
                        }
                    }
                }

                if (!printed)
                {
                    int px = (int)std::floor(cx);
                    int py = (int)std::floor(cy);
                    if (px == (int)wx && py == (int)wy)
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

        std::cout << "World pos: (" << cx << ", " << cy << ") | "
                  << "Chunk: (" << chunkX << ", " << chunkY << ") | "
                  << "Active patrols: " << activeCount << "\n";

        usleep(16000); // ~60 FPS
    }

    setRawMode(false);
    std::cout << "\033[H\033[J";
    return 0;
}
