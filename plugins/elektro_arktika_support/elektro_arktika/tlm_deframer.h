#pragma once

/*
    A CADU Deframer
*/

#include <vector>
#include <array>
#include <cstdint>

namespace elektro_arktika
{
    /*
    CCSDS Values to use throughout the whole program
    */

    const int CADU_SIZE = 224;
    const int CADU_ASM_SIZE = 4;
    const uint32_t CADU_ASM = 0x1ACFFC1D;
    const uint32_t CADU_ASM_INV = 0xE53003E2;
    const uint8_t CADU_ASM_1 = 0x1A;
    const uint8_t CADU_ASM_2 = 0xCF;
    const uint8_t CADU_ASM_3 = 0xFC;
    const uint8_t CADU_ASM_4 = 0x1D;

    class CADUDeframer
    {
    private:
        // Main shifter used to locate sync words
        uint32_t shifter;
        // Bit inversion?
        bool bit_inversion;
        // Sync machine state
        int state;
        // Write a frame?
        bool writeFrame;
        // Values used to output a found frame
        int wroteBits, wroteBytes;
        uint8_t outBuffer;
        int skip, errors, good, sep_errors, state_2_bits_count;
        // Output Frame buffer
        std::array<uint8_t, CADU_SIZE> frameBuffer;
        // Found frame count
        int numFrames;

    public:
        CADUDeframer();
        // Get found frame count
        int getFrameCount();
        // Return state
        int getState();
        // Perform deframing
        std::vector<std::array<uint8_t, CADU_SIZE>> work(uint8_t *input, int size);
    };
} // namespace elektro_arktika