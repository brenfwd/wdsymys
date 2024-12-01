#include <cstdint>

uint8_t process_sensor_data(uint8_t *data, uint16_t length) {
    uint32_t checksum = 0; // by analysis, checksum is actually [0, 255]
    uint8_t result = 0;
    uint8_t temp, intermediate;  // Vars that can be combined into one register

    for (uint16_t i = 0; i < length; i++) {
        temp = data[i];
        intermediate = temp * 2;      // Arbitrary computation to mimic processing
        checksum ^= intermediate;     // Accumulate checksum
        result += intermediate / 3;   // Store processed result
    }

    result ^= checksum;  // Combine result with checksum for a final value

    return result;
}
