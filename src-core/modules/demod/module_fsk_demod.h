#pragma once

#include "module_demod_base.h"
#include "common/dsp/random.h"
#include "common/dsp/fir.h"
#include "common/dsp/quadrature_demod.h"
#include "common/dsp/clock_recovery_mm.h"

namespace demod
{
    class FSKDemodModule : public BaseDemodModule
    {
    protected:
        std::shared_ptr<dsp::FIRBlock<complex_t>> rrc;
        std::shared_ptr<dsp::QuadratureDemodBlock> qua;
        std::shared_ptr<dsp::MMClockRecoveryBlock<float>> rec;

        float d_rrc_alpha;
        int d_rrc_taps = 31;

        float d_clock_gain_omega = pow(1.7e-2, 2) / 4.0;
        float d_clock_mu = 0.5f;
        float d_clock_gain_mu = 1.7e-2;
        float d_clock_omega_relative_limit = 0.005f;

        int8_t *sym_buffer;

    public:
        FSKDemodModule(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
        ~FSKDemodModule();
        void init();
        void stop();
        void process();

    public:
        static std::string getID();
        virtual std::string getIDM() { return getID(); };
        static std::vector<std::string> getParameters();
        static std::shared_ptr<ProcessingModule> getInstance(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
    };
}