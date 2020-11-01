/* Edge Impulse ingestion SDK
 * Copyright (c) 2020 EdgeImpulse Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "qcbor.h"
// Override the stream
// Holder for the stream
typedef struct {
    uint8_t buffer[2048];
    size_t length;
    size_t current_position;
} memory_stream_t;


#define EI_SENSOR_AQ_STREAM         memory_stream_t
#include "sensor_aq.h"
#include "sensor_aq_mbedtls_hs256.h"




// fwrite function for the stream
size_t ms_fwrite(const void *ptr, size_t size, size_t count, memory_stream_t *stream) {
    memcpy(stream->buffer + stream->current_position, ptr, size * count);
    stream->current_position += size * count;

    if (stream->current_position > stream->length) {
        stream->length = stream->current_position;
    }

    return count;
}

// set current position in the stream
int ms_fseek(memory_stream_t *stream, long int offset, int origin) {
    if (origin == 0 /* SEEK_SET */) {
        stream->current_position = offset;
    }
    else if (origin == 1 /* SEEK_CUR */) {
        stream->current_position += offset;
    }
    else if (origin == 2 /* SEEK_END */) {
        stream->current_position = stream->length + offset;
    }
    // @todo: do a boundary check here
    return 0;
}

extern "C" void app_main()
{
    // The sensor format supports signing the data, set up a signing context
    sensor_aq_signing_ctx_t signing_ctx;

    // We'll use HMAC SHA256 signatures, which can be created through Mbed TLS
    // If you use a different crypto library you can implement your own context
    sensor_aq_mbedtls_hs256_ctx_t hs_ctx;
    // Set up the context, the last argument is the HMAC key
    sensor_aq_init_mbedtls_hs256_context(&signing_ctx, &hs_ctx, "fed53116f20684c067774ebf9e7bcbdc");

   // Set up the sensor acquisition basic context
    sensor_aq_ctx ctx =
    {
        { (unsigned char*)malloc(1024), 1024 },
        &signing_ctx,
        // custom fwrite / fseek
        &ms_fwrite,
        &ms_fseek,
        NULL // no time on this system
    };
 
    // Payload header
    sensor_aq_payload_info payload = {
        // Unique device ID (optional), set this to e.g. MAC address or device EUI **if** your device has one
        "ac:87:a3:0a:2d:1b",
        // Device type (required), use the same device type for similar devices
        "ESP32",
        // How often new data is sampled in ms. (100Hz = every 10 ms.)
        10,
        // The axes which you'll use. The units field needs to comply to SenML units (see https://www.iana.org/assignments/senml/senml.xhtml)
        { { "accX", "m/s2" }, { "accY", "m/s2" }, { "accZ", "m/s2" } }
    };

       // Place to write our data.
    memory_stream_t stream;
    stream.length = 0;
    stream.current_position = 0;    

    // Initialize the context, this verifies that all requirements are present
    // it also writes the initial CBOR structure
    int res;
    res = sensor_aq_init(&ctx, &payload, &stream);
    if (res != AQ_OK) {
        printf("sensor_aq_init failed (%d)\n", res);
        return;
    }

    // Periodically call `sensor_aq_add_data` (every 10 ms. in this example) to append data
    float values[][3] = {
        { -9.81, 0.03, 1.21 },
        { -9.83, 0.04, 1.28 },
        { -9.12, 0.03, 1.23 },
        { -9.14, 0.01, 1.25 }
    };
    for (size_t ix = 0; ix < sizeof(values) / sizeof(values[0]); ix++) {
        res = sensor_aq_add_data(&ctx, values[ix], 3);
        if (res != AQ_OK) {
            printf("sensor_aq_add_data failed (%d)\n", res);
            return;
        }
    }

    // When you're done call sensor_aq_finish - this will calculate the finalized signature and close the CBOR file
    res = sensor_aq_finish(&ctx);
    if (res != AQ_OK) {
        printf("sensor_aq_finish failed (%d)\n", res);
        return;
    }

    // For convenience we'll write the encoded file. You can throw this directly in http://cbor.me to decode
    printf("Encoded file:\n");

    #if 0

    for (size_t ix = 0; ix < len; ix++) {
        printf("%02x ", buffer[ix]);
    }
    printf("\n");

    // Verify if the file can be decoded:
    //
    // $ npm install cbor
    // $ node cbor.js
    //
    // If you changed the HMAC key make sure to update it in cbor.js too
    #endif
}
