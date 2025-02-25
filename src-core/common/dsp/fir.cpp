#include "fir.h"
#include <cstring>
#include <volk/volk.h>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846 /* pi */
#endif

namespace dsp
{
    template <typename T>
    FIRBlock<T>::FIRBlock(std::shared_ptr<dsp::stream<T>> input, std::vector<float> taps) : Block<T, T>(input)
    {
        // Get alignement parameters
        align = volk_get_alignment();
        aligned_tap_count = std::max<int>(1, align / sizeof(T));

        // Set taps
        ntaps = taps.size();

        // Init taps
        this->taps = (float **)volk_malloc(aligned_tap_count * sizeof(float *), align);
        for (int i = 0; i < aligned_tap_count; i++)
        {
            this->taps[i] = (float *)volk_malloc((ntaps + aligned_tap_count - 1) * sizeof(float), align);
            memset(this->taps[i], 0, ntaps + aligned_tap_count - 1);
            for (int j = 0; j < ntaps; j++)
                this->taps[i][i + j] = taps[(ntaps - 1) - j]; // Reverse taps
        }

        // Init history buffer
        int size_hist = 2 * STREAM_BUFFER_SIZE;
        history = (T *)volk_malloc(size_hist * sizeof(T), align);
        std::fill(history, &history[size_hist], 0);
    }

    template <typename T>
    FIRBlock<T>::~FIRBlock()
    {
        for (int i = 0; i < aligned_tap_count; i++)
            volk_free(taps[i]);
        volk_free(taps);
        volk_free(history);
    }

    template <typename T>
    void FIRBlock<T>::work()
    {
        int nsamples = Block<T, T>::input_stream->read();
        if (nsamples <= 0)
        {
            Block<T, T>::input_stream->flush();
            return;
        }

        memcpy(&history[ntaps], Block<T, T>::input_stream->readBuf, nsamples * sizeof(T));
        Block<T, T>::input_stream->flush();

        if constexpr (std::is_same_v<T, float>)
        {
            for (int i = 0; i < nsamples; i++)
            {
                // Doing it this way instead of the normal :
                // volk_32f_x2_dot_prod_32f(&output_stream->writeBuf[i], &history[i + 1], taps, ntaps);
                // turns out to enhance performances quite a bit... Initially tested this after noticing
                // inconsistencies between the above simple way and GNU Radio's implementation
                const float *ar = (float *)((size_t)&history[i + 1] & ~(align - 1));
                const unsigned al = &history[i + 1] - ar;
                volk_32f_x2_dot_prod_32f_a(&Block<T, T>::output_stream->writeBuf[i], ar, taps[al], ntaps + al);
            }
        }
        if constexpr (std::is_same_v<T, complex_t>)
        {
            for (int i = 0; i < nsamples; i++)
            {
                // Doing it this way instead of the normal :
                // volk_32fc_32f_dot_prod_32fc(&output_stream->writeBuf[i], &history[i + 1], taps, ntaps);
                // turns out to enhance performances quite a bit... Initially tested this after noticing
                // inconsistencies between the above simple way and GNU Radio's implementation
                const complex_t *ar = (complex_t *)((size_t)&history[i + 1] & ~(align - 1));
                const unsigned al = &history[i + 1] - ar;
                volk_32fc_32f_dot_prod_32fc_a((lv_32fc_t *)&Block<T, T>::output_stream->writeBuf[i], (lv_32fc_t *)ar, taps[al], ntaps + al);
            }
        }

        Block<T, T>::output_stream->swap(nsamples);

        memmove(&history[0], &history[nsamples], ntaps * sizeof(T));
    }

    template class FIRBlock<complex_t>;
    template class FIRBlock<float>;
}