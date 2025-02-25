#pragma once

#include "block.h"

/*
Quadrature demodulator
*/
namespace dsp
{
    class QuadratureDemodBlock : public Block<complex_t, float>
    {
    private:
        float gain;
        complex_t *buffer;
        void work();

    public:
        QuadratureDemodBlock(std::shared_ptr<dsp::stream<complex_t>> input, float gain);
        ~QuadratureDemodBlock();
    };
}