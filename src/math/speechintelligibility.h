/**
 *  OSM
 *  Copyright (C) 2026  Pavel Smokotnin

 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef MATH_SPEECHINTELLIGIBILITY_H
#define MATH_SPEECHINTELLIGIBILITY_H

#include <array>
#include <vector>
#include <QString>

namespace Speech {

class Calculator
{
public:
    static constexpr int BandCount = 7;

    struct Result {
        bool valid = false;
        bool shortImpulse = false;
        float sampleRate = 0.f;
        float duration = 0.f;
        float sti = 0.f;
        float stipa = 0.f;
        float rasti = 0.f;
        std::array<float, BandCount> mti{};
        QString error;
    };

    // Calculates the indirect STI values from an impulse response. Times are
    // in milliseconds, matching Abstract::Data::impulseTime().
    static Result fromImpulse(const std::vector<float> &values,
                              const std::vector<float> &times,
                              unsigned sampleRate = 0);

    static QString rating(float value);
};

} // namespace Speech

#endif // MATH_SPEECHINTELLIGIBILITY_H
