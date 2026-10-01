// ESPager by Hardcore Corey Harding
/*=====================================================================
  GSC encoder for ESP32
  // Based on code by unsynchronized (https://github.com/unsynchronized/gr-mixalot)
  // gr-mixalot License: "./3RD_PARTY_LICENSE/license_gr-mixalot.md" (https://github.com/unsynchronized/gr-mixalot/blob/main/COPYING)
  https://www.sigidwiki.com/images/5/54/Guide_to_Golay.pdf
=====================================================================*/

#ifndef GSC_H
#define GSC_H

/*--------------------------------------------------------------------
  Standard & Arduino includes
--------------------------------------------------------------------*/
#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

/*--------------------------------------------------------------------
  Configuration (compile‑time constants)
--------------------------------------------------------------------*/
#define GSC_OUTPUT_BYTES 4096          // raw output buffer size (bytes)

// Logic levels expected by the receiver – change only if your hardware
// uses opposite polarity.
#define GSC_ONE_LEVEL   HIGH
#define GSC_ZERO_LEVEL  LOW
#define GSC_IDLE_LEVEL  HIGH

// Fixed transmission speed (bits per second).  The rate never changes
// at runtime, so we keep it as a compile‑time constant.
#define GSC_BAUD 600
constexpr uint32_t BIT_PERIOD_US = (1'000'000UL / GSC_BAUD); // µs per bit

#ifndef GSC_MAX_MESSAGE_LENGTH
#define GSC_MAX_MESSAGE_LENGTH 256   // max characters per transmitted message
#endif

/*--------------------------------------------------------------------
  Public types
--------------------------------------------------------------------*/
enum GSCMessageType {
  GSC_MESSAGE_NUMERIC = 0,
  GSC_MESSAGE_ALPHA   = 1
};

/* Simple FIFO‑style bit buffer used by every encoder routine */
struct GSCBitBuffer {
  uint8_t *data;           // raw byte array
  size_t   capacityBits;   // total capacity (in bits)
  size_t   lengthBits;     // number of bits already written
};

/*--------------------------------------------------------------------
  Global buffer (shared by all transmissions)
--------------------------------------------------------------------*/
static uint8_t   gscOutput[GSC_OUTPUT_BYTES];
static GSCBitBuffer gscBuffer = {
    gscOutput,
    GSC_OUTPUT_BYTES * 8,
    0
};

/*--------------------------------------------------------------------
  Bit‑buffer helpers
--------------------------------------------------------------------*/
static inline void gscClearBuffer(GSCBitBuffer *buf) {
  if (!buf || !buf->data) return;
  memset(buf->data, 0, (buf->capacityBits + 7) / 8);
  buf->lengthBits = 0;
}

static inline bool gscWriteBit(GSCBitBuffer *buf, bool value) {
  if (!buf || !buf->data || buf->lengthBits >= buf->capacityBits) return false;

  const size_t byteIdx = buf->lengthBits / 8;
  const uint8_t bitIdx = 7 - (buf->lengthBits % 8);
  const uint8_t mask   = static_cast<uint8_t>(1U << bitIdx);

  if (value) buf->data[byteIdx] |= mask;
  else       buf->data[byteIdx] &= static_cast<uint8_t>(~mask);

  ++buf->lengthBits;
  return true;
}

static inline bool gscReadBit(const GSCBitBuffer *buf, size_t idx) {
  if (!buf || !buf->data || idx >= buf->lengthBits) return false;
  const size_t byteIdx = idx / 8;
  const uint8_t  bit    = 7 - (idx % 8);
  return (buf->data[byteIdx] & (1U << bit)) != 0;
}

/*--------------------------------------------------------------------
  Golay (23,12) encoder
--------------------------------------------------------------------*/
static inline uint32_t gscGolayRemainder(uint32_t v) {
  const uint32_t gen = 0x0C75U;               // generator polynomial g(x)
  uint32_t work = (v & 0xFFFU) << 11;        // shift data to MSB side

  for (int b = 22; b >= 11; --b)             // polynomial long division
    if (work & (1UL << b))
      work ^= gen << (b - 11);

  return work & 0x7FFU;                       // 11‑bit remainder
}

static inline uint32_t gscGolayEncode(uint32_t data) {
  data &= 0xFFFU;                             // keep only 12 bits
  return (data << 11) | gscGolayRemainder(data);
}

/*--------------------------------------------------------------------
  BCH (15,7) encoder
--------------------------------------------------------------------*/
static inline uint16_t gscBchEncode(uint8_t d) {
  const uint16_t gen = 0x01D1U;
  const uint16_t info = static_cast<uint16_t>(d & 0x7FU);
  uint16_t work = info << 8;                  // move data to top of 15‑bit word

  for (int b = 14; b >= 8; --b)               // polynomial division
    if (work & (1U << b))
      work ^= gen << (b - 8);

  return static_cast<uint16_t>((info << 8) | (work & 0xFFU));
}

/*--------------------------------------------------------------------
  Generic bit‑writing helpers
--------------------------------------------------------------------*/
static inline bool gscWriteComma(GSCBitBuffer *buf,
                                 unsigned int count,
                                 bool startPolarity) {
  bool bit = startPolarity;
  for (unsigned int i = 0; i < count; ++i) {
    if (!gscWriteBit(buf, bit)) return false;
    bit = !bit;
  }
  return true;
}

/*
 * Write `bitCount` bits of `value` LSB‑first, duplicating each bit.
 * `invert` flips every bit before duplication (used for polarity functionCodes).
 */
static inline bool gscWriteDuplicatedBits(GSCBitBuffer *buf,
                                          uint32_t value,
                                          unsigned int bitCount,
                                          bool invert = false) {
  for (unsigned int i = 0; i < bitCount; ++i) {
    bool b = (value & (1UL << i)) != 0;
    if (invert) b = !b;
    if (!gscWriteBit(buf, b) || !gscWriteBit(buf, b)) return false;
  }
  return true;
}

/*--------------------------------------------------------------------
  Preamble handling (10 predefined values)
--------------------------------------------------------------------*/
static const uint32_t gscPreambleValues[10] = {
  2030, 1628, 3198, 647, 191,
  3315, 1949, 2540, 1560, 2335
};

static inline bool gscWritePreamble(GSCBitBuffer *buf,
                                    unsigned int idx) {
  if (idx >= 10) return false;

  const uint32_t data  = gscPreambleValues[idx];
  const uint32_t golay = gscGolayEncode(data);

  if (!gscWriteComma(buf, 28, (data & 1U) != 0)) return false;

  for (unsigned int i = 0; i < 18; ++i) {
    if (!gscWriteDuplicatedBits(buf, data, 12))  return false;
    if (!gscWriteDuplicatedBits(buf, golay, 11)) return false;
  }
  return true;
}

/*--------------------------------------------------------------------
  Start code (fixed value 713)
--------------------------------------------------------------------*/
static inline bool gscWriteStartCode(GSCBitBuffer *buf) {
  const uint32_t data  = 713;
  const uint32_t golay = gscGolayEncode(data);

  if (!gscWriteComma(buf, 28, true))                    return false;
  if (!gscWriteDuplicatedBits(buf, data, 12))           return false;
  if (!gscWriteDuplicatedBits(buf, golay, 11))          return false;
  if (!gscWriteBit(buf, true))                          return false; // separator

  if (!gscWriteDuplicatedBits(buf, data, 12, true))    return false;
  if (!gscWriteDuplicatedBits(buf, golay, 11, true))   return false;

  return true;
}

/*--------------------------------------------------------------------
  Address handling – 50 possible word‑1 values + a 12‑bit word‑2
--------------------------------------------------------------------*/
static const uint32_t gscWord1Values[50] = {
  721, 2731, 2952, 1387, 1578, 1708, 2650, 1747, 2580, 1376,
  2692, 696, 1667, 3800, 3552, 3424, 1384, 3595, 876, 3124,
  2285, 2608, 899, 3684, 3129, 2124, 1287, 2616, 1647, 3216,
  375, 1232, 2824, 1840, 408, 3127, 3387, 882, 3468, 3267,
  1575, 3463, 3152, 2572, 1252, 2592, 1552, 835, 1440, 160
};

static inline bool gscWriteAddress(GSCBitBuffer *buf,
                                   unsigned int w1,
                                   unsigned int w2,
                                   uint8_t functionCode) {
  if (!buf || !buf->data ||
      w1 >= 100 || w2 >= 4096 || functionCode > 3) return false;

  // Resolve “high‑half” (+50) rule.
  bool highHalf = (w1 >= 50);
  if (highHalf) w1 -= 50;

  // Polarity bits from function code.
  const bool invertW1 = (functionCode & 0x02U) != 0;
  const bool invertW2 = (functionCode & 0x01U) != 0;

  const uint32_t data1  = gscWord1Values[w1];
  const uint32_t data2  = w2 & 0xFFFU;
  const uint32_t golay1 = gscGolayEncode(data1);
  const uint32_t golay2 = gscGolayEncode(data2);

  const bool firstBit1 = ((data1 & 1U) != 0) ^ invertW1;
  const bool firstBit2 = ((data2 & 1U) != 0) ^ invertW2;

  // Word‑1
  if (!gscWriteComma(buf, 28, firstBit1))                     return false;
  if (!gscWriteDuplicatedBits(buf, data1, 12, invertW1))      return false;
  if (!gscWriteDuplicatedBits(buf, golay1, 11, invertW1))     return false;

  // Separator (opposite of first bit of Word‑2)
  if (!gscWriteBit(buf, !firstBit2))                          return false;

  // Word‑2
  if (!gscWriteDuplicatedBits(buf, data2, 12, invertW2))      return false;
  if (!gscWriteDuplicatedBits(buf, golay2, 11, invertW2))     return false;

  return true;
}

/*--------------------------------------------------------------------
  Pager ID → word1 / word2 / preamble conversion
--------------------------------------------------------------------*/
static inline bool gscCalculatePagerID(uint32_t code,
                                       unsigned int *w1,
                                       unsigned int *w2,
                                       unsigned int *preambleIdx) {
  if (!w1 || !w2 || !preambleIdx || code > 999999U) return false;

  // Split decimal code into its six components
  uint32_t r = code;
  const unsigned int i  = r / 100000U; r -= i * 100000U;
  const unsigned int g1 = r / 10000U;  r -= g1 * 10000U;
  const unsigned int g0 = r / 1000U;   r -= g0 * 1000U;
  const unsigned int a2 = r / 100U;    r -= a2 * 100U;
  const unsigned int a1 = r / 10U;     r -= a1 * 10U;
  const unsigned int a0 = r;

  *preambleIdx = (i + g0) % 10U;

  // Low three digits doubled (as defined by the spec)
  unsigned int ap = (code % 1000U) * 2U;
  const unsigned int ap3 = ap / 1000U; ap -= ap3 * 1000U;
  const unsigned int ap2 = ap / 100U;  ap -= ap2 * 100U;
  const unsigned int ap1 = ap / 10U;   ap -= ap1 * 10U;
  const unsigned int ap0 = ap;

  const unsigned int b1b0 = (ap1 * 10U + ap0) / 2U;
  const unsigned int b3b2 = ap3 * 10U + ap2;
  const unsigned int g1g0 = g1 * 10U + g0;

  // Apply “+50” rule for the high half of word‑1
  if (g1g0 >= 50U) {
    *w1 = g1g0 - 50U;
    *w2 = b3b2 * 100U + b1b0 + 50U;
  } else {
    *w1 = g1g0;
    *w2 = b3b2 * 100U + b1b0;
  }

  // ----- Illegal address checks (as in the reference implementation) -----
  const unsigned int a = a2 * 100U + a1 * 10U + a0;
  static const unsigned int illegalLow[16] = {
    0, 25, 51, 103, 206, 340, 363, 412,
    445, 530, 642, 726, 782, 810, 825, 877
  };
  static const unsigned int illegalHigh[7] = {
    0, 292, 425, 584, 631, 841, 851
  };

  if (g1g0 < 50U) {
    for (unsigned int n = 0; n < 16; ++n)
      if (a == illegalLow[n]) return false;
  } else {
    for (unsigned int n = 0; n < 7; ++n)
      if (a == illegalHigh[n]) return false;
  }
  return true;
}

/*--------------------------------------------------------------------
  Character encoding – numeric & alphabetic
--------------------------------------------------------------------*/
static inline bool gscEncodeAlphaCharacter(uint8_t ch, uint8_t *out) {
  if (!out) return false;

  // Force upper‑case (protocol defines only upper‑case)
  if (ch >= 'a' && ch <= 'z')
    ch = static_cast<uint8_t>(ch - 'a' + 'A');

  // Special escapes
  if (ch == '\r' || ch == '\n') { *out = 0x3C; return true; }
  if (ch == '{')               { *out = 0x3B; return true; }
  if (ch == '~')               { *out = 0x3D; return true; }
  if (ch == '\\')              { *out = 0x20; return true; }

  // Anything outside the supported range maps to space (0x20)
  if (ch < 0x20 || ch > 0x5D) { *out = 0x20; return true; }

  *out = static_cast<uint8_t>(ch - 0x20);
  return true;
}

static inline bool gscEncodeNumericCharacter(uint8_t ch, uint8_t *out) {
  if (!out) return false;

  if (ch >= '0' && ch <= '9') { *out = static_cast<uint8_t>(ch - '0'); return true; }

  switch (ch) {
    case 'U': case 'u': *out = 11; return true;
    case ' ':           *out = 12; return true;
    case '-':           *out = 13; return true;
    case '=':           *out = 14; return true;
    case 'E': case 'e': *out = 15; return true;
    default:            return false;
  }
}

/*--------------------------------------------------------------------
  BCH‑encoded data block (numeric or alphanumeric)
--------------------------------------------------------------------*/
static inline bool gscWriteEncodedDataBlock(GSCBitBuffer *buf,
                                           const uint8_t data[8]) {
  uint16_t encoded[8];
  for (unsigned int i = 0; i < 8; ++i) encoded[i] = gscBchEncode(data[i]);

  // First comma bit = inverse of the MSB of the first encoded word
  if (!gscWriteBit(buf, (encoded[0] & 0x4000U) == 0)) return false;

  // Transmit column‑wise, MSB first
  for (int bit = 14; bit >= 0; --bit) {
    uint16_t mask = static_cast<uint16_t>(1U << bit);
    for (unsigned int w = 0; w < 8; ++w)
      if (!gscWriteBit(buf, (encoded[w] & mask) != 0)) return false;
  }
  return true;
}

/*--------------------------------------------------------------------
  Alpha (8‑character) data block
--------------------------------------------------------------------*/
static inline bool gscWriteAlphaDataBlock(GSCBitBuffer *buf,
                                          const uint8_t vals[8],
                                          bool continueBit) {
  uint8_t data[8];

  // Pack the 8 7‑bit values into seven 7‑bit words (spec‑defined)
  data[0] = static_cast<uint8_t>((vals[0] | (vals[1] << 6)) & 0x7F);
  data[1] = static_cast<uint8_t>(((vals[1] >> 1) | (vals[2] << 5)) & 0x7F);
  data[2] = static_cast<uint8_t>(((vals[2] >> 2) | (vals[3] << 4)) & 0x7F);
  data[3] = static_cast<uint8_t>(((vals[3] >> 3) | (vals[4] << 3)) & 0x7F);
  data[4] = static_cast<uint8_t>(((vals[4] >> 4) | (vals[5] << 2)) & 0x7F);
  data[5] = static_cast<uint8_t>(((vals[5] >> 5) | (vals[6] << 1)) & 0x7F);
  data[6] = static_cast<uint8_t>(vals[7] & 0x3F);
  if (continueBit) data[6] |= 0x40;               // continuation flag

  // Checksum = sum of the seven information words
  uint16_t checksum = 0;
  for (unsigned int i = 0; i < 7; ++i) checksum += data[i];
  data[7] = static_cast<uint8_t>(checksum & 0x7F);

  // Reverse bit order of each 7‑bit word (protocol requirement)
  for (unsigned int i = 0; i < 8; ++i) {
    uint8_t rev = 0;
    for (unsigned int b = 0; b < 7; ++b)
      if (data[i] & (1U << b)) rev |= static_cast<uint8_t>(1U << (6 - b));
    data[i] = rev;
  }

  return gscWriteEncodedDataBlock(buf, data);
}

/*--------------------------------------------------------------------
  Numeric (12‑character) data block
--------------------------------------------------------------------*/
static inline bool gscWriteNumericDataBlock(GSCBitBuffer *buf,
                                            const uint8_t vals[12],
                                            bool continueBit) {
  if (!buf || !vals) return false;

  uint8_t data[8] = {};

  // Pack 12×4‑bit nibbles (=48 bits) into seven 7‑bit words
  uint64_t payload = 0;
  for (unsigned int i = 0; i < 12; ++i)
    payload |= static_cast<uint64_t>(vals[i] & 0x0F) << (i * 4);

  for (unsigned int i = 0; i < 7; ++i)
    data[i] = static_cast<uint8_t>((payload >> (i * 7)) & 0x7F);

  // Continuation flag is the upper bit of data[6]
  data[6] &= 0x3F;
  if (continueBit) data[6] |= 0x40;

  // Checksum over the seven information words
  uint16_t checksum = 0;
  for (unsigned int i = 0; i < 7; ++i) checksum += data[i];
  data[7] = static_cast<uint8_t>(checksum & 0x7F);

  // Reverse bit order of each word (same rule as alphanumeric)
  for (unsigned int i = 0; i < 8; ++i) {
    uint8_t rev = 0;
    for (unsigned int b = 0; b < 7; ++b)
      if (data[i] & (1U << b)) rev |= static_cast<uint8_t>(1U << (6 - b));
    data[i] = rev;
  }

  return gscWriteEncodedDataBlock(buf, data);
}

/*--------------------------------------------------------------------
  Message handling – split into blocks, encode, and write
--------------------------------------------------------------------*/
static inline bool gscWriteMessage(GSCBitBuffer *buf,
                                   const char *msg,
                                   GSCMessageType type) {
  if (!buf || !msg) return false;

  const size_t len = strlen(msg);
  if (len > GSC_MAX_MESSAGE_LENGTH) return false;

  const size_t perBlock = (type == GSC_MESSAGE_NUMERIC) ? 12 : 8;
  const size_t blockCnt = (len == 0) ? 1 : (len + perBlock - 1) / perBlock;

  for (size_t blk = 0; blk < blockCnt; ++blk) {
    const bool cont = (blk + 1) < blockCnt;          // continuation flag

    if (type == GSC_MESSAGE_NUMERIC) {
      uint8_t vals[12];
      for (unsigned int i = 0; i < 12; ++i) vals[i] = 12; // pad with space

      for (unsigned int i = 0; i < 12; ++i) {
        const size_t idx = blk * 12 + i;
        if (idx >= len) break;
        if (!gscEncodeNumericCharacter(static_cast<uint8_t>(msg[idx]), &vals[i]))
          return false;
      }
      if (!gscWriteNumericDataBlock(buf, vals, cont)) return false;
    } else { // alphanumeric
      uint8_t vals[8];
      for (unsigned int i = 0; i < 8; ++i) vals[i] = 0x3E; // pad with 0x3E

      for (unsigned int i = 0; i < 8; ++i) {
        const size_t idx = blk * 8 + i;
        if (idx >= len) break;
        if (!gscEncodeAlphaCharacter(static_cast<uint8_t>(msg[idx]), &vals[i]))
          return false;
      }
      if (!gscWriteAlphaDataBlock(buf, vals, cont)) return false;
    }
  }
  return true;
}

/*--------------------------------------------------------------------
  Complete GSC encoder – entry point used by the user
--------------------------------------------------------------------*/
static inline bool gscEncode(GSCBitBuffer *buf,
                             uint32_t capcode,
                             const char *msg,
                             GSCMessageType type,
                             uint8_t GSCFunctionCode) {
  if (!buf || !buf->data || !msg || GSCFunctionCode > 3) return false;

  unsigned int w1 = 0, w2 = 0, preIdx = 0;
  if (!gscCalculatePagerID(capcode, &w1, &w2, &preIdx)) return false;

  gscClearBuffer(buf);

  if (!gscWritePreamble(buf, preIdx))       return false;
  if (!gscWriteStartCode(buf))              return false;
  if (!gscWriteAddress(buf, w1, w2, GSCFunctionCode)) return false;
  if (!gscWriteMessage(buf, msg, type))    return false;

  // Final idle/termination pattern (121 bytes of alternating bits)
  if (!gscWriteComma(buf, 121 * 8, true))  return false;

  return true;
}

/*--------------------------------------------------------------------
  Private helper – bit‑bang the already‑encoded buffer
--------------------------------------------------------------------*/
static void transmitBuffer() {
  digitalWrite(TX_PIN, GSC_IDLE_LEVEL);   // Ensure line is idle first
  uint32_t nextBitTime = micros();        // Time when first bit starts

  for (size_t i = 0; i < gscBuffer.lengthBits; ++i) {
    bool bit = gscReadBit(&gscBuffer, i);  // Retrieve next encoded bit

    digitalWrite(TX_PIN, bit ? GSC_ONE_LEVEL : GSC_ZERO_LEVEL);

    // Busy‑wait until the exact bit period has elapsed.
    while ((uint32_t)(micros() - nextBitTime) < BIT_PERIOD_US) { }
    nextBitTime += BIT_PERIOD_US;           // Schedule start of next bit
  }

  digitalWrite(TX_PIN, GSC_IDLE_LEVEL);   // Return to idle after frame
}

/*--------------------------------------------------------------------
  Public API – encode then transmit
      // Based on gr-mixalot by unsynchronized (https://github.com/unsynchronized/gr-mixalot)
      // gr-mixalot License: "license_gr-mixalot.h" //(https://github.com/unsynchronized/gr-mixalot/blob/main/COPYING)
      // Also adapted from the Motorola Guide to Golay PDF: (https://www.sigidwiki.com/images/5/54/Guide_to_Golay.pdf)
--------------------------------------------------------------------*/
bool sendGSCFrame(uint32_t        capcode,
                  const char *    message,
                  GSCMessageType  msgType,
                  uint8_t         funcCode) {
  gscClearBuffer(&gscBuffer);                 // Start with a clean buffer

  // Encode the payload; abort if encoding fails.
  if (!gscEncode(&gscBuffer, capcode, message, msgType, funcCode))
    return false;

  transmitBuffer();                           // Send the encoded bits
  return true;
}

/*=====================================================================
  Note: all functions are `static inline` so that inclusion in multiple
  translation units does not cause linkage errors.  The buffer size,
  timing constants, and pin definitions are left for the user to set
  (e.g. `#define TX_PIN 7` before including this header).
=====================================================================*/

#endif // GSC_H