#include "reproj.h"
#include "../projs/equirectangular.h"
#include "../sat_proj/geo_projection.h"
#include "logger.h"
#include "resources.h"
#include "core/opencl.h"

namespace satdump
{
    namespace reproj
    {
#ifdef USE_OPENCL
        void reproject_geos_to_equ_GPU(image::Image<uint16_t> &source_img,
                                       double geos_lon,
                                       double geos_height,
                                       double geos_hscale,
                                       double geos_vscale,
                                       double geos_xoff,
                                       double geos_yoff,
                                       bool geos_sweepx,
                                       image::Image<uint16_t> &target_img,
                                       float equ_tl_lon, float equ_tl_lat,
                                       float equ_br_lon, float equ_br_lat,
                                       float *progress)
        {
            // Build GPU Kernel
            cl_program proj_program = opencl::buildCLKernel(resources::getResourcePath("opencl/reproj_image_geos_to_equ_fp32.cl"));

            cl_int err = 0;
            auto &context = satdump::opencl::ocl_context;
            auto &device = satdump::opencl::ocl_device;

            // Projs for later
            geodetic::projection::GEOProjector geo_proj(geos_lon, geos_height, source_img.width(), source_img.height(),
                                                        geos_hscale, geos_vscale, geos_xoff, geos_yoff, geos_sweepx);

            // Now, run the actual OpenCL Kernel
            auto gpu_start = std::chrono::system_clock::now();
            {
                // Images
                cl_mem buffer_src_img = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(uint16_t) * source_img.size(), NULL, &err);
                if (err != CL_SUCCESS)
                    throw std::runtime_error("Couldn't load buffer_map!");
                cl_mem buffer_trg_img = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(uint16_t) * target_img.size(), NULL, &err);
                if (err != CL_SUCCESS)
                    throw std::runtime_error("Couldn't load buffer_img!");

                // Settings Stuff
                cl_mem buffer_img_sizes = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int) * 5, NULL, &err);
                cl_mem buffer_geos_settings = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * 17, NULL, &err);
                cl_mem buffer_equ_settings = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * 4, NULL, &err);

                // IMG Sizes
                int img_sizes[5];
                img_sizes[0] = source_img.width();
                img_sizes[1] = source_img.height();
                img_sizes[2] = target_img.width();
                img_sizes[3] = target_img.height();
                img_sizes[4] = source_img.channels();

                // GEOS stuff
                float geo_settings[17];
                geo_proj.get_for_gpu_float(geo_settings);

                // Equirectangular stuff
                float equ_settings[4];
                equ_settings[0] = equ_tl_lat;
                equ_settings[1] = equ_tl_lon;
                equ_settings[2] = equ_br_lat;
                equ_settings[3] = equ_br_lon;

                // Create an OpenCL queue
                cl_command_queue queue = clCreateCommandQueue(context, device, 0, &err);

                // Load buffers
                clEnqueueWriteBuffer(queue, buffer_src_img, true, 0, sizeof(uint16_t) * source_img.size(), source_img.data(), 0, NULL, NULL);
                clEnqueueWriteBuffer(queue, buffer_trg_img, true, 0, sizeof(uint16_t) * target_img.size(), target_img.data(), 0, NULL, NULL);
                clEnqueueWriteBuffer(queue, buffer_img_sizes, true, 0, sizeof(int) * 5, img_sizes, 0, NULL, NULL);
                clEnqueueWriteBuffer(queue, buffer_geos_settings, true, 0, sizeof(float) * 17, geo_settings, 0, NULL, NULL);
                clEnqueueWriteBuffer(queue, buffer_equ_settings, true, 0, sizeof(float) * 4, equ_settings, 0, NULL, NULL);

                // Init the kernel
                cl_kernel proj_kernel = clCreateKernel(proj_program, "reproj_image_geos_to_equ", &err);
                clSetKernelArg(proj_kernel, 0, sizeof(cl_mem), &buffer_src_img);
                clSetKernelArg(proj_kernel, 1, sizeof(cl_mem), &buffer_trg_img);
                clSetKernelArg(proj_kernel, 2, sizeof(cl_mem), &buffer_img_sizes);
                clSetKernelArg(proj_kernel, 3, sizeof(cl_mem), &buffer_geos_settings);
                clSetKernelArg(proj_kernel, 4, sizeof(cl_mem), &buffer_equ_settings);

                // Get proper workload size
                size_t size_wg = 0;
                size_t compute_units = 0;
                clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), &size_wg, NULL);
                clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(size_t), &compute_units, NULL);

                logger->debug("Workgroup size {:d}", size_wg * compute_units);

                // Run the kernel!
                size_t total_wg_size = int(size_wg) * int(compute_units);
                if (clEnqueueNDRangeKernel(queue, proj_kernel, 1, NULL, &total_wg_size, NULL, 0, NULL, NULL) != CL_SUCCESS)
                    throw std::runtime_error("Couldn't clEnqueueNDRangeKernel!");

                // Read image result back from VRAM
                clEnqueueReadBuffer(queue, buffer_trg_img, true, 0, sizeof(uint16_t) * target_img.size(), target_img.data(), 0, NULL, NULL);

                // Free up everything
                clReleaseMemObject(buffer_src_img);
                clReleaseMemObject(buffer_trg_img);
                clReleaseMemObject(buffer_img_sizes);
                clReleaseMemObject(buffer_geos_settings);
                clReleaseMemObject(buffer_equ_settings);
                clReleaseKernel(proj_kernel);
                clReleaseProgram(proj_program);
                clReleaseCommandQueue(queue);
            }
            auto gpu_time = (std::chrono::system_clock::now() - gpu_start);
            logger->debug("GPU Processing Time {:f}", gpu_time.count() / 1e9);

            if (progress != nullptr)
                *progress = 1;
        }
#endif

        void reproject_geos_to_equ_CPU(image::Image<uint16_t> &source_img,
                                       double geos_lon,
                                       double geos_height,
                                       double geos_hscale,
                                       double geos_vscale,
                                       double geos_xoff,
                                       double geos_yoff,
                                       bool geos_sweepx,
                                       image::Image<uint16_t> &target_img,
                                       float equ_tl_lon, float equ_tl_lat,
                                       float equ_br_lon, float equ_br_lat,
                                       float *progress)
        {
            geodetic::projection::GEOProjector geo_proj(geos_lon, geos_height, source_img.width(), source_img.height(),
                                                        geos_hscale, geos_vscale, geos_xoff, geos_yoff, geos_sweepx);
            geodetic::projection::EquirectangularProjection equi_proj;
            equi_proj.init(target_img.width(), target_img.height(), equ_tl_lon, equ_tl_lat, equ_br_lon, equ_br_lat);

            float lon, lat;
            int x2, y2;
            for (int x = 0; x < (int)target_img.width(); x++)
            {
                for (int y = 0; y < (int)target_img.height(); y++)
                {
                    equi_proj.reverse(x, y, lon, lat);
                    if (lon == -1 || lat == -1)
                        continue;

                    geo_proj.forward(lon, lat, x2, y2);
                    if (x2 == -1 || y2 == -1 ||
                        x2 >= (int)source_img.width() || x2 < 0 ||
                        y2 >= (int)source_img.height() || y2 < 0)
                        continue;

                    if (source_img.channels() == 3)
                        for (int c = 0; c < 3; c++)
                            target_img.channel(c)[y * target_img.width() + x] = source_img.channel(c)[y2 * source_img.width() + x2];
                    else
                        for (int c = 0; c < 3; c++)
                            target_img.channel(c)[y * target_img.width() + x] = source_img.channel(0)[y2 * source_img.width() + x2];
                }

                if (progress != nullptr)
                    *progress = float(x) / float(target_img.height());
            }
        }

        void reproject_geos_to_equ(image::Image<uint16_t> &source_img,
                                   double geos_lon,
                                   double geos_height,
                                   double geos_hscale,
                                   double geos_vscale,
                                   double geos_xoff,
                                   double geos_yoff,
                                   bool geos_sweepx,
                                   image::Image<uint16_t> &target_img,
                                   float equ_tl_lon, float equ_tl_lat,
                                   float equ_br_lon, float equ_br_lat,
                                   float *progress)
        {
#ifdef USE_OPENCL
            try
            {
                logger->info("GEOS to Equ projection on GPU...");
                satdump::opencl::setupOCLContext();
                reproject_geos_to_equ_GPU(source_img,
                                          geos_lon,
                                          geos_height,
                                          geos_hscale,
                                          geos_vscale,
                                          geos_xoff,
                                          geos_yoff,
                                          geos_sweepx,
                                          target_img,
                                          equ_tl_lon, equ_tl_lat,
                                          equ_br_lon, equ_br_lat,
                                          progress);
                return;
            }
            catch (std::runtime_error &e)
            {
                logger->error("Error stereo reproj on GPU : {:s}", e.what());
            }
#endif

            logger->info("GEOS to Equ projection on CPU...");
            reproject_geos_to_equ_CPU(source_img,
                                      geos_lon,
                                      geos_height,
                                      geos_hscale,
                                      geos_vscale,
                                      geos_xoff,
                                      geos_yoff,
                                      geos_sweepx,
                                      target_img,
                                      equ_tl_lon, equ_tl_lat,
                                      equ_br_lon, equ_br_lat,
                                      progress);
        }
    }
}