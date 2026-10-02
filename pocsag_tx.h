// ESPager by Hardcore Corey Harding
/*-------------------------------------------------------------
   POCSAG encoder for ESP32
   // Based on rpitx by F5OEO (https://github.com/F5OEO/rpitx)
   // rpitx License: "./3RD_PARTY_LICENSE/license_rpitx.h" (https://github.com/F5OEO/rpitx/blob/master/LICENCE)
   // Also adapted from Arduino Nano reference code from:
   //   https://hackaday.io/project/183267-motolora-advisor
   // advisorradio.ino License: "./3RD_PARTY_LICENSE/license_advisorradio.h"
-------------------------------------------------------------*/

#include <Arduino.h>
#include <stdint.h>
#include <string.h>
#include <driver/gpio.h>

/*-------------------------------------------------------------------
   Protocol‑level constants
--------------------------------------------------------------------*/
#define SYNC               0x7CD215D8UL
#define IDLE               0x7A89C197UL
#define FRAME_SIZE         2
#define BATCH_SIZE         16
#define PREAMBLE_LENGTH    576
#define FLAG_ADDRESS       0x000000UL
#define FLAG_MESSAGE       0x100000UL
#define TEXT_BITS_PER_WORD 20
#define TEXT_BITS_PER_CHAR 7
#define CRC_BITS           10
#define NUMERIC_BITS_PER_WORD   20
#define NUMERIC_BITS_PER_DIGIT   4
#define NUMERIC_DIGITS_PER_WORD (NUMERIC_BITS_PER_WORD / NUMERIC_BITS_PER_DIGIT)

/*-------------------------------------------------------------------
   BCH‑CRC helpers – generate the 10‑bit CRC and overall parity
--------------------------------------------------------------------*/
static uint32_t crc(uint32_t msg) {
    const uint32_t poly = 0b11101101001U << 20;
    uint32_t reg = msg << CRC_BITS;
    for (int i = 0; i <= 20; ++i) {
        if ((reg >> (30 - i)) & 1U)
            reg ^= (poly >> i);
    }
    return reg & 0x3FFU;
}

static uint32_t parity(uint32_t x) {
    uint32_t p = 0;
    for (int i = 0; i < 32; ++i) {
        p ^= (x & 1U);
        x >>= 1;
    }
    return p;
}

static uint32_t encodeCodeword(uint32_t data) {
    uint32_t cw = (data << CRC_BITS) | crc(data);
    return (cw << 1) | parity(cw);
}

/*-------------------------------------------------------------------
   ASCII message encoding – packs 7‑bit characters into 20‑bit words
--------------------------------------------------------------------*/
static uint32_t encodeASCII(uint32_t offset,
                            const char *txt,
                            uint32_t *out) {
    uint32_t words = 0;
    uint32_t cur   = 0;
    uint32_t bits  = 0;
    uint32_t pos   = offset;

    while (*txt) {
        unsigned char c = static_cast<unsigned char>(*txt++);
        for (int i = 0; i < TEXT_BITS_PER_CHAR; ++i) {
            cur <<= 1;
            cur |= (c >> i) & 1U;
            bits++;
            if (bits == TEXT_BITS_PER_WORD) {
                *out++ = encodeCodeword(cur | FLAG_MESSAGE);
                ++words;
                cur  = 0;
                bits = 0;
                ++pos;
                if (pos == BATCH_SIZE) {
                    *out++ = SYNC;
                    ++words;
                    pos = 0;
                }
            }
        }
    }

    if (bits) {
        cur <<= TEXT_BITS_PER_WORD - bits;
        *out++ = encodeCodeword(cur | FLAG_MESSAGE);
        ++words;
        ++pos;
        if (pos == BATCH_SIZE) {
            *out++ = SYNC;
            ++words;
        }
    }
    return words;
}

/*-------------------------------------------------------------------
   Numeric helpers – translate characters to 4‑bit POCSAG digits
--------------------------------------------------------------------*/
static uint32_t digitValue(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    switch (c) {
        case 'S': case 's': return 0xA;
        case 'U': case 'u': return 0xB;
        case ' ':          return 0xC;
        case '-':          return 0xD;
        case ']': case ')':return 0xE;
        case '[': case '(':return 0xF;
        default:           return 0xC;
    }
}

static uint32_t encodeNumeric(uint32_t offset,
                              const char *txt,
                              uint32_t *out) {
    uint32_t words = 0;
    uint32_t cur   = 0;
    uint32_t bits  = 0;
    uint32_t pos   = offset;

    while (*txt) {
        uint32_t digit = digitValue(*txt++);
        for (uint32_t bit = 0; bit < NUMERIC_BITS_PER_DIGIT; ++bit) {
            cur <<= 1;
            cur |= (digit >> bit) & 1U;
            bits++;
            if (bits == NUMERIC_BITS_PER_WORD) {
                *out++ = encodeCodeword(cur | FLAG_MESSAGE);
                ++words;
                cur  = 0;
                bits = 0;
                ++pos;
                if (pos == BATCH_SIZE) {
                    *out++ = SYNC;
                    ++words;
                    pos = 0;
                }
            }
        }
    }

    if (bits) {
        while (bits < NUMERIC_BITS_PER_WORD) {
            uint32_t paddingDigit = 0xC;
            for (uint32_t bit = 0;
                 bit < NUMERIC_BITS_PER_DIGIT && bits < NUMERIC_BITS_PER_WORD;
                 ++bit) {
                cur <<= 1;
                cur |= (paddingDigit >> bit) & 1U;
                ++bits;
            }
        }
        *out++ = encodeCodeword(cur | FLAG_MESSAGE);
        ++words;
        ++pos;
        if (pos == BATCH_SIZE) {
            *out++ = SYNC;
            ++words;
        }
    }
    return words;
}

/*-------------------------------------------------------------------
   Address handling – determines the word offset inside a batch
--------------------------------------------------------------------*/
static inline int addressOffset(long addr) {
    return (addr & 0x7) * FRAME_SIZE;
}

/*-------------------------------------------------------------------
   Batch helpers                                                      */
static size_t messageSyncCount(size_t offset,
                               size_t messageWords) {
    size_t syncs = 0;
    size_t pos   = offset;
    for (size_t i = 0; i < messageWords; ++i) {
        ++pos;
        if (pos == BATCH_SIZE) {
            ++syncs;
            pos = 0;
        }
    }
    return syncs;
}

static size_t batchPadding(size_t words) {
    return (BATCH_SIZE + 1 - (words % (BATCH_SIZE + 1))) %
           (BATCH_SIZE + 1);
}

/*-------------------------------------------------------------------
   Build a complete transmission buffer for one repetition
--------------------------------------------------------------------*/
static void encodeTransmission(int repeatIdx,
                               long addr,
                               int fb,
                               bool numeric,
                               const char *msg,
                               uint32_t *buf) {
    if (repeatIdx == 0) {
        for (int i = 0; i < PREAMBLE_LENGTH / 32; ++i)
            *buf++ = 0xAAAAAAAAU;
    }

    uint32_t *start = buf;

    *buf++ = SYNC;                                   // SYNC for every repeat

    int prefix = addressOffset(addr);
    for (int i = 0; i < prefix; ++i)
        *buf++ = IDLE;

    *buf++ = encodeCodeword(((addr >> 3) << 2) | (fb & 0x3));

    // Tone page: fill the rest of the batch with IDLE codewords
    if (!msg || !*msg) {
        // We've used: prefix IDLEs + 1 address codeword
        // Total in batch: BATCH_SIZE (16)
        // Remaining: BATCH_SIZE - prefix - 1
        int idlesNeeded = BATCH_SIZE - prefix - 1;
        for (int i = 0; i < idlesNeeded; ++i)
            *buf++ = IDLE;
    } else {
        // Text or numeric message: encode normally
        buf += numeric
                   ? encodeNumeric(prefix + 1, msg, buf)
                   : encodeASCII(prefix + 1, msg, buf);
    }

    *buf++ = IDLE;

    size_t written = buf - start;
    size_t padding = batchPadding(written);
    for (size_t i = 0; i < padding; ++i)
        *buf++ = IDLE;
}

/*-------------------------------------------------------------------
   Estimate how many 32‑bit words a text message will occupy
--------------------------------------------------------------------*/
static size_t textMessageLength(int repeatIdx,
                                long addr,
                                int chars) {
    size_t prefix = addressOffset(addr);
    
    // Tone page (0 characters): SYNC + prefix + address + (BATCH_SIZE - prefix - 1) IDLEs + trailing IDLE
    if (chars == 0) {
        size_t words = 1 +                         // SYNC
                       prefix +                     // prefix IDLEs
                       1 +                          // address codeword
                       (BATCH_SIZE - prefix - 1) + // IDLEs to fill batch
                       1;                           // trailing IDLE
        words += batchPadding(words);
        if (repeatIdx == 0)
            words += PREAMBLE_LENGTH / 32;
        return words;
    }
    
    size_t messageWords =
        (chars * TEXT_BITS_PER_CHAR + TEXT_BITS_PER_WORD - 1) /
        TEXT_BITS_PER_WORD;
    size_t syncWords = messageSyncCount(prefix + 1, messageWords);
    size_t words = 1 +                 // SYNC (present each repeat)
                   prefix +
                   1 +                 // address codeword
                   messageWords +
                   syncWords +
                   1;                  // trailing IDLE
    words += batchPadding(words);
    if (repeatIdx == 0)
        words += PREAMBLE_LENGTH / 32;
    return words;
}

/*-------------------------------------------------------------------
   Estimate how many 32‑bit words a numeric message will occupy      */
static size_t numericMessageLength(int repeatIdx,
                                   long addr,
                                   int chars) {
    size_t prefix = addressOffset(addr);
    
    // Tone page (0 characters): SYNC + prefix + address + (BATCH_SIZE - prefix - 1) IDLEs + trailing IDLE
    if (chars == 0) {
        size_t words = 1 +                         // SYNC
                       prefix +                     // prefix IDLEs
                       1 +                          // address codeword
                       (BATCH_SIZE - prefix - 1) + // IDLEs to fill batch
                       1;                           // trailing IDLE
        words += batchPadding(words);
        if (repeatIdx == 0)
            words += PREAMBLE_LENGTH / 32;
        return words;
    }
    
    size_t messageWords =
        (chars + NUMERIC_DIGITS_PER_WORD - 1) /
        NUMERIC_DIGITS_PER_WORD;
    size_t syncWords = messageSyncCount(prefix + 1, messageWords);
    size_t words = 1 +                 // SYNC (present each repeat)
                   prefix +
                   1 +                 // address codeword
                   messageWords +
                   syncWords +
                   1;                  // trailing IDLE
    words += batchPadding(words);
    if (repeatIdx == 0)
        words += PREAMBLE_LENGTH / 32;
    return words;
}

/*-------------------------------------------------------------------
   Emit the bits for a single repetition, respecting inversion and
   the required bit delay (derived from the baud rate).               */
static void sendRepetition(uint32_t *words,
                           size_t wordCount,
                           bool inverted,
                           unsigned long bitDelay) {
    unsigned long nextBitTime = micros();
    
    for (size_t i = 0; i < wordCount; ++i) {
        uint32_t word = words[i];
        if (inverted)
            word = ~word;
            
        for (int bit = 31; bit >= 0; --bit) {
            bool value = ((word >> bit) & 1U) != 0;
            gpio_set_level((gpio_num_t)TX_PIN, value ? 0 : 1);
            
            // Busy-wait until the next bit time (more stable than delayMicroseconds)
            nextBitTime += bitDelay;
            while (micros() < nextBitTime) {
                // Spin
            }
        }
    }
}

/*-------------------------------------------------------------------
   POCSAG Function – Encode/Transmit a POCSAG Page
     Parameters
     ----------
     address      : pager capcode (0 … 2^31‑1)
     message      : text or numeric payload (null‑terminated)
     functionBits : 2‑bit function code (0‑3)
     data_type    : alphanumeric(0), numeric(1), tone(2)
     inverted     : true ⇒ inverted signalling (idle = HIGH)
     repeatCount  : how many times the whole frame is sent
     baud         : transmission speed in bits‑per‑second
--------------------------------------------------------------------*/
bool txPOCSAGMessage(long address,
                     const char *message,
                     int baud,
                     int functionBits,
                     int data_type,
                     bool inverted,
                     int repeatCount) {
    bool numeric = false;
    if (data_type == 1) {
        numeric = true;
    } else if (data_type == 2) {
        numeric = false;
        message = "";
    } else {
        numeric = false;
    }

    if (message == nullptr)
        message = "";

    if (baud <= 0)
        baud = 1200;

    unsigned long bitDelay = (1'000'000UL /
                              static_cast<unsigned long>(baud));

    gpio_reset_pin((gpio_num_t)TX_PIN);
    gpio_set_direction((gpio_num_t)TX_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t)TX_PIN, 1);

    size_t messageLength = strlen(message);

    for (int rep = 0; rep < repeatCount; ++rep) {
        size_t bufWords = numeric
                          ? numericMessageLength(rep, address, messageLength)
                          : textMessageLength(rep, address, messageLength);

        uint32_t *txBuf = static_cast<uint32_t *>(
            malloc(bufWords * sizeof(uint32_t)));
        if (!txBuf)
            return false;

        encodeTransmission(rep,
                           address,
                           functionBits,
                           numeric,
                           message,
                           txBuf);

        sendRepetition(txBuf,
                       bufWords,
                       inverted,
                       bitDelay);

        free(txBuf);
    }
    
    return true;
    gpio_set_level((gpio_num_t)TX_PIN, 1);
}