#include <iostream>
#include <stdint.h>
#include <vector>
#include "unordered_map"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/adc.h"
#include "mcp2515.h"
#include "LeiA/leia.h"

using namespace std;
using namespace leia;

const uint ADC_CHN = 4;
const uint ADC_PIN = ADC_CHN + 26;
const uint ADC_RANGE = 1 << 12;
const uint LED = 25;
const double ADC_VREF = 3.3;
const double ADC_CONVERSION_FACTOR = ADC_VREF / (ADC_RANGE - 1);

const static uint8_t MAC_LENGTH = 8;
uint8_t dataHolder1[BYTES] = {0};

uint8_t
    LONG_TERM_KEY1[BYTES] = {0x33, 0x74, 0x36, 0x77, 0x39, 0x7A, 0x24, 0x43, 0x26, 0x46, 0x29, 0x4A, 0x40, 0x4E, 0x63, 0x52},
    LONG_TERM_KEY2[BYTES] = {0x4A, 0x40, 0x4E, 0x63, 0x52, 0x66, 0x55, 0x6A, 0x58, 0x6E, 0x32, 0x72, 0x35, 0x75, 0x38, 0x78};

unordered_map<uint16_t, LeiAState *>
    canIdToLeiA = {
        {0b11001100111, new LeiAState(LONG_TERM_KEY1)},
        {0b10101010111, new LeiAState(LONG_TERM_KEY2)}};

int main()
{
    stdio_init_all();
    const uint16_t temperatureId = 0b11001100111;
    const uint16_t temperatureMACId = temperatureId + 0b1;
    const uint16_t temperatureAECId = temperatureId + 0b10;
    LeiAState *leiastate = canIdToLeiA[temperatureId];

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
        /*
        if (canInterface.readMessage(&frame) == MCP2515::ERROR_OK and frame.can_id == temperatureAECId)
        {
            uint64_t data = 0u;
            const uint16_t canIdSelectionBits = 0b11111111111u;
            const uint64_t epochSelectionBits = ~(canIdSelectionBits << 53u);
            for (size_t i = 0u; i < 8u; i++)
            {
                data = data << (i * 8u) | frame.data[i];
            }

            uint16_t dataChannelCanId = (data >> 53u) & canIdSelectionBits;
            uint64_t receiverEpoch = data & epochSelectionBits;

            if ((leiastate->getEpoch() & epochSelectionBits) != receiverEpoch)
            {
                cout << "-------------------------------" << endl;
                cout << "Resync Started" << endl;
                // Resync required
                uint64_t senderEpoch = leiastate->getEpoch();
                uint16_t senderCounter = leiastate->getCounter();
                frame.can_id = getExtendedCanID(temperatureAECId, senderCounter, LeiACommand::EPOCH);
                frame.can_dlc = 8u;
                for (size_t i = 0u; i < 8u; i++)
                {
                    frame.data[i] = (senderEpoch >> (i * 8)) & 0xFF;
                }

                if (canInterface.sendMessage(&frame) == MCP2515::ERROR_OK)
                {
                    frame.can_id = getExtendedCanID(temperatureAECId, senderCounter, LeiACommand::MAC_OF_EPOCH);
                    frame.can_dlc = 8u;
                    vector<uint8_t> macOfEpoch = leiastate->resyncOfSender();
                    for (size_t i = 0u; i < 8u; i++)
                    {
                        frame.data[i] = macOfEpoch[i];
                    }
                    if (canInterface.sendMessage(&frame) == MCP2515::ERROR_OK)
                    {
                        cout << "Resync from sender completed" << endl;
                        cout << "-------------------------------" << endl;
                    }
                }
            }
        }
*/
        reading = adc_read() * ADC_CONVERSION_FACTOR;
        temperature = 27 - (reading - 0.706) / 0.001721;
        const uint16_t currentCounter = leiastate->getCounter();
        frame.can_id = getExtendedCanID(temperatureId, currentCounter, LeiACommand::DATA);
        frame.can_dlc = 1;
        frame.data[0] = temperature;

        cout << "Temperature: " << (int)temperature << endl;
        if (canInterface.sendMessage(&frame) == MCP2515::ERROR_OK)
        {
            frame.can_id = getExtendedCanID(temperatureMACId, currentCounter, LeiACommand::MAC_OF_DATA);

            dataHolder1[0] = dataHolder1[8] = frame.data[0];
            vector<uint8_t> macOfData = leiastate->generateMAC(dataHolder1);
            for (uint8_t i = 0; i < MAC_LENGTH; i++)
            {
                frame.data[i] = macOfData[i];
            }

            cout << "MAC"
                 << "------------------------" << endl;
            for (size_t i = 0; i < 16U; i++)
            {
                cout << (int)macOfData[i] << " -> ";
            }
            cout << endl
                 << "----------------------------------" << endl;

            frame.can_dlc = MAC_LENGTH;
            if (canInterface.sendMessage(&frame) == MCP2515::ERROR_OK)
            {
                cout << "Success: " << currentCounter << endl;
                gpio_put(LED, 1);
                sleep_ms(2500);
                gpio_put(LED, 0);
            }
        }
        sleep_ms(2500);
    }
    return 0;
}
