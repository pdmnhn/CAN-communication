#include <iostream>
#include <string>
#include "pico/stdlib.h"
#include "mcp2515.h"
#include "pico-ssd1306/ssd1306.h"
#include "pico-ssd1306/textRenderer/TextRenderer.h"
#include "hardware/i2c.h"

using namespace std;
using namespace pico_ssd1306;

const uint I2C_SDA = 2;
const uint I2C_SCL = 3;
const uint LED = 25;
const unsigned char MAC[8] = {0xFF, 0xDD, 0xCC, 0xBB, 0x00, 0x11, 0x22, 0x33};

int main()
{
    stdio_init_all();

    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);

    gpio_init(I2C_SDA);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);

    gpio_init(I2C_SCL);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SCL);

    i2c_init(i2c1, 1000000);

    SSD1306 display(i2c1, 0x3C, Size::W128xH64);
    display.setOrientation(0);

    // drawText(&display, font_8x8, "Temperature:", 0, 0);
    // drawText(&display, font_8x8, "Waiting...", 0, 16);
    // display.sendBuffer();

    MCP2515 canInterface;
    can_frame dataFrame, macFrame;

    canInterface.reset();
    canInterface.setBitrate(CAN_1000KBPS, MCP_16MHZ);
    canInterface.setNormalMode();

    while (true)
    {
        // drawText(&display, font_8x8, "Inside while loop", 0, 8);
        // display.sendBuffer();

        if (canInterface.readMessage(&dataFrame) == MCP2515::ERROR_OK && ((dataFrame.can_id >> 17) & 0b11) == 0b00)
        {
            display.clear();
            drawText(&display, font_8x8, "Data Frame", 0, 8);
            display.sendBuffer();
            sleep_ms(200);
            if (canInterface.readMessage(&macFrame) == MCP2515::ERROR_OK && ((macFrame.can_id >> 17) & 0b11) == 0b01)
            {
                drawText(&display, font_8x8, "MAC Frame", 0, 16);
                display.sendBuffer();
                uint16_t dataId = dataFrame.can_id >> 21;
                uint16_t dataFrameCounter = dataFrame.can_id >> 1, macFrameCounter = macFrame.can_id >> 1;

                if (dataFrameCounter == macFrameCounter)
                {
                    cout << "Received both data and mac frame for the same counter value" << endl;
                    int payloadLength = dataFrame.can_dlc;
                    cout << "DLC: " << payloadLength << endl;
                    // int temperature = dataFrame.data[0];
                    int temperature = 91;
                    // cout << "dataframe: ";
                    // for (int i = 0; i < 8; i++)
                    // {
                    //     cout << dataFrame.data[i] << ", ";
                    // }
                    cout << "Temperature: " << temperature << endl;
                    cout << "Counter: " << dataFrameCounter << endl;

                    drawText(&display, font_8x8, to_string(temperature).data(), 0, 24);
                    display.sendBuffer();
                    drawText(&display, font_8x8, to_string(dataFrameCounter).data(), 0, 32);
                    display.sendBuffer();
                    bool macMatch = true;
                    for (int i = 0; i < 8; i++)
                    {
                        // cout << "inside: ";
                        // cout << macFrame.data[i] << ", ";
                        if (MAC[i] != macFrame.data[i])
                        {
                            // cout << "MAC failed: " << i << endl;
                            macMatch = false;
                            break;
                        }
                    }

                    drawText(&display, font_8x8, "Authenticated", 0, 48);
                    // if (macMatch)
                    // {
                    // }
                    // else
                    // {
                    //     drawText(&display, font_8x8, "Failed", 0, 48);
                    // }
                    display.sendBuffer();
                }
            }
            // drawText(&display, font_8x8, "Received...", 0, 16);
            // display.sendBuffer();
            // }
            // cout << "ID: " << frame.can_id << "\n"
            //      << "DLC: " << frame.can_dlc << "\n"
            //      << "Temperature: " << temperature << "\n\n";
            // int dlc = frame.can_dlc;

            // cout << dlc << endl;

            // string canId = to_string(dataFrame.can_id);
            // string canDLC = to_string(dataFrame.can_dlc);
            // display.clear();
        }
    }

    return 0;
}
