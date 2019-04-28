#ifndef WVLX_UTIL_SOUND_MATH_H
#define WVLX_UTIL_SOUND_MATH_H
#include <cmath>

constexpr double pi = 3.14159265358979323846264338327950288;
constexpr double halfpi = 0.5 * pi;
constexpr double tau = 2.0 * pi;

namespace sound {
double Square(double theta, double duty=0.5) {
    theta = fmod(theta, tau);
    return (theta < tau*duty) ? -1.0 : 1.0;
}

double Saw(double theta) {
    theta = fmod(theta, tau);
    return -1.0 + theta / pi;
}

double Triangle(double theta) {
    theta = fmod(theta, tau);
    if (theta < pi) {
        return -1.0 + theta / halfpi;
    } else {
        return 1.0 - (theta-pi) / halfpi;
    }
}

}  // namespace sound
#endif // WVLX_UTIL_SOUND_MATH_H
