#include <iostream>
#include "FastNoiseLite.h"

int main()
{
    const int width = 80;
    const int height = 25;

    FastNoiseLite noise;
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

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            float v = noise.GetNoise((float)x, (float)y);
            std::cout << getSymbol(v);
        }
        std::cout << '\n';
    }

    return 0;
}
