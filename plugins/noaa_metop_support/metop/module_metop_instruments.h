#pragma once

#include "core/module.h"
#include "instruments/avhrr/avhrr_reader.h"
#include "../instruments/mhs/mhs_reader.h"
#include "instruments/ascat/ascat_reader.h"
#include "instruments/iasi/iasi_reader.h"
#include "instruments/iasi/iasi_imaging_reader.h"
#include "instruments/amsu/amsu_a1_reader.h"
#include "instruments/amsu/amsu_a2_reader.h"
#include "instruments/gome/gome_reader.h"
#include "instruments/sem/sem_reader.h"
#include "instruments/admin_msg/admin_msg_reader.h"

namespace metop
{
    namespace instruments
    {
        class MetOpInstrumentsDecoderModule : public ProcessingModule
        {
        protected:
            std::atomic<size_t> filesize;
            std::atomic<size_t> progress;

            // Readers
            avhrr::AVHRRReader avhrr_reader;
            noaa_metop::mhs::MHSReader mhs_reader;
            ascat::ASCATReader ascat_reader;
            iasi::IASIReader iasi_reader;
            iasi::IASIIMGReader iasi_reader_img;
            amsu::AMSUA1Reader amsu_a1_reader;
            amsu::AMSUA2Reader amsu_a2_reader;
            gome::GOMEReader gome_reader;
            sem::SEMReader sem_reader;
            admin_msg::AdminMsgReader admin_msg_reader;

            // Statuses
            instrument_status_t avhrr_status = DECODING;
            instrument_status_t iasi_status = DECODING;
            instrument_status_t iasi_img_status = DECODING;
            instrument_status_t mhs_status = DECODING;
            instrument_status_t amsu_status = DECODING;
            instrument_status_t gome_status = DECODING;
            instrument_status_t ascat_status = DECODING;
            instrument_status_t sem_status = DECODING;
            instrument_status_t admin_msg_status = DECODING;

        public:
            MetOpInstrumentsDecoderModule(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
            void process();
            void drawUI(bool window);

        public:
            static std::string getID();
            virtual std::string getIDM() { return getID(); };
            static std::vector<std::string> getParameters();
            static std::shared_ptr<ProcessingModule> getInstance(std::string input_file, std::string output_file_hint, nlohmann::json parameters);
        };
    } // namespace amsu
} // namespace metop