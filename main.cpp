#include <iostream>
#include <random>
#include <vector>
#include <algorithm>
#include <termios.h>
#include <unistd.h>
#include <chrono>
#include <cmath>
#include <fcntl.h>
#include "FastNoiseLite.h"

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
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }
    else
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
    }
}

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

struct Patrol
{
    float wx, wy;
    float stamina;
    bool active;
};

int main()
{
    const int viewW = 80;
    const int viewH = 25;
    const float walkSpeed = 1.5f;
    const float runSpeed = 5.0f;
    const float patrolSpeed = 4.2f;
    const float patrolStamina = 20.0f;

    FastNoiseLite noise;
    noise.SetSeed(std::random_device{}());
    noise.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    noise.SetFrequency(0.05f);

    float cx = 0.0f, cy = 0.0f;

    std::vector<Patrol> patrols;
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> spawnDist(-15.0f, 15.0f);
    std::uniform_real_distribution<float> timeDist(5.0f, 12.0f);
    float nextSpawnTime = timeDist(rng);
    float spawnTimer = 0.0f;

    // key hold timestamps
    using clock = std::chrono::steady_clock;
    clock::time_point t_up{}, t_down{}, t_left{}, t_right{}, t_run{};
    const float keyTimeout = 0.16f; // seconds: consider key held while recent

    setRawMode(true);
    auto lastTime = clock::now();

    while (true)
    {
        auto now = clock::now();
        std::chrono::duration<float> deltaTime = now - lastTime;
        lastTime = now;
        float dt = deltaTime.count();

        // Read all available key presses and update timestamps
        char ch;
        while (read(STDIN_FILENO, &ch, 1) > 0)
        {
            if (ch == 'q')
            {
                setRawMode(false);
                std::cout << "\033[H\033[J";
                return 0;
            }
            // movement keys (lowercase)
            if (ch == 'w')
                t_up = now;
            if (ch == 's')
                t_down = now;
            if (ch == 'a')
                t_left = now;
            if (ch == 'd')
                t_right = now;
            // uppercase indicates Shift held for that event
            if (ch == 'W')
            {
                t_up = now;
                t_run = now;
            }
            if (ch == 'S')
            {
                t_down = now;
                t_run = now;
            }
            if (ch == 'A')
            {
                t_left = now;
                t_run = now;
            }
            if (ch == 'D')
            {
                t_right = now;
                t_run = now;
            }
            // ignore other chars
        }

        // Determine which keys are currently considered held
        auto held = [&](const clock::time_point &t) -> bool
        {
            if (t == clock::time_point{})
                return false;
            std::chrono::duration<float> age = now - t;
            return age.count() < keyTimeout;
        };

        bool up = held(t_up);
        bool down = held(t_down);
        bool left = held(t_left);
        bool right = held(t_right);
        bool run = held(t_run);

        // Movement vector
        float dx = 0.0f, dy = 0.0f;
        float speed = run ? runSpeed : walkSpeed;

        if (up)
            dy -= speed * dt;
        if (down)
            dy += speed * dt;
        if (left)
            dx -= speed * dt;
        if (right)
            dx += speed * dt;

        cx += dx;
        cy += dy;

        // spawn patrols
        spawnTimer += dt;
        if (spawnTimer >= nextSpawnTime)
        {
            spawnTimer = 0;
            nextSpawnTime = timeDist(rng);
            Patrol p{cx + spawnDist(rng), cy + spawnDist(rng), patrolStamina, true};
            patrols.push_back(p);
        }

        // update patrols
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

        float camX = cx - viewW / 2.0f;
        float camY = cy - viewH / 2.0f;

        std::cout << "\033[H\033[J";
        for (int y = 0; y < viewH; ++y)
        {
            for (int x = 0; x < viewW; ++x)
            {
                float wx = camX + x;
                float wy = camY + y;
                char c = getSymbol(noise.GetNoise(wx, wy));

                bool printed = false;
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
                  << "Active patrols: " << activeCount << "  "
                  << "Run: " << (run ? "YES" : "no") << "\n";

        usleep(16000); // ~60 FPS
    }

    // unreachable
    setRawMode(false);
    return 0;
}
