#pragma once

#include "products.h"
#include "common/image/image.h"

namespace satdump
{
    class RadiationProducts : public Products
    {
    public:
        std::vector<std::vector<int>> channel_counts;
        bool has_timestamps = true;

        void set_timestamps(int channel, std::vector<double> timestamps)
        {
            contents["timestamps"][channel] = timestamps;
        }

        void set_timestamps_all(std::vector<double> timestamps)
        {
            contents["timestamps"] = timestamps;
        }

        std::vector<double> get_timestamps(int channel)
        {
            std::vector<double> timestamps;
            try
            {
                timestamps = contents["timestamps"][channel].get<std::vector<double>>();
            }
            catch (std::exception &e)
            {
                timestamps = contents["timestamps"].get<std::vector<double>>();
            }

            return timestamps;
        }

        void set_energy(int channel, double energy)
        {
            contents["energy"][channel] = energy;
        }

        double get_energy(int channel)
        {
            if (contents.contains("energy"))
                return contents["energy"][channel].get<double>();
            else
                return 0;
        }

    public:
        virtual void save(std::string directory);
        virtual void load(std::string file);
    };

    // Map handling
    struct RadiationMapCfg
    {
        int channel;
        int radius;
        int min;
        int max;
    };

    inline void to_json(nlohmann::json &j, const RadiationMapCfg &v)
    {
        j["channel"] = v.channel;
        j["radius"] = v.radius;
        j["min"] = v.min;
        j["max"] = v.max;
    }

    inline void from_json(const nlohmann::json &j, RadiationMapCfg &v)
    {
        v.channel = j["channel"].get<int>();
        v.radius = j["radius"].get<int>();
        v.min = j["min"].get<int>();
        v.max = j["max"].get<int>();
    }

    image::Image<uint16_t> make_radiation_map(RadiationProducts &products, RadiationMapCfg cfg, float *progress = nullptr);
}