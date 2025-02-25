#pragma once

#include "nlohmann/json.hpp"
#include "common/image/image.h"
#include "products/image_products.h"

namespace satdump
{
    // Reprojection interface. WIP
    namespace reprojection
    {
        // Re-Projection operation
        struct ReprojectionOperation
        {
            nlohmann::json source_prj_info;
            nlohmann::json target_prj_info;
            image::Image<uint16_t> img;
            int output_width, output_height;
            bool use_draw_algorithm;
            TLE img_tle;                 // Only if required
            std::vector<double> img_tim; // Only if required
        };

        struct ProjectionResult
        {
            nlohmann::json settings;
            image::Image<uint16_t> img;
        };

        ProjectionResult reproject(ReprojectionOperation &op, float *progress = nullptr);

        std::function<std::pair<int, int>(float, float, int, int)> setupProjectionFunction(int width, int height,
                                                                                           nlohmann::json params,
                                                                                           TLE tle = TLE(),
                                                                                           std::vector<double> timestamps = std::vector<double>(),
                                                                                           bool rotate = false);
    }
}