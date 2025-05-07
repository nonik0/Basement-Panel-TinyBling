#include <tinyNeoPixel_static.h>
#include <Wire.h>

#include "Picopixel.h"

#define I2C_ADDRESS 0x13
#define LED_VIN_PIN 15
#define MATRIX_PIN 16
#define MATRIX_HEIGHT 8
#define MATRIX_WIDTH 40
#define MATRIX_PIXELS (MATRIX_WIDTH * MATRIX_HEIGHT)
#define MAX_MESSAGE_SIZE 325
#define MIN_UPDATE_INTERVAL 20
#define MAX_UPDATE_INTERVAL 500
#define CHAR_GAP 1

byte matrixPixelData[MATRIX_PIXELS * 3];
tinyNeoPixel matrix = tinyNeoPixel(MATRIX_PIXELS, MATRIX_PIN, NEO_GRB, matrixPixelData);

const uint8_t ColorCount = 3;
const uint32_t Colors[ColorCount] = {
    matrix.Color(0xFF, 0x00, 0x00),
    matrix.Color(0xFF, 0x55, 0x00),
    matrix.Color(0xFF, 0x88, 0x00)};

char message[MAX_MESSAGE_SIZE];
int messageWidth;
int messageColor = 0;
int x = MATRIX_WIDTH;
int y = MATRIX_HEIGHT;
bool display = true;
unsigned long lastUpdate = 0;
unsigned long updateInterval = 40;

uint16_t getCharWidth(unsigned char c)
{
  uint16_t first = pgm_read_byte(&Picopixel.first);
  uint16_t last = pgm_read_byte(&Picopixel.last);
  uint8_t charWidth = 0;

  if ((c >= first) && (c <= last)) // Char present in this font?
  {
    GFXglyph *glyph = &(((GFXglyph *)pgm_read_ptr(&Picopixel.glyph))[c - first]);
    charWidth = pgm_read_byte(&glyph->xAdvance);
  }

  return charWidth;
}

int getTextWidth(const char *str)
{
  char c;
  uint16_t width = 0;

  while ((c = *str++))
  {
    width += getCharWidth(c) + CHAR_GAP;
  }

  if (width > 0)
  {
    width -= CHAR_GAP; // Remove the extra gap after the last character
  }

  return width;
}

void setMessage(const char *newMessage)
{
  strncpy(message, newMessage, MAX_MESSAGE_SIZE - 1);
  message[MAX_MESSAGE_SIZE - 1] = '\0';
  messageWidth = getTextWidth(message);
  x = MATRIX_WIDTH;
}

void receiveEvent(int bytesReceived)
{
  if (bytesReceived < 2)
    return;

  static char buffer[MAX_MESSAGE_SIZE];
  static int bufferIndex = 0;

  uint8_t command = Wire.read();
  if (command == 0x00)
  {
    bool state = Wire.read();
    display = state;
  }
  else if (command == 0x01)
  {
    // read chunk into buffer, discard extra bytes if past buffer size
    while (Wire.available())
    {
      uint8_t byte = Wire.read();
      if (bufferIndex < MAX_MESSAGE_SIZE - 1)
      {
        buffer[bufferIndex++] = byte;
      }
    }
    buffer[bufferIndex] = '\0';

    // last chunk (or buffer overflow)
    if (bufferIndex > 0 && (buffer[bufferIndex - 1] == '\n' || bufferIndex >= MAX_MESSAGE_SIZE - 1))
    {
      if (buffer[bufferIndex - 1] == '\n') {
        buffer[--bufferIndex] = '\0';
      }

      setMessage(buffer);
      bufferIndex = 0;
    }
  }
  else if (command == 0x02)
  {
    uint8_t scrollSpeed = Wire.read();
    updateInterval = map(constrain(scrollSpeed, 0, 100), 100, 0, MIN_UPDATE_INTERVAL, MAX_UPDATE_INTERVAL);
  }
}

void drawPixel(uint16_t x, uint16_t y, uint32_t color)
{
  if (x < MATRIX_WIDTH && y < MATRIX_HEIGHT)
  {
    uint16_t flippedX = MATRIX_WIDTH - 1 - x;
    uint16_t flippedY = MATRIX_HEIGHT - 1 - y;
    matrix.setPixelColor(flippedY * MATRIX_WIDTH + flippedX, color);
  }
}

void drawChar(uint16_t x, uint16_t y, char c, uint32_t color, uint8_t &glyphWidth)
{
  c -= (uint8_t)pgm_read_byte(&Picopixel.first);

  GFXglyph *glyph = &(((GFXglyph *)pgm_read_ptr(&Picopixel.glyph))[c]);
  uint8_t *bitmap = (uint8_t *)pgm_read_ptr(&Picopixel.bitmap);
  int count = 0;

  uint16_t bo = pgm_read_word(&glyph->bitmapOffset);
  uint8_t w = pgm_read_byte(&glyph->width);
  uint8_t h = pgm_read_byte(&glyph->height);
  int8_t xo = pgm_read_byte(&glyph->xOffset);
  int8_t yo = pgm_read_byte(&glyph->yOffset);
  uint8_t xx, yy, bits = 0, bit = 0;

  for (yy = 0; yy < h; yy++)
  {
    for (xx = 0; xx < w; xx++)
    {
      if (!(bit++ & 7))
      {
        bits = pgm_read_byte(&bitmap[bo++]);
      }
      if (bits & 0x80)
      {
        drawPixel(x + xo + xx, y + yo + yy, color);
        count++;
      }
      bits <<= 1;
    }
  }

  glyphWidth = w;
}

void drawString(int16_t x, int16_t y, const char *str, uint32_t color)
{
  while (*str && x < MATRIX_WIDTH)
  {
    uint8_t charWidth = 0;
    drawChar(x, y, *str, color, charWidth);
    x += charWidth + CHAR_GAP;
    str++;
  }
}

void setup()
{
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(receiveEvent);

  setMessage("Once upon a midnight dreary, while I pondered, weak and weary, over many a quaint and curious volume of forgotten lore. While I nodded, nearly napping, suddenly there came a tapping, as of some one gently rapping, rapping at my chamber door. \"Tis some visitor,\" I muttered, \"tapping at my chamber door. Only this and nothing more.\"");

  pinMode(LED_VIN_PIN, INPUT);
  pinMode(MATRIX_PIN, OUTPUT);
  matrix.begin();
  matrix.setBrightness(3);
  matrix.show();
}

void loop()
{
  if (millis() - lastUpdate < updateInterval)
  {
    return;
  }

  if (!display)
  {
    matrix.clear();
    matrix.show();
    delay(100);
    return;
  }

  lastUpdate = millis();

  matrix.fill(0);
  drawString(x, MATRIX_HEIGHT - 2, message, Colors[messageColor]);
  matrix.show();

  //if (--x < -messageWidth)
  if (--x < -messageWidth)
  {
    x = MATRIX_WIDTH + CHAR_GAP; // Reset position with a gap before restarting
    messageColor = (messageColor + 1) % ColorCount;
  }
}