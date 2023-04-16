#include <iostream>
#include <string>
#include <tuple>
#include <unordered_map>
#include "pico/stdlib.h"
#include "mcp2515.h"
#include "LeiA/leia.h"
#include "pico-ssd1306/ssd1306.h"
#include "pico-ssd1306/textRenderer/TextRenderer.h"
#include "hardware/i2c.h"

using namespace std;
using namespace pico_ssd1306;
using namespace leia;

const uint I2C_SDA = 2;
const uint I2C_SCL = 3;
const uint LED = 25;
const uint16_t I2C_DISPLAY_ADDRESS = 0x3C;

const static uint8_t MAC_LENGTH = 8;
const unsigned char STATIC_MAC[MAC_LENGTH] = {0xFF, 0xDD, 0xCC, 0xBB, 0x00, 0x11, 0x22, 0x33};

tuple<uint16_t, uint16_t, LeiACommand> splitExtendedCanID(uint32_t exCanID)
{
    /*
    exCanID -> 29 bit extended CAN identifier taken in 32 bit long unsigned integer
    Returns {11 bit CAN identifier, 16 bit counter for LeiA, 2 bit command for LeiA}
    */
    exCanID = exCanID & (~(1 << 31));
    uint16_t canID = exCanID >> 18;
    uint16_t counter = exCanID & 0xFFFF;
    LeiACommand command = LeiACommand((exCanID >> 16) & 0b11);

    return {canID, counter, command};
}

uint8_t
    LONG_TERM_KEY1[BYTES] = {0x33, 0x74, 0x36, 0x77, 0x39, 0x7A, 0x24, 0x43, 0x26, 0x46, 0x29, 0x4A, 0x40, 0x4E, 0x63, 0x52},
    LONG_TERM_KEY2[BYTES] = {0x4A, 0x40, 0x4E, 0x63, 0x52, 0x66, 0x55, 0x6A, 0x58, 0x6E, 0x32, 0x72, 0x35, 0x75, 0x38, 0x78};

unordered_map<uint16_t, LeiAState *>
    canIdToLeiA = {
        {0b11001100111, new LeiAState(LONG_TERM_KEY1)},
        {0b10101010111, new LeiAState(LONG_TERM_KEY2)}};

uint8_t dataHolder1[BYTES] = {0}, dataHolder2[BYTES] = {0};

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
    can_frame dataFrame, macFrame, aecFrame;

    canInterface.reset();
    canInterface.setBitrate(CAN_1000KBPS, MCP_16MHZ);
    canInterface.setNormalMode();

    while (true)
    {
        if (canInterface.readMessage(&dataFrame) == MCP2515::ERROR_OK)
        {
            const auto [dataFrameCanID, dataFrameCounter, dataFrameCommand] = splitExtendedCanID(dataFrame.can_id);
            if (dataFrameCommand == LeiACommand::DATA)
            {
                display.clear();
                drawText(&display, font_8x8, "RXd Data Frame", 0, 8);
                display.sendBuffer();

                while (canInterface.readMessage(&macFrame) != MCP2515::ERROR_OK)
                    ;

                auto [macFrameCanID, macFrameCounter, macFrameCommand] = splitExtendedCanID(macFrame.can_id);

                if (macFrameCommand == LeiACommand::MAC_OF_DATA &&
                    macFrameCanID == (dataFrameCanID + 1u) &&
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

                    dataHolder1[0] = dataHolder1[8] = dataFrame.data[0];

                    memcpy(dataHolder2, macFrame.data, 8);
                    memcpy(dataHolder2 + 8, macFrame.data, 8);

                    bool shouldResync = false;
                    LeiAState *leiastate = canIdToLeiA.count(dataFrameCanID) ? canIdToLeiA[dataFrameCanID] : nullptr;
                    if (leiastate != nullptr)
                    {
                        if (leiastate->authenticate(dataHolder1, dataHolder2))
                        {
                            drawText(&display, font_8x8, "Authenticated", 0, 48);
                            cout << "Authenticated" << endl;
                        }
                        else
                        {
                            drawText(&display, font_8x8, "Failed", 0, 48);
                            cout << "Authentication Failed" << endl;
                            shouldResync = true;
                        }
                        display.sendBuffer();
                    }

                    if (shouldResync)
                    {
                        cout << "-------------------------------" << endl;
                        cout << "Init Resync" << endl;
                        display.clear();
                        drawText(&display, font_8x8, "Resync Started", 0, 8);
                        display.sendBuffer();
                        const uint16_t aecId = 0;
                        aecFrame.can_id = aecId;
                        uint64_t canIdMask = 0b11111111111;
                        uint64_t data = (((uint64_t)dataFrameCanID) << 53u) | (leiastate->getEpoch() & (~(canIdMask << 53u)));
                        aecFrame.can_dlc = 8u;

                        for (int i = 7; i >= 0; i--)
                        {
                            aecFrame.data[i] = (uint8_t)(data & (0xFF));
                            data = data >> ((uint8_t)0xFF);
                        }

                        if (canInterface.sendMessage(&aecFrame) == MCP2515::ERROR_OK)
                        {
                            cout << "Sent AEC frame" << endl;
                            drawText(&display, font_8x8, "Sent AUTH_FAIL", 0, 16);
                            display.sendBuffer();
                            while (true)
                            {
                                if (canInterface.readMessage(&aecFrame) == MCP2515::ERROR_OK)
                                    break;
                            }

                            auto [senderAecId, senderCounter, senderCommand] = splitExtendedCanID(aecFrame.can_id);
                            cout << "Sender Counter: " << (int)senderCounter << endl;
                            if (senderAecId == aecId and senderCommand == LeiACommand::EPOCH)
                            {
                                uint64_t senderEpoch = 0u;
                                for (int i = 7; i >= 0; i--)
                                {
                                    senderEpoch = (senderEpoch << 8u) | aecFrame.data[i];
                                }
                                cout << "Sender Epoch: " << senderEpoch << endl;

                                while (true)
                                {
                                    if (canInterface.readMessage(&aecFrame) == MCP2515::ERROR_OK)
                                        break;
                                }

                                auto [macAecId, macAecCounter, macAecCommand] = splitExtendedCanID(aecFrame.can_id);
                                if (macAecId == aecId and macAecCommand == LeiACommand::MAC_OF_EPOCH)
                                {
                                    if (leiastate->resyncOfReceiver(senderEpoch, senderCounter, aecFrame.data))
                                    {
                                        cout << "Resync Succeeded" << endl;
                                        drawText(&display, font_8x8, "Resync Succeeded", 0, 48);
                                        display.sendBuffer();
                                    }
                                    else
                                    {
                                        cout << "Resync Failed" << endl;
                                        drawText(&display, font_8x8, "Resync Failed", 0, 48);
                                        display.sendBuffer();
                                    }
                                }
                            }
                        }
                        cout << "-------------------------------" << endl;
                    }
                }
            }
        }
    }

    return 0;
}
