#include "image.h"
#include <cstring>
#include <limits>
#include <fstream>

namespace image
{
    template <typename T>
    void Image<T>::init(size_t width, size_t height, int channels)
    {
        // Reset image if we already had one
        if (has_data)
            delete[] d_data;

        // Init buffer
        data_size = width * height * channels;
        d_data = new T[data_size];

        // Set to 0
        memset(d_data, 0, sizeof(T) * data_size);

        // Init local variables
        d_depth = sizeof(T) * 8;
        d_width = width;
        d_height = height;
        d_channels = channels;

        // We have data now if we didn't already
        has_data = true;
    }

    template <typename T>
    void Image<T>::clear()
    {
        // Reset image
        if (has_data)
            delete[] d_data;
        has_data = false;
    }

    template <typename T>
    Image<T>::Image()
    {
        // Do nothing
    }

    template <typename T>
    Image<T>::Image(size_t width, size_t height, int channels)
    {
        init(width, height, channels);
    }

    template <typename T>
    Image<T>::Image(const Image &img)
    {
        // Copy contents of the image over
        init(img.d_width, img.d_height, img.d_channels);
        memcpy(d_data, img.d_data, img.data_size * sizeof(T));
    }

    template <typename T>
    Image<T>::Image(T *buffer, size_t width, size_t height, int channels)
    {
        // Copy contents of the image over
        init(width, height, channels);
        memcpy(d_data, buffer, data_size * sizeof(T));
    }

    template <typename T>
    Image<T> &Image<T>::operator=(const Image<T> &img)
    {
        // Copy contents of the image over
        init(img.d_width, img.d_height, img.d_channels);
        memcpy(d_data, img.d_data, img.data_size * sizeof(T));
        return *this;
    }

    template <typename T>
    Image<T> &Image<T>::operator<<=(const int &shift)
    {
        for (size_t i = 0; i < data_size; i++)
            d_data[i] <<= shift;
        return *this;
    }

    template <typename T>
    Image<T>::~Image()
    {
        if (has_data)
            delete[] d_data;
    }

    template <typename T>
    T Image<T>::clamp(int input)
    {
        if (input > std::numeric_limits<T>::max())
            return std::numeric_limits<T>::max();
        else if (input < 0)
            return 0;
        else
            return input;
    }

    template <typename T>
    void Image<T>::to_rgb()
    {
        if (d_channels == 1)
        {
            Image<T> tmp = *this;       // Backup image
            init(d_width, d_height, 3); // Init new image as RGB

            // Fill in all 3 channels
            draw_image(0, tmp);
            draw_image(1, tmp);
            draw_image(2, tmp);
        }
        else if (d_channels == 4)
        {
            Image<T> tmp = *this;       // Backup image
            init(d_width, d_height, 3); // Init new image as RGB

            // Copy over all 3 channels
            memcpy(d_data, tmp.data(), d_width * d_height * 3 * sizeof(T));
        }
    }

    template <typename T>
    Image<uint8_t> Image<T>::to8bits()
    {
        if (d_depth == 8)
        {
            return *((image::Image<uint8_t> *)this);
        }
        else if (d_depth == 16)
        {
            image::Image<uint8_t> image8(d_width, d_height, d_channels);
            for (size_t i = 0; i < data_size; i++)
                image8[i] = d_data[i] >> 8;
            return image8;
        }

        return Image<uint8_t>(); // This should never happen
    }

    template <typename T>
    Image<uint16_t> Image<T>::to16bits()
    {
        if (d_depth == 16)
        {
            return *((image::Image<uint16_t> *)this);
        }
        else if (d_depth == 8)
        {
            image::Image<uint16_t> image16(d_width, d_height, d_channels);
            for (size_t i = 0; i < data_size; i++)
                image16[i] = d_data[i] << 8;
            return image16;
        }

        return Image<uint16_t>(); // This should never happen
    }

    template <typename T>
    void Image<T>::load_img(std::string file)
    {
        std::ifstream file_sigature_src(file, std::ios::binary);
        uint8_t signature[10];
        file_sigature_src.read((char *)signature, 10);
        if (signature[0] == 0xFF && signature[1] == 0xD8)
            load_jpeg(file);
        else if (signature[0] == 0x89 && signature[1] == 0x50 && signature[2] == 0x4E && signature[3] == 0x47)
            load_png(file);
    }

    template <typename T>
    void Image<T>::load_img(uint8_t *buffer, int size)
    {
        if (buffer[0] == 0xFF && buffer[1] == 0xD8)
            load_jpeg(buffer, size);
        else if (buffer[0] == 0x89 && buffer[1] == 0x50 && buffer[2] == 0x4E && buffer[3] == 0x47)
            load_png(buffer, size);
    }

    // Generate Images for uint16_t and uint8_t
    template class Image<uint8_t>;
    template class Image<uint16_t>;
}