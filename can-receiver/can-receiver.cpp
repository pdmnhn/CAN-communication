#include <iostream>
#include <string>
#include <tuple>
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
const uint16_t I2C_DISPLAY_ADDRESS = 0x3C;

enum LeiACommand : uint8_t
{
    DATA = 0b00,
    MAC_OF_DATA = 0b01,
    EPOCH = 0b10,
    MAC_OF_EPOCH = 0b11
};
const uint16_t LEIA_COUNTER_MAX_VALUE = 0xFFFF;
const static uint8_t MAC_LENGTH = 8;
const unsigned char STATIC_MAC[MAC_LENGTH] = {0xFF, 0xDD, 0xCC, 0xBB, 0x00, 0x11, 0x22, 0x33};

uint32_t getExtendedCanID(uint16_t canID, uint16_t counter, LeiACommand command)
{
    /*
    canID -> 11 bit CAN identifier
    counter -> 16 bit counter for LeiA
    command -> 2 bit command for LeiA
    Returns 32 bit uint with 29 bit extended CAN ID to be used with the MCP-2515 Library
    ORed with the CAN_EFF_FLAG flag
    */
    uint32_t exCanID = (canID << 18) | (command << 16) | counter | CAN_EFF_FLAG;
    return exCanID;
}

tuple<uint16_t, uint16_t, LeiACommand> splitExtendedCanID(uint32_t exCanID)
{
    /*
    exCanID -> 29 bit extended CAN identifier taken in 32 bit long unsigned integer
    Returns {11 bit CAN identifier, 16 bit counter for LeiA, 2 bit command for LeiA}
    */
    uint16_t canID = exCanID >> 18;
    uint16_t counter = exCanID & 0xFFFF;
    LeiACommand command = LeiACommand((exCanID >> 16) & 0b11);

    return {canID, counter, command};
}

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

    SSD1306 display(i2c1, I2C_DISPLAY_ADDRESS, Size::W128xH64);
    display.setOrientation(0);

    MCP2515 canInterface;
    can_frame dataFrame, macFrame;

    canInterface.reset();
    canInterface.setBitrate(CAN_1000KBPS, MCP_16MHZ);
    canInterface.setNormalMode();

    while (true)
    {
        if (canInterface.readMessage(&dataFrame) == MCP2515::ERROR_OK)
        {
            auto [dataFrameCanID, dataFrameCounter, dataFrameCommand] = splitExtendedCanID(dataFrame.can_id);
            if (dataFrameCommand == LeiACommand::DATA)
            {
                display.clear();
                drawText(&display, font_8x8, "RXd Data Frame", 0, 8);
                display.sendBuffer();

                sleep_ms(200); // to ensure the MAC frame is sent before reading

                if (canInterface.readMessage(&macFrame) == MCP2515::ERROR_OK)
                {
                    cout << "RXd MAC Frame" << endl;
                    auto [macFrameCanID, macFrameCounter, macFrameCommand] = splitExtendedCanID(macFrame.can_id);

                    if (macFrameCommand == LeiACommand::MAC_OF_DATA &&
                        macFrameCanID == dataFrameCanID &&
                        macFrameCounter == dataFrameCounter)
                    {
                        drawText(&display, font_8x8, "RXd MAC Frame", 0, 16);
                        display.sendBuffer();
                        uint8_t temperature = dataFrame.data[0];

                        cout << "Temperature: " << (int)temperature << endl;
                        cout << "Counter: " << dataFrameCounter << endl;

                        drawText(&display, font_8x8, ("Temp: " + to_string(temperature)).data(), 0, 24);
                        drawText(&display, font_8x8, ("Counter: " + to_string(dataFrameCounter)).data(), 0, 32);
                        display.sendBuffer();

                        bool macMatch = true;

                        for (uint8_t i = 0; i < MAC_LENGTH; i++)
                        {
                            if (macFrame.data[i] != STATIC_MAC[i])
                            {
                                macMatch = false;
                                break;
                            }
                        }

                        if (macMatch)
                        {
                            drawText(&display, font_8x8, "Authenticated", 0, 48);
                        }
                        else
                        {
                            drawText(&display, font_8x8, "Failed", 0, 48);
                        }
                        display.sendBuffer();
                    }
                }
            }
        }
    }

    return 0;
}
