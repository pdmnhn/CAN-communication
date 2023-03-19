#include <iostream>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/adc.h"
#include "mcp2515.h"

using namespace std;

const uint ADC_CHN = 4;
const uint ADC_PIN = ADC_CHN + 26;
const uint ADC_RANGE = 1 << 12;
const uint LED = 25;
const double ADC_VREF = 3.3;
const double ADC_CONVERSION_FACTOR = ADC_VREF / (ADC_RANGE - 1);

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

int main()
{
    stdio_init_all();
    const uint16_t temperatureId = 0b01001100110;
    uint16_t counter = 0;

    MCP2515 canInterface;
    can_frame frame;
    frame.can_id;

    canInterface.reset();
    canInterface.setBitrate(CAN_1000KBPS, MCP_16MHZ);
    canInterface.setNormalMode();

    adc_init();
    adc_gpio_init(ADC_PIN);
    adc_select_input(ADC_CHN);
    gpio_init(LED);
    gpio_set_dir(LED, GPIO_OUT);

    double reading;
    uint8_t temperature = 0;

    while (true)
    {
        reading = adc_read() * ADC_CONVERSION_FACTOR;
        temperature = 27 - (reading - 0.706) / 0.001721;

        frame.can_id = getExtendedCanID(temperatureId, counter, LeiACommand::DATA);
        frame.can_dlc = 1;
        frame.data[0] = temperature;

        cout << "Temperature: " << (int)temperature << endl;

        if (canInterface.sendMessage(&frame) == MCP2515::ERROR_OK)
        {
            frame.can_id = getExtendedCanID(temperatureId, counter, LeiACommand::MAC_OF_DATA);
            for (uint8_t i = 0; i < MAC_LENGTH; i++)
            {
                frame.data[i] = STATIC_MAC[i];
            }
            frame.can_dlc = MAC_LENGTH;
            if (canInterface.sendMessage(&frame) == MCP2515::ERROR_OK)
            {
                cout << "Success: " << counter << endl;
                gpio_put(LED, 1);
                sleep_ms(2500);
                gpio_put(LED, 0);
            }
        }
        counter = (counter + 1) % LEIA_COUNTER_MAX_VALUE;
        sleep_ms(2500);
    }

    return 0;
}
