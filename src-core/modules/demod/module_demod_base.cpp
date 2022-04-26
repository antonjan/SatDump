#include "module_demod_base.h"
#include "logger.h"
#include "imgui/imgui.h"

namespace demod
{
    BaseDemodModule::BaseDemodModule(std::string input_file, std::string output_file_hint, nlohmann::json parameters) : ProcessingModule(input_file, output_file_hint, parameters),
                                                                                                                        constellation(100.0f / 127.0f, 100.0f / 127.0f, demod_constellation_size)
    {
        // Parameters parsing
        if (parameters.count("buffer_size") > 0)
            d_buffer_size = parameters["buffer_size"].get<long>();

        if (parameters.count("samplerate") > 0)
            d_samplerate = parameters["samplerate"].get<long>();
        else
            throw std::runtime_error("Samplerate parameter must be present!");

        if (parameters.count("symbolrate") > 0)
            d_symbolrate = parameters["symbolrate"].get<long>();

        if (parameters.count("agc_rate") > 0)
            d_agc_rate = parameters["agc_rate"].get<float>();

        if (parameters.count("dc_block") > 0)
            d_dc_block = parameters["dc_block"].get<bool>();

        if (parameters.count("iq_swap") > 0)
            d_iq_swap = parameters["iq_swap"].get<bool>();

        snr = 0;
        peak_snr = 0;
    }

    void BaseDemodModule::init()
    {
        float input_sps = (float)d_samplerate / (float)d_symbolrate;                                  // Compute input SPS
        resample = input_sps > MAX_SPS || input_sps < MIN_SPS;                                        // If SPS is out of allowed range, we resample
        int range = pow(10, (std::to_string(int(d_symbolrate)).size() - 1));                          // Avoid complex resampling
        final_samplerate = resample ? (round(d_symbolrate / range) * range) * MAX_SPS : d_samplerate; // Get the final samplerate we'll be working with
        float decimation_factor = d_samplerate / final_samplerate;                                    // Decimation factor to rescale our input buffer

        if (resample)
            d_buffer_size *= round(decimation_factor);

        final_sps = final_samplerate / (float)d_symbolrate;

        logger->debug("Input SPS : {:f}", input_sps);
        logger->debug("Resample : " + std::to_string(resample));
        logger->debug("Samplerate : {:f}", final_samplerate);
        logger->debug("Dec factor : {:f}", decimation_factor);
        logger->debug("Final SPS : {:f}", final_sps);

        // Init DSP Blocks
        if (input_data_type == DATA_FILE)
            file_source = std::make_shared<dsp::FileSourceBlock>(d_input_file, dsp::BasebandTypeFromString(d_parameters["baseband_format"]), d_buffer_size, d_iq_swap);

        if (d_dc_block)
            dc_blocker = std::make_shared<dsp::CorrectIQBlock>(input_data_type == DATA_DSP_STREAM ? input_stream : file_source->output_stream);

        // Cleanup things a bit
        std::shared_ptr<dsp::stream<complex_t>> input_data = d_dc_block ? dc_blocker->output_stream : (input_data_type == DATA_DSP_STREAM ? input_stream : file_source->output_stream);

        fft_splitter = std::make_shared<dsp::SplitterBlock>(input_data);
        fft_splitter->set_output_2nd(true);

        fft_proc = std::make_shared<dsp::FFTBlock>(fft_splitter->output_stream_2);
        fft_proc->set_fft_settings(8192);
        fft_plot = std::make_shared<widgets::FFTPlot>(fft_proc->output_stream->writeBuf, 8192, -10, 20);

        // Init resampler if required
        if (resample)
            resampler = std::make_shared<dsp::CCRationalResamplerBlock>(fft_splitter->output_stream, final_samplerate, d_samplerate);

        // AGC
        agc = std::make_shared<dsp::AGCBlock>(resample ? resampler->output_stream : fft_splitter->output_stream, d_agc_rate, 1.0f, 1.0f, 65536);
    }

    std::vector<ModuleDataType> BaseDemodModule::getInputTypes()
    {
        return {DATA_FILE, DATA_DSP_STREAM};
    }

    std::vector<ModuleDataType> BaseDemodModule::getOutputTypes()
    {
        return {DATA_FILE, DATA_STREAM};
    }

    BaseDemodModule::~BaseDemodModule()
    {
    }

    void BaseDemodModule::start()
    {
        // Start
        if (input_data_type == DATA_FILE)
            file_source->start();
        if (d_dc_block)
            dc_blocker->start();
        fft_splitter->start();
        fft_proc->start();
        if (resample)
            resampler->start();
        agc->start();
    }

    void BaseDemodModule::stop()
    {
        // Stop
        if (input_data_type == DATA_FILE)
            file_source->stop();
        if (d_dc_block)
            dc_blocker->stop();
        fft_splitter->stop();
        fft_proc->stop();
        if (resample)
            resampler->stop();
        agc->stop();
    }

    void BaseDemodModule::drawUI(bool window)
    {
        ImGui::Begin(name.c_str(), NULL, window ? NULL : NOWINDOW_FLAGS);

        ImGui::BeginGroup();
        constellation.draw(); // Constellation
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        {
            // Show SNR information
            ImGui::Button("Signal", {200 * ui_scale, 20 * ui_scale});
            if (show_freq)
            {
                ImGui::Text("Freq : ");
                ImGui::SameLine();
                ImGui::TextColored(IMCOLOR_SYNCING, "%.0f Hz", display_freq);
            }
            snr_plot.draw(snr, peak_snr);
            if (ImGui::Checkbox("Show FFT", &show_fft) && !streamingInput)
                fft_splitter->set_output_2nd(show_fft);
        }
        ImGui::EndGroup();

        if (!streamingInput)
            ImGui::ProgressBar((float)progress / (float)filesize, ImVec2(ImGui::GetWindowWidth() - 10, 20 * ui_scale));

        ImGui::End();

        drawFFT();
    }

    void BaseDemodModule::drawFFT()
    {
        if (show_fft && !streamingInput)
        {
            ImGui::SetNextWindowSize({400 * ui_scale, 200 * ui_scale});
            ImGui::Begin("Baseband FFT");
            fft_plot->draw({ImGui::GetWindowSize().x - 0, ImGui::GetWindowSize().y - 40 * ui_scale});
            float min = 1000;
            for (int i = 0; i < 8192; i++)
                if (fft_proc->output_stream->writeBuf[i] < min)
                    min = fft_proc->output_stream->writeBuf[i];
            float max = -1000;
            for (int i = 0; i < 8192; i++)
                if (fft_proc->output_stream->writeBuf[i] > max)
                    max = fft_proc->output_stream->writeBuf[i];

            fft_plot->scale_min = fft_plot->scale_min * 0.99 + min * 0.01;
            fft_plot->scale_max = fft_plot->scale_max * 0.99 + max * 0.01;

            ImGui::End();
        }
    }

    std::vector<std::string> BaseDemodModule::getParameters()
    {
        return {"samplerate", "symbolrate", "agc_rate", "iq_swap", "buffer_size", "dc_block", "baseband_format"};
    }
}