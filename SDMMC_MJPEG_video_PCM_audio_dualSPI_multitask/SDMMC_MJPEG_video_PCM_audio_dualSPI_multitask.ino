#define AUDIO_FILENAME "/44100_u16le.pcm"
#define FPS 30
#define MJPEG_FILENAME "/220_30fps.mjpeg"
#define MJPEG_BUFFER_SIZE (220 * 176 * 2 / 4)
#define READ_BUFFER_SIZE 2048
/*
 * Connect the SD card to the following pins:
 *
 * SD Card | ESP32
 *    D2       12
 *    D3       13
 *    CMD      15
 *    VSS      GND
 *    VDD      3.3V
 *    CLK      14
 *    VSS      GND
 *    D0       2  (add 1K pull up after flashing)
 *    D1       4
 */

#include <WiFi.h>
#include <SD_MMC.h>
#include <driver/i2s.h>

#include <Arduino_ESP32SPI.h>
#include <Arduino_Display.h>
#define TFT_BL 22
#define TFT_BRIGHTNESS 128
// ST7789 Display
// Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(15 /* DC */, 12 /* CS */, 18 /* SCK */, 23 /* MOSI */, 19 /* MISO */);
// Arduino_ST7789 *gfx = new Arduino_ST7789(bus, -1 /* RST */, 2 /* rotation */, true /* IPS */, 240 /* width */, 240 /* height */, 0 /* col offset 1 */, 80 /* row offset 1 */);
// ILI9225 Display
Arduino_ESP32SPI *bus = new Arduino_ESP32SPI(27 /* DC */, 5 /* CS */, 18 /* SCK */, 23 /* MOSI */, 19 /* MISO */);
Arduino_ILI9225 *gfx = new Arduino_ILI9225(bus, 33 /* RST */, 1 /* rotation */);

#include "MjpegClass.h"
static MjpegClass mjpeg;

int next_frame = 0;
int skipped_frames = 0;
unsigned long total_read_audio = 0;
unsigned long total_play_audio = 0;
unsigned long total_read_video = 0;
unsigned long total_play_video = 0;
unsigned long total_remain = 0;
unsigned long start_ms, curr_ms, next_frame_ms;
void setup()
{
  WiFi.mode(WIFI_OFF);
  Serial.begin(115200);

  // Init Video
  gfx->begin();
  gfx->fillScreen(BLACK);

#ifdef TFT_BL
  ledcAttachPin(TFT_BL, 1);     // assign TFT_BL pin to channel 1
  ledcSetup(1, 12000, 8);       // 12 kHz PWM, 8-bit resolution
  ledcWrite(1, TFT_BRIGHTNESS); // brightness 0 - 255
#endif

  // Init SD card
  // if (!SD_MMC.begin()) /* 4-bit SD bus mode */
  if (!SD_MMC.begin("/sdcard", true)) /* 1-bit SD bus mode */
  {
    Serial.println(F("ERROR: SD card mount failed!"));
    gfx->println(F("ERROR: SD card mount failed!"));
  }
  else
  {
    // Init Audio
    i2s_config_t i2s_config_dac = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
        .sample_rate = 44100,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_PCM | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1, // lowest interrupt priority
        .dma_buf_count = 4,
        .dma_buf_len = 490,
        .use_apll = false,
    };
    Serial.printf("%p\n", &i2s_config_dac);
    if (i2s_driver_install(I2S_NUM_0, &i2s_config_dac, 0, NULL) != ESP_OK)
    {
      Serial.println(F("ERROR: Unable to install I2S drives!"));
      gfx->println(F("ERROR: Unable to install I2S drives!"));
    }
    else
    {
      i2s_set_pin((i2s_port_t)0, NULL);
      i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN);
      i2s_zero_dma_buffer((i2s_port_t)0);

      File aFile = SD_MMC.open(AUDIO_FILENAME);
      if (!aFile || aFile.isDirectory())
      {
        Serial.println(F("ERROR: Failed to open " AUDIO_FILENAME " file for reading!"));
        gfx->println(F("ERROR: Failed to open " AUDIO_FILENAME " file for reading!"));
      }
      else
      {
        File vFile = SD_MMC.open(MJPEG_FILENAME);
        if (!vFile || vFile.isDirectory())
        {
          Serial.println(F("ERROR: Failed to open " MJPEG_FILENAME " file for reading"));
          gfx->println(F("ERROR: Failed to open " MJPEG_FILENAME " file for reading"));
        }
        else
        {
          uint8_t *aBuf = (uint8_t *)malloc(2940);
          if (!aBuf)
          {
            Serial.println(F("aBuf malloc failed!"));
          }
          else
          {
            uint8_t *mjpeg_buf = (uint8_t *)malloc(MJPEG_BUFFER_SIZE);
            if (!mjpeg_buf)
            {
              Serial.println(F("mjpeg_buf malloc failed!"));
            }
            else
            {
              Serial.println(F("PCM audio MJPEG video start"));
              start_ms = millis();
              curr_ms = millis();
              mjpeg.setup(vFile, mjpeg_buf, gfx, true);
              next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
              while (vFile.available() && aFile.available())
              {
                // Read audio
                aFile.read(aBuf, 2940);
                total_read_audio += millis() - curr_ms;
                curr_ms = millis();

                // Play audio
                i2s_write_bytes((i2s_port_t)0, (char *)aBuf, 980, 0);
                i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 980), 980, 0);
                i2s_write_bytes((i2s_port_t)0, (char *)(aBuf + 1960), 980, 0);
                total_play_audio += millis() - curr_ms;
                curr_ms = millis();

                // Read video
                mjpeg.readMjpegBuf();
                total_read_video += millis() - curr_ms;
                curr_ms = millis();

                if (millis() < next_frame_ms) // check show frame or skip frame
                {
                  // Play video
                  mjpeg.drawJpg();
                  total_play_video += millis() - curr_ms;

                  int remain_ms = next_frame_ms - millis();
                  total_remain += remain_ms;
                  if (remain_ms > 0)
                  {
                    delay(remain_ms);
                  }
                }
                else
                {
                  ++skipped_frames;
                  Serial.println(F("Skip frame"));
                }

                curr_ms = millis();
                next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
              }
              int time_used = millis() - start_ms;
              Serial.println(F("PCM audio RGB565 video end"));
              vFile.close();
              aFile.close();
              int played_frames = next_frame - 1 - skipped_frames;
              float fps = 1000.0 * played_frames / time_used;
              Serial.printf("Played frame: %d\n", played_frames);
              Serial.printf("Skipped frames: %d (%0.1f %%)\n", skipped_frames, 100.0 * skipped_frames / played_frames);
              Serial.printf("Time used: %d ms\n", time_used);
              Serial.printf("Expected FPS: %d\n", FPS);
              Serial.printf("Actual FPS: %0.1f\n", fps);
              Serial.printf("SDMMC read PCM: %d ms (%0.1f %%)\n", total_read_audio, 100.0 * total_read_audio / time_used);
              Serial.printf("Play audio: %d ms (%0.1f %%)\n", total_play_audio, 100.0 * total_play_audio / time_used);
              Serial.printf("SDMMC read MJPEG: %d ms (%0.1f %%)\n", total_read_video, 100.0 * total_read_video / time_used);
              Serial.printf("Play video: %d ms (%0.1f %%)\n", total_play_video, 100.0 * total_play_video / time_used);
              Serial.printf("Remain: %d ms (%0.1f %%)\n", total_remain, 100.0 * total_remain / time_used);

              gfx->setCursor(0, 0);
              gfx->setTextColor(WHITE, BLACK);
              gfx->printf("Played frame: %d\n", played_frames);
              gfx->printf("Skipped frames: %d (%0.1f %%)\n", skipped_frames, 100.0 * skipped_frames / played_frames);
              gfx->printf("Time used: %d ms\n", time_used);
              gfx->printf("Expected FPS: %d\n", FPS);
              gfx->printf("Actual FPS: %0.1f\n", fps);
              gfx->printf("SDMMC read PCM: %d ms (%0.1f %%)\n", total_read_audio, 100.0 * total_read_audio / time_used);
              gfx->printf("Play audio: %d ms (%0.1f %%)\n", total_play_audio, 100.0 * total_play_audio / time_used);
              gfx->printf("SDMMC read MJPEG: %d ms (%0.1f %%)\n", total_read_video, 100.0 * total_read_video / time_used);
              gfx->printf("Play video: %d ms (%0.1f %%)\n", total_play_video, 100.0 * total_play_video / time_used);
              gfx->printf("Remain: %d ms (%0.1f %%)\n", total_remain, 100.0 * total_remain / time_used);

              i2s_driver_uninstall((i2s_port_t)0); //stop & destroy i2s driver
              // avoid unexpected output at audio pins
              pinMode(25, OUTPUT);
              digitalWrite(25, LOW);
              pinMode(26, OUTPUT);
              digitalWrite(26, LOW);
            }
          }
        }
      }
    }
  }
#ifdef TFT_BL
  delay(60000);
  ledcDetachPin(TFT_BL);
#endif
}

void loop()
{
}
