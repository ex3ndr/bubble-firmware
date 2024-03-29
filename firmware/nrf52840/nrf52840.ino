#define FDK_LOG_LEVEL FDKDebug

#include <ArduinoBLE.h>
#include <mic.h>
extern "C"{
  #include "g72x.h"
}
// #include <g711.с>
// #include <opus_config.h>
// #include <opus.h>
// #include <MemoryFree.h>
// #include <AACEncoderFDK.h>

// using namespace aac_fdk;

// Settings
#define SAMPLE_RATE 16000
#define DOWNSAMPLE 1
#define BUFFER_MIC 640
#define BUFFER_RING 16000
#define BUFFER_OUTPUT 512

// Codec settings
#define PACKET_SAMPLES 160
#define CODEC_MAX_PACKET_SIZE 512
#define SCALE 32

// Microphone and ring buffer
mic_config_t mic_config{ .channel_cnt = 1, .sampling_rate = SAMPLE_RATE, .buf_size = BUFFER_MIC, .debug_pin = LED_BUILTIN };
NRF52840_ADC_Class Mic(&mic_config);
uint16_t recording_buf[BUFFER_RING];
volatile uint32_t recording_idx = 0;
volatile uint32_t read_idx = 0;
volatile bool recording = false;

// Encoder
int16_t codec_input_buffer[PACKET_SAMPLES];
byte codec_output_buffer[CODEC_MAX_PACKET_SIZE];
// __ALIGN(4)
// static uint8_t m_opus_encoder[24540];
// unsigned char opus_output[OPUS_MAX_PACKET_SIZE];
// static OpusEncoder *const encoder = (OpusEncoder *)m_opus_encoder;
// opus_int16 opus_buffer[PACKET_SAMPLES];

// int16_t codec_input_buffer[PACKET_SAMPLES];
// static void dataCallback(uint8_t *aac_data, size_t len) {
//     Serial.print("AAC generated with ");
//     Serial.print(len);
//     Serial.println(" bytes");
// }

// AACEncoderFDK aac(dataCallback);
// AudioInfo info;

// Connection State
bool isConnected = false;
BLEService audioService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLECharacteristic audioCharacteristic("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify, CODEC_MAX_PACKET_SIZE);

//
// Device Setup
//

void setup() {
  // Start serial
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  // Serial.println(freeMemory());

  // Enable microphone
  Mic.set_callback(audio_rec_callback);
  if (!Mic.begin()) {
    Serial.println("Mic initialization failed");
    while (1)
      ;
  }
  Serial.println("Mic initialization done.");
    // Serial.println(freeMemory());

  // Enable BLE
  if (!BLE.begin()) {
    Serial.println("Starting Bluetooth® Low Energy module failed!");
    while (1)
      ;
  }
  BLE.setLocalName("Arduino");
  BLE.setAdvertisedService(audioService);
  audioService.addCharacteristic(audioCharacteristic);
  BLE.addService(audioService);
  BLE.advertise();
  // Serial.println(freeMemory());

  // AAC
  // info.channels = 1;
  // info.sample_rate = SAMPLE_RATE;
  // aac.setOutputBufferSize(CODEC_MAX_PACKET_SIZE);
  // aac.setAudioObjectType(2);  // AAC low complexity
  // aac.setVariableBitrateMode(2); // low variable bitrate
  // aac.begin(info);

  // Ready
  Serial.print("Device Address: ");
  Serial.println(BLE.address());
  Serial.println("BLE Audio Recorder");
  // Serial.println(freeMemory());
}

void loop() {

  // Check if connected
  BLEDevice central = BLE.central();
  if (central && !isConnected) {
    Serial.print("Connected to central: ");
    Serial.println(central.address());
    isConnected = true;
    recording = true;
    Serial.println("Recording started");
    Serial.println("Type 'stop' to stop recording");
  }

  // Start/Stop recording
  if (Serial.available()) {
    String resp = Serial.readStringUntil('\n');
    if (resp == "rec" && !recording) {
      Serial.println("Recording started");

      // // Create Encoder
      // Serial.println(opus_encoder_get_size(1));
      // int error = opus_encoder_init(encoder, SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP);
      // if (error != OPUS_OK) {
      //   Serial.println("Opus creation failed");
      //   Serial.println(opus_strerror(error));
      //   // while (1)
      //   //   ;
      // }
      // error = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(OPUS_BITRATE));
      // if (error != OPUS_OK) {
      //   Serial.println("Opus creation failed (2)");
      //   Serial.println(opus_strerror(error));
      //   // while (1)
      //   //   ;
      // }
      // error = opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(1));
      // if (error != OPUS_OK) {
      //   Serial.println("Opus creation failed (3)");
      //   Serial.println(opus_strerror(error));
      //   // while (1)
      //   //   ;
      // }

      recording = true;

    } else if (resp == "stop" && recording) {
      Serial.println("Recording stopped");

      // Destroy encoder
      // opus_encoder_destroy(encoder);
      recording = false;
    }
  }

  // Send packet
  if (recording) {
    uint32_t available_samples = (recording_idx + BUFFER_RING - read_idx) % BUFFER_RING;
    if (available_samples >= PACKET_SAMPLES) {
      // Serial.println("Encoding");

      // Read target buffer
      // Serial.println("####");
      // Serial.println(recording_buf[(read_idx) % BUFFER_RING]);
      // Serial.println((int16_t)recording_buf[(read_idx) % BUFFER_RING]);
      // Serial.println(linear2ulaw(recording_buf[(read_idx) % BUFFER_RING] * SCALE));
      // Serial.println("####");
      for (int i = 0; i < PACKET_SAMPLES; i++) {
        // codec_output_buffer[i * 2] = recording_buf[(read_idx + i) % BUFFER_RING] & 0xff;
        // codec_output_buffer[i * 2 + 1] = recording_buf[(read_idx + i) % BUFFER_RING] >> 8;
        codec_output_buffer[i] = linear2ulaw(recording_buf[(read_idx + i) % BUFFER_RING] * SCALE);
        // codec_output_buffer[i] = recording_buf[(read_idx + i) % BUFFER_RING] >> 8;
      }
      read_idx = (read_idx + PACKET_SAMPLES) % BUFFER_RING;

      // Encode
      // aac.write((uint8_t *)&codec_input_buffer, BUFFER_SIZE * 2);
      // Serial.println("Encoding2");
      // int encodedSize = opus_encode(encoder, opus_buffer, PACKET_SAMPLES, opus_output, OPUS_MAX_PACKET_SIZE);
      // Serial.println("Encoding3");
      // if (encodedSize < 0) {
      //   Serial.println("Opus encoding failed");
      //   Serial.println(opus_strerror(encodedSize));
      // }

      // Send buffer
      audioCharacteristic.writeValue(codec_output_buffer, PACKET_SAMPLES);
    }
  }
}

static void audio_rec_callback(uint16_t *buf, uint32_t buf_len) {
  if (recording) {
    for (uint32_t i = 0; i < buf_len; i += DOWNSAMPLE) {
      // int16_t v = (int16_t)(((int32_t)buf[i]) - 32768);
      // Serial.print("U Law:");
      // Serial.print(buf[i]);
      uint16_t v = buf[i];

      // Append to ring buffer
      recording_buf[recording_idx] = v;
      recording_idx = (recording_idx + 1) % BUFFER_RING;
    }
  }
}