/**
 *  OSM
 *  Copyright (C) 2019  Pavel Smokotnin

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
#ifndef AVERAGING_H
#define AVERAGING_H

#include <queue>
#include <algorithm>
#include <cmath>
#include <limits>
#include "complex.h"
#include "container/fifo.h"
#include "container/array.h"

template<typename T> class Averaging
{

private:
    typedef Container::Fifo<T> dataT;
    Container::array<dataT> m_data;

    Container::array<T> m_value;
    Container::array<unsigned int> m_collected;
    float m_gain;
    unsigned int m_size;
    unsigned int m_depth;

public:
    Averaging():
        m_data(),
        m_value(),
        m_collected(),
        m_gain(1.f),
        m_size(1),
        m_depth(1) {}

    void append(unsigned int i, const T &value);
    T value(unsigned int i);

    void setSize(unsigned int size)
    {
        m_size = size;
        m_data.resize(m_size);
        m_value.resize(m_size);
        m_collected.resize(m_size);

        reset();
    }
    unsigned int size() const
    {
        return m_size;
    }

    void setDepth(unsigned int depth)
    {
        m_depth = depth;
    }
    unsigned int depth() const
    {
        return m_depth;
    }

    float gain() const
    {
        return m_gain;
    }

    void reset()
    {
        m_value.fill(T(0));
        m_data.each([](auto * p) {
            p->reset();
        });
        m_collected.fill(0);
    }

protected:
    void checkDepth(unsigned int i);
};

/*
 * One-pole (exponential) and infinite running averaging per frequency bin.
 * Unlike Averaging, it has no history buffer: each new value is blended into
 * the stored one, so the amount of smoothing is set by a time constant instead
 * of a frame count.
 */
template<typename T> class TemporalAveraging
{

private:
    Container::array<T> m_value;
    Container::array<float> m_alpha;
    Container::array<float> m_weight;
    Container::array<unsigned int> m_count;
    bool m_infinite;
    unsigned int m_size;

public:
    TemporalAveraging():
        m_value(),
        m_alpha(),
        m_weight(),
        m_count(),
        m_infinite(false),
        m_size(0) {}

    void setSize(unsigned int size)
    {
        m_size = size;
        m_value.resize(m_size);
        m_alpha.resize(m_size);
        m_weight.resize(m_size);
        m_count.resize(m_size);
        m_alpha.fill(1.f);
        reset();
    }
    unsigned int size() const
    {
        return m_size;
    }

    void setInfinite(bool infinite)
    {
        m_infinite = infinite;
    }
    bool infinite() const
    {
        return m_infinite;
    }

    /*
     * per frame smoothing coefficient:
     * 1 - no averaging, values closer to 0 give more smoothing
     */
    void setAlpha(unsigned int i, float alpha)
    {
        m_alpha[i] = std::max(0.0001f, std::min(1.f, alpha));
    }
    void setAlpha(float alpha)
    {
        for (unsigned int i = 0; i < m_size; ++i) {
            setAlpha(i, alpha);
        }
    }

    //! coefficient of the one-pole averager for a time constant and a frame interval, both in seconds
    static float alphaFor(float timeConstant, float interval)
    {
        if (timeConstant <= 0.f) {
            return 1.f;
        }
        return 1.f - std::exp(-interval / timeConstant);
    }

    T append(unsigned int i, const T &value)
    {
        return append(i, value, 1.f);
    }

    /*
     * weight is in the range 0..1. A zero weight leaves the stored value
     * untouched after the first value, which lets callers suppress bins with
     * low coherence. The first value always seeds the bin.
     */
    T append(unsigned int i, const T &value, float weight)
    {
        if (!std::isfinite(weight)) {
            weight = 0.f;
        }
        weight = std::max(0.f, std::min(1.f, weight));

        // start from the first value instead of ramping up from zero
        if (m_count[i] == 0) {
            m_value[i] = value;
            m_count[i] = 1;
            m_weight[i] = 1.f;
            return m_value[i];
        }

        if (m_infinite) {
            const float totalWeight = m_weight[i] + weight;
            if (weight > 0.f && totalWeight > 0.f) {
                m_value[i] += (value - m_value[i]) * (weight / totalWeight);
                m_weight[i] = totalWeight;
            }
            if (m_count[i] < std::numeric_limits<unsigned int>::max()) {
                ++m_count[i];
            }
        } else if (weight > 0.f) {
            m_value[i] += (value - m_value[i]) * m_alpha[i] * weight;
        }
        return m_value[i];
    }

    T value(unsigned int i) const
    {
        return m_value[i];
    }

    void reset()
    {
        m_value.fill(T(0));
        m_weight.fill(0.f);
        m_count.fill(0);
    }
};

template<> void Averaging<unsigned int>::append(unsigned int i, const unsigned int &value);
template<> void Averaging<float>::append(unsigned int i, const float &value);
template<> void Averaging<Complex>::append(unsigned int i, const Complex &value);

template <> unsigned int Averaging<unsigned int>::value(unsigned int i);
template <> float Averaging<float>::value(unsigned int i);
template <> Complex Averaging<Complex>::value(unsigned int i);


#endif // AVERAGING_H
