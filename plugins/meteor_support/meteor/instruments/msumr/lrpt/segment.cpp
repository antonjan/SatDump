#include "segment.h"
#include "huffman.h"
#include <cstring>
#include "tables.h"
#include "idct.h"
#include "common/ccsds/ccsds_time.h"

namespace meteor
{
    namespace msumr
    {
        namespace lrpt
        {
            bool Segment::isValid()
            {
                return QT == 0x00 && DC == 0x00 && AC == 0x00 && QFM == 0xFFF0 &&
                       valid; // &&
                              //day_time == 0 && us_time == 0;
            }

            Segment::Segment(uint8_t *data, int length)
            {
                //buffer = new bool[length * 8];
                buffer = std::shared_ptr<bool>(new bool[length * 8], [](bool *p)
                                               { delete[] p; });

                if (length - 14 <= 0)
                {
                    valid = false;
                }
                else
                {
                    day_time = data[0] << 8 | data[1];
                    ms_time = data[2] << 24 | data[3] << 16 | data[4] << 8 | data[5];
                    us_time = data[6] << 8 | data[7];
                    timestamp = ccsds::parseCCSDSTimeFullRaw(data, 0); // Parse ignoring the day

                    MCUN = data[8];
                    QT = data[9];
                    DC = data[10] & 0xF0 >> 4;
                    AC = data[10] & 0x0F;
                    QFM = data[11] << 8 | data[12];
                    QF = data[13];

                    decode(&data[14], length - 14);

                    valid = true;
                }
            }

            Segment::Segment()
            {
                valid = false;
            }

            Segment::~Segment()
            {
                //delete[] buffer;
            }

            void Segment::decode(uint8_t *data, int length)
            {
                //std::cout << "START " << length << std::endl;
                //std::cout << "DC SEGBUFLEN " << length << std::endl;
                convertToArray(buffer.get(), data, length);
                length = length * 8;
                std::array<int64_t, 64> qTable = GetQuantizationTable((float)QF);
                int64_t lastDC = 0;

                //std::cout << "START2" << std::endl;

                bool *buf = &buffer.get()[0];

                for (int i = 0; i < 14; i++)
                {
                    int64_t block[64];
                    std::fill(&block[0], &block[64], 0);
                    int index = 1;

                    //std::cout << "START3" << std::endl;

                    int64_t val = FindDC(buf, length);

                    //std::cout << "START4" << std::endl;

                    //std::cout << "DC " << i << " = " << val << std::endl;

                    if (val == CFC[0])
                    {
                        valid = false;
                        // Invalid
                        //std::cout << "INVALID DC" << std::endl;
                        return;
                    }

                    //std::cout << "VALID DC" << std::endl;

                    lastDC += val;
                    block[0] = lastDC;

                    for (int j = 0; j < 63;)
                    {
                        std::vector<int64_t> vals = FindAC(buf, length);
                        j += vals.size();

                        if (vals[0] == CFC[0])
                        {
                            // Invalid
                            //std::cout << "INVALID AC" << std::endl;
                            valid = false;
                            return;
                        }

                        //std::cout << "VALID AC" << std::endl;

                        if (vals[0] != EOB[0] && index + vals.size() < 64)
                        {
                            std::memcpy(&block[index], &vals.data()[0], vals.size() * sizeof(int64_t));
                            index += vals.size();
                        }
                        else
                        {
                            break;
                        }

                        //std::cout << "VALID AC 2" << std::endl;
                        //detect.write((char *)vals.data(), vals.size() * sizeof(int64_t));
                    }

                    //detect.write((char *)&block, 64 * sizeof(int64_t));

                    int64_t idctBlock[64];
                    std::fill(&idctBlock[0], &idctBlock[64], 0);
                    for (int x = 0; x < 64; x++)
                    {
                        idctBlock[x] = block[Zigzag[x]] * qTable[x];
                    }

                    //std::cout << "Start IDCT" << std::endl;

                    Idct(idctBlock);

                    //std::cout << "End IDCT" << std::endl;

                    for (int x = 0; x < 64; x++)
                    {
                        int64_t normalizedPixel = idctBlock[x] + 128;

                        if (normalizedPixel > 255)
                            normalizedPixel = 255;
                        if (normalizedPixel < 0)
                            normalizedPixel = 0;

                        lines[x / 8][(i * 8) + (x % 8)] = (uint8_t)normalizedPixel;
                    }
                    //std::cout << "fully decoded" << std::endl;
                }
            }
        } // namespace lrpt
    }     // namespace msumr
} // namespace meteor