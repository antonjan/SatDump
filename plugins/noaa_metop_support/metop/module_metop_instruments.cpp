#include "module_metop_instruments.h"
#include <fstream>
#include "common/ccsds/ccsds_1_0_1024/vcdu.h"
#include "logger.h"
#include <filesystem>
#include "imgui/imgui.h"
#include "common/utils.h"
#include "metop.h"
#include "common/image/bowtie.h"
#include "common/ccsds/ccsds_1_0_1024/demuxer.h"
#include "products/image_products.h"
#include "products/radiation_products.h"
#include "products/dataset.h"
#include "common/tracking/tle.h"
#include "resources.h"

namespace metop
{
    namespace instruments
    {
        MetOpInstrumentsDecoderModule::MetOpInstrumentsDecoderModule(std::string input_file, std::string output_file_hint, nlohmann::json parameters)
            : ProcessingModule(input_file, output_file_hint, parameters)
        {
        }

        void MetOpInstrumentsDecoderModule::process()
        {
            filesize = getFilesize(d_input_file);
            std::ifstream data_in(d_input_file, std::ios::binary);

            logger->info("Using input frames " + d_input_file);

            time_t lastTime = 0;
            uint8_t cadu[1024];

            // Demuxers
            ccsds::ccsds_1_0_1024::Demuxer demuxer_vcid3(882, true);
            ccsds::ccsds_1_0_1024::Demuxer demuxer_vcid9(882, true);
            ccsds::ccsds_1_0_1024::Demuxer demuxer_vcid10(882, true);
            ccsds::ccsds_1_0_1024::Demuxer demuxer_vcid12(882, true);
            ccsds::ccsds_1_0_1024::Demuxer demuxer_vcid15(882, true);
            ccsds::ccsds_1_0_1024::Demuxer demuxer_vcid24(882, true);
            ccsds::ccsds_1_0_1024::Demuxer demuxer_vcid34(882, true);

            // Setup Admin Message
            {
                std::string directory = d_output_file_hint.substr(0, d_output_file_hint.rfind('/')) + "/Admin Messages";
                if (!std::filesystem::exists(directory))
                    std::filesystem::create_directory(directory);
                admin_msg_reader.directory = directory;
            }

            std::vector<uint8_t> metop_scids;

            while (!data_in.eof())
            {
                // Read buffer
                data_in.read((char *)&cadu, 1024);

                // Parse this transport frame
                ccsds::ccsds_1_0_1024::VCDU vcdu = ccsds::ccsds_1_0_1024::parseVCDU(cadu);

                if (vcdu.spacecraft_id == METOP_A_SCID ||
                    vcdu.spacecraft_id == METOP_B_SCID ||
                    vcdu.spacecraft_id == METOP_C_SCID)
                    metop_scids.push_back(vcdu.spacecraft_id);

                if (vcdu.vcid == 9) // AVHRR/3
                {
                    std::vector<ccsds::CCSDSPacket> ccsdsFrames = demuxer_vcid9.work(cadu);
                    for (ccsds::CCSDSPacket &pkt : ccsdsFrames)
                        if (pkt.header.apid == 103 || pkt.header.apid == 104)
                            avhrr_reader.work(pkt);
                }
                else if (vcdu.vcid == 12) // MHS
                {
                    std::vector<ccsds::CCSDSPacket> ccsdsFrames = demuxer_vcid12.work(cadu);
                    for (ccsds::CCSDSPacket &pkt : ccsdsFrames)
                        if (pkt.header.apid == 34)
                        {
                            if (vcdu.spacecraft_id == METOP_A_SCID)
                                mhs_reader.work_metop(pkt, 1);
                            else if (vcdu.spacecraft_id == METOP_B_SCID)
                                mhs_reader.work_metop(pkt, 2);
                            else if (vcdu.spacecraft_id == METOP_C_SCID)
                                mhs_reader.work_metop(pkt, 3);
                        }
                }
                else if (vcdu.vcid == 15) // ASCAT
                {
                    std::vector<ccsds::CCSDSPacket> ccsdsFrames = demuxer_vcid15.work(cadu);
                    for (ccsds::CCSDSPacket &pkt : ccsdsFrames)
                        ascat_reader.work(pkt);
                }
                else if (vcdu.vcid == 10) // IASI
                {
                    std::vector<ccsds::CCSDSPacket> ccsdsFrames = demuxer_vcid10.work(cadu);
                    for (ccsds::CCSDSPacket &pkt : ccsdsFrames)
                    {
                        if (pkt.header.apid == 150)
                            iasi_reader_img.work(pkt);
                        else if (pkt.header.apid == 130 || pkt.header.apid == 135 || pkt.header.apid == 140 || pkt.header.apid == 145)
                            iasi_reader.work(pkt);
                    }
                }
                else if (vcdu.vcid == 3) // AMSU
                {
                    std::vector<ccsds::CCSDSPacket> ccsdsFrames = demuxer_vcid3.work(cadu);
                    for (ccsds::CCSDSPacket &pkt : ccsdsFrames)
                    {
                        if (pkt.header.apid == 39)
                            amsu_a1_reader.work(pkt);
                        else if (pkt.header.apid == 40)
                            amsu_a2_reader.work(pkt);
                        else if (pkt.header.apid == 37)
                            sem_reader.work(pkt);
                    }
                }
                else if (vcdu.vcid == 24) // GOME
                {
                    std::vector<ccsds::CCSDSPacket> ccsdsFrames = demuxer_vcid24.work(cadu);
                    for (ccsds::CCSDSPacket &pkt : ccsdsFrames)
                        if (pkt.header.apid == 384)
                            gome_reader.work(pkt);
                }
                else if (vcdu.vcid == 34) // Admin Messages
                {
                    std::vector<ccsds::CCSDSPacket> ccsdsFrames = demuxer_vcid34.work(cadu);
                    for (ccsds::CCSDSPacket &pkt : ccsdsFrames)
                        if (pkt.header.apid == 6)
                            admin_msg_reader.work(pkt);
                }

                progress = data_in.tellg();
                if (time(NULL) % 10 == 0 && lastTime != time(NULL))
                {
                    lastTime = time(NULL);
                    logger->info("Progress " + std::to_string(round(((float)progress / (float)filesize) * 1000.0f) / 10.0f) + "%");
                }
            }

            data_in.close();

            int scid = most_common(metop_scids.begin(), metop_scids.end());
            metop_scids.clear();

            std::string sat_name = "Unknown MetOp";
            if (scid == METOP_A_SCID)
                sat_name = "MetOp-A";
            else if (scid == METOP_B_SCID)
                sat_name = "MetOp-B";
            else if (scid == METOP_C_SCID)
                sat_name = "MetOp-C";

            int norad = 0;
            if (scid == METOP_A_SCID)
                norad = METOP_A_NORAD;
            else if (scid == METOP_B_SCID)
                norad = METOP_B_NORAD;
            else if (scid == METOP_C_SCID)
                norad = METOP_C_NORAD;

            std::optional<satdump::TLE> satellite_tle = admin_msg_reader.tles.get_from_norad(norad);
            if (!satellite_tle.has_value())
                satellite_tle = satdump::general_tle_registry.get_from_norad(norad);

            // Products dataset
            satdump::ProductDataSet dataset;
            dataset.satellite_name = sat_name;
            dataset.timestamp = avg_overflowless(avhrr_reader.timestamps);

            // Satellite ID
            {
                logger->info("----------- Satellite");
                logger->info("NORAD : " + std::to_string(norad));
                logger->info("Name  : " + sat_name);
            }

            // AVHRR
            {
                avhrr_status = SAVING;
                std::string directory = d_output_file_hint.substr(0, d_output_file_hint.rfind('/')) + "/AVHRR";

                if (!std::filesystem::exists(directory))
                    std::filesystem::create_directory(directory);

                logger->info("----------- AVHRR/3");
                logger->info("Lines : " + std::to_string(avhrr_reader.lines));

                satdump::ImageProducts avhrr_products;
                avhrr_products.instrument_name = "avhrr_3";
                avhrr_products.has_timestamps = true;
                avhrr_products.set_tle(satellite_tle);
                avhrr_products.bit_depth = 10;
                avhrr_products.timestamp_type = satdump::ImageProducts::TIMESTAMP_LINE;
                avhrr_products.set_timestamps(avhrr_reader.timestamps);
                avhrr_products.set_proj_cfg(loadJsonFile(resources::getResourcePath("projections_settings/metop_abc_avhrr.json")));

                for (int i = 0; i < 5; i++)
                    avhrr_products.images.push_back({"AVHRR-" + std::to_string(i + 1) + ".png", std::to_string(i + 1), avhrr_reader.getChannel(i)});

                avhrr_products.save(directory);
                dataset.products_list.push_back("AVHRR");

                avhrr_status = DONE;
            }

            // MHS
            {
                mhs_status = SAVING;
                std::string directory = d_output_file_hint.substr(0, d_output_file_hint.rfind('/')) + "/MHS";

                if (!std::filesystem::exists(directory))
                    std::filesystem::create_directory(directory);

                logger->info("----------- MHS");
                logger->info("Lines : " + std::to_string(mhs_reader.line));

                satdump::ImageProducts mhs_products;
                mhs_products.instrument_name = "mhs";
                mhs_products.has_timestamps = true;
                mhs_products.set_tle(satellite_tle);
                mhs_products.bit_depth = 16;
                mhs_products.timestamp_type = satdump::ImageProducts::TIMESTAMP_LINE;
                mhs_products.set_timestamps(mhs_reader.timestamps);
                mhs_products.set_proj_cfg(loadJsonFile(resources::getResourcePath("projections_settings/metop_abc_mhs.json")));

                for (int i = 0; i < 5; i++)
                    mhs_products.images.push_back({"MHS-" + std::to_string(i + 1) + ".png", std::to_string(i + 1), mhs_reader.getChannel(i)});

                mhs_products.save(directory);
                dataset.products_list.push_back("MHS");

                mhs_status = DONE;
            }

            // ASCAT
            {
                ascat_status = SAVING;
                std::string directory = d_output_file_hint.substr(0, d_output_file_hint.rfind('/')) + "/ASCAT";

                if (!std::filesystem::exists(directory))
                    std::filesystem::create_directory(directory);

                logger->info("----------- ASCAT");
                for (int i = 0; i < 6; i++)
                    logger->info("Channel " + std::to_string(i + 1) + " lines : " + std::to_string(ascat_reader.lines[i]));

                for (int i = 0; i < 6; i++)
                {
                    logger->info("Channel " + std::to_string(i + 1) + "...");
                    image::Image<uint16_t> img = ascat_reader.getChannel(i);
                    WRITE_IMAGE(img, directory + "/ASCAT-" + std::to_string(i + 1) + ".png");
                }
                ascat_status = DONE;
            }

            // IASI
            {
                iasi_img_status = iasi_status = SAVING;
                std::string directory_img = d_output_file_hint.substr(0, d_output_file_hint.rfind('/')) + "/IASI-IMG";
                std::string directory = d_output_file_hint.substr(0, d_output_file_hint.rfind('/')) + "/IASI";

                if (!std::filesystem::exists(directory))
                    std::filesystem::create_directory(directory);
                if (!std::filesystem::exists(directory_img))
                    std::filesystem::create_directory(directory_img);

                logger->info("----------- IASI");
                logger->info("Lines : " + std::to_string(iasi_reader.lines));
                logger->info("Lines (Imaging) : " + std::to_string(iasi_reader_img.lines * 64));

                const float alpha = 1.0 / 1.8;
                const float beta = 0.58343;
                const long scanHeight = 64;

                if (iasi_reader_img.lines > 0)
                {
                    logger->info("Channel IR imaging...");
                    image::Image<uint16_t> iasi_imaging = iasi_reader_img.getIRChannel();
                    iasi_imaging = image::bowtie::correctGenericBowTie(iasi_imaging, 1, scanHeight, alpha, beta); // Bowtie.... As IASI scans per IFOV
                    iasi_imaging.simple_despeckle(10);                                                            // And, it has some dead pixels sometimes so well, we need to remove them I guess?

                    // Test! TODO : Cleanup!!
                    satdump::ImageProducts iasi_img_products;
                    iasi_img_products.instrument_name = "iasi_img";
                    iasi_img_products.has_timestamps = true;
                    iasi_img_products.set_tle(satellite_tle);
                    iasi_img_products.bit_depth = 16;
                    iasi_img_products.timestamp_type = satdump::ImageProducts::TIMESTAMP_IFOV;
                    iasi_img_products.set_timestamps(iasi_reader_img.timestamps_ifov);
                    iasi_img_products.set_proj_cfg(loadJsonFile(resources::getResourcePath("projections_settings/metop_abc_iasi_img.json")));
                    iasi_img_products.images.push_back({"IASI-IMG.png", "1", iasi_imaging});

                    iasi_img_products.save(directory_img);
                    dataset.products_list.push_back("IASI-IMG");
                }
                iasi_img_status = DONE;

                satdump::ImageProducts iasi_products;
                iasi_products.instrument_name = "iasi";
                iasi_products.has_timestamps = true;
                iasi_products.timestamp_type = satdump::ImageProducts::TIMESTAMP_LINE;
                iasi_products.set_timestamps(iasi_reader.timestamps);
                iasi_products.set_tle(satellite_tle);
                iasi_products.set_proj_cfg(loadJsonFile(resources::getResourcePath("projections_settings/metop_abc_iasi.json")));
                iasi_products.save_as_matrix = true;

                for (int i = 0; i < 8461; i++)
                    iasi_products.images.push_back({"IASI-ALL.png", std::to_string(i + 1), iasi_reader.getChannel(i)});

                iasi_products.save(directory);
                dataset.products_list.push_back("IASI");

                iasi_status = DONE;
            }

            // AMSU
            {
                amsu_status = SAVING;
                std::string directory = d_output_file_hint.substr(0, d_output_file_hint.rfind('/')) + "/AMSU";

                if (!std::filesystem::exists(directory))
                    std::filesystem::create_directory(directory);

                logger->info("----------- AMSU");
                logger->info("Lines (AMSU A1) : " + std::to_string(amsu_a1_reader.lines));
                logger->info("Lines (AMSU A2) : " + std::to_string(amsu_a2_reader.lines));

                satdump::ImageProducts amsu_products;
                amsu_products.instrument_name = "amsu_a";
                amsu_products.has_timestamps = true;
                amsu_products.set_tle(satellite_tle);
                amsu_products.bit_depth = 16;
                amsu_products.timestamp_type = satdump::ImageProducts::TIMESTAMP_LINE;
                // amsu_products.set_timestamps(mhs_reader.timestamps);
                amsu_products.set_proj_cfg(loadJsonFile(resources::getResourcePath("projections_settings/metop_abc_amsu.json")));

                for (int i = 0; i < 2; i++)
                    amsu_products.images.push_back({"AMSU-A2-" + std::to_string(i + 1) + ".png", std::to_string(i + 1), amsu_a2_reader.getChannel(i), amsu_a2_reader.timestamps});

                for (int i = 0; i < 13; i++)
                    amsu_products.images.push_back({"AMSU-A1-" + std::to_string(i + 1) + ".png", std::to_string(i + 3), amsu_a1_reader.getChannel(i), amsu_a1_reader.timestamps});

                amsu_products.save(directory);
                dataset.products_list.push_back("AMSU");

                amsu_status = DONE;
            }

            // GOME
            {
                gome_status = SAVING;
                std::string directory = d_output_file_hint.substr(0, d_output_file_hint.rfind('/')) + "/GOME";

                if (!std::filesystem::exists(directory))
                    std::filesystem::create_directory(directory);

                logger->info("----------- GOME");
                logger->info("Lines : " + std::to_string(gome_reader.lines));

                satdump::ImageProducts gome_products;
                gome_products.instrument_name = "gome";
                gome_products.has_timestamps = true;
                gome_products.timestamp_type = satdump::ImageProducts::TIMESTAMP_LINE;
                gome_products.set_timestamps(gome_reader.timestamps);
                gome_products.save_as_matrix = true;

                for (int i = 0; i < 6144; i++)
                    gome_products.images.push_back({"GOME-ALL.png", std::to_string(i + 1), gome_reader.getChannel(i)});

                gome_products.save(directory);
                dataset.products_list.push_back("GOME");
                gome_status = DONE;
            }

            // Admin Messages
            {
                admin_msg_status = DONE;
                logger->info("----------- Admin Message");
                logger->info("Count : " + std::to_string(admin_msg_reader.count));
            }

            // SEM
            {
                sem_status = SAVING;
                std::string directory = d_output_file_hint.substr(0, d_output_file_hint.rfind('/')) + "/SEM";

                if (!std::filesystem::exists(directory))
                    std::filesystem::create_directory(directory);

                logger->info("----------- SEM");
                logger->info("Packets : " + std::to_string(sem_reader.samples));

                satdump::RadiationProducts sem_products;
                sem_products.instrument_name = "sem";
                sem_products.has_timestamps = true;
                sem_products.set_tle(satellite_tle);
                sem_products.set_timestamps_all(sem_reader.timestamps);
                sem_products.channel_counts.resize(40);
                for (int i = 0; i < sem_reader.samples * 16; i++)
                    for (int c = 0; c < 40; c++)
                        sem_products.channel_counts[c].push_back(sem_reader.channels[c][i]);

                sem_products.save(directory);
                dataset.products_list.push_back("SEM");

                sem_status = DONE;
            }

            dataset.save(d_output_file_hint.substr(0, d_output_file_hint.rfind('/')));
        }

        void MetOpInstrumentsDecoderModule::drawUI(bool window)
        {
            ImGui::Begin("MetOp Instruments Decoder", NULL, window ? 0 : NOWINDOW_FLAGS);

            if (ImGui::BeginTable("##metopinstrumentstable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Instrument");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("Lines / Frames");
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("Status");

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("AVHRR");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImColor(0, 255, 0), "%d", avhrr_reader.lines);
                ImGui::TableSetColumnIndex(2);
                drawStatus(avhrr_status);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("IASI");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImColor(0, 255, 0), "%d", iasi_reader.lines);
                ImGui::TableSetColumnIndex(2);
                drawStatus(iasi_status);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("IASI Imaging");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImColor(0, 255, 0), "%d", iasi_reader_img.lines * 64);
                ImGui::TableSetColumnIndex(2);
                drawStatus(iasi_img_status);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("MHS");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImColor(0, 255, 0), "%d", mhs_reader.line);
                ImGui::TableSetColumnIndex(2);
                drawStatus(mhs_status);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("AMSU A1");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImColor(0, 255, 0), "%d", amsu_a1_reader.lines);
                ImGui::TableSetColumnIndex(2);
                drawStatus(amsu_status);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("AMSU A2");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImColor(0, 255, 0), "%d", amsu_a2_reader.lines);
                ImGui::TableSetColumnIndex(2);
                drawStatus(amsu_status);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("GOME");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImColor(0, 255, 0), "%d", gome_reader.lines);
                ImGui::TableSetColumnIndex(2);
                drawStatus(gome_status);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("ASCAT");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImColor(0, 255, 0), "%d", ascat_reader.lines[0]);
                ImGui::TableSetColumnIndex(2);
                drawStatus(gome_status);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("SEM");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImColor(0, 255, 0), "%d", sem_reader.samples);
                ImGui::TableSetColumnIndex(2);
                drawStatus(sem_status);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Admin Messages");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextColored(ImColor(0, 255, 0), "%d", admin_msg_reader.count);
                ImGui::TableSetColumnIndex(2);
                drawStatus(admin_msg_status);

                ImGui::EndTable();
            }

            ImGui::ProgressBar((float)progress / (float)filesize, ImVec2(ImGui::GetWindowWidth() - 10, 20 * ui_scale));

            ImGui::End();
        }

        std::string MetOpInstrumentsDecoderModule::getID()
        {
            return "metop_instruments";
        }

        std::vector<std::string> MetOpInstrumentsDecoderModule::getParameters()
        {
            return {};
        }

        std::shared_ptr<ProcessingModule> MetOpInstrumentsDecoderModule::getInstance(std::string input_file, std::string output_file_hint, nlohmann::json parameters)
        {
            return std::make_shared<MetOpInstrumentsDecoderModule>(input_file, output_file_hint, parameters);
        }
    } // namespace amsu
} // namespace metop