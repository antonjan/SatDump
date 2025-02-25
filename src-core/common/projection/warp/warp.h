#pragma once

#include "common/image/image.h"
#include "common/projection/thinplatespline.h"
#include <memory>

namespace satdump
{
    namespace warp
    {
        /*
        Image warping operation, taking an image and Ground Control Points.
        output_width and output_height do NOT define the *actual* output
        height / width. It defines the overall resolution assuming it was
        covering 360 degrees of longitude and 180 of latitude.
        */
        struct WarpOperation
        {
            image::Image<uint16_t> input_image;
            std::vector<projection::GCP> ground_control_points;
            int output_width;
            int output_height;
        };

        /*
        Result of an image warping operation, holding the image and resulting
        4 GCPs.
        Those GCPs allow referencing the resulting image to Lat / Lon using a
        much less compute-intensive affine transform.
        */
        struct WarpResult
        {
            image::Image<uint16_t> output_image;
            projection::GCP top_left;
            projection::GCP top_right;
            projection::GCP bottom_right;
            projection::GCP bottom_left;
        };

        /*
        Initialize the Thin Plate Spline transform we need for warping
        imagery.
        */
        std::unique_ptr<projection::VizGeorefSpline2D> initTPSTransform(WarpOperation &op);

        /*
        More or less internal structs that holds the required area to
        cover given input GCPs.
        */
        struct WarpCropSettings
        {
            float lat_min;
            float lat_max;
            float lon_min;
            float lon_max;
            int y_min;
            int y_max;
            int x_min;
            int x_max;
        };

        /*
        Returns the required "croped" area to cover provided GCPs.
        */
        WarpCropSettings choseCropArea(WarpOperation &op);

        /*
        Reproject a provided image with GCPs to an equirectangular projection,
        from where other projections can be done.

        A WarpOperation is the input. At least once, but otherwise each time
        GCPs or output size is changed, update() should be called to recompute
        the transformer. Doing this is more-or-less intensive, so it not
        automatically called as that would introduce potentially unwanted
        latencies.

        After update() was called, warp() can be called. That function
        will return a WarpResult containing the output image and resulting
        GCPs, as an equirectangular projection.
        */
        class ImageWarper
        {
        private:
            WarpCropSettings crop_set;
            std::unique_ptr<projection::VizGeorefSpline2D> tps;

        private:
            // Slower, generic CPU implementation
            void warpOnCPU(WarpResult &result);

#ifdef USE_OPENCL
            // 64-bits GPU implementation, more accurate and faster than CPU, but not all GPUs
            // have FP64 cores and if they do, usually quite few of them
            void warpOnGPU_fp64(WarpResult &result);

            // 32-bits GPU implementation, less accurate than FP64 but really, as long as
            // you are not re-projecting something super high resolution, no difference to
            // FP64. The big plus though is, most GPU especially on the consumer end and older
            // generations pack a LOT more FP32 cores (On recent Pascal GPUs, it's 24x more than
            // FP64!). This is the default, and gives quite impressive performances.
            void warpOnGPU_fp32(WarpResult &result);
#endif

        public:
            WarpOperation op;

        public:
            // Call this if the GCPs or height/width in the WarpOperation changed. Has to be called at least once.
            void update();

            // Warp and return result.
            // This will if possible try to use a graphics accelerator
            // (GPU, iGPU, or CPUs that have OpenCL support),
            // and if not available or in case of a failure, will use
            // a slower CPU function instead.
            WarpResult warp();
        };
    }
}