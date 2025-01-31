#include "IRremote.h"

// Based off the Magiquest fork of Arduino-IRremote by mpflaga
// https://github.com/mpflaga/Arduino-IRremote/

//==============================================================================
//
//
//                            M A G I Q U E S T
//
//
//==============================================================================

// MagiQuest packet is both Wand ID and magnitude of swish and flick
union magiquest_t {
    unsigned long long llword;
    struct {
        unsigned int magnitude;
        unsigned long wand_id;
        char padding;
        char scrap;	// just to pad the struct out to 64 bits so we can union with llword
    } cmd;
};

#define MAGIQUEST_BITS        50     // The number of bits in the command itself
#define MAGIQUEST_PERIOD      1150   // Length of time a full MQ "bit" consumes (1100 - 1200 usec)
/*
 * 0 = 25% mark & 75% space across 1 period
 *     1150 * 0.25 = 288 usec mark
 *     1150 - 288 = 862 usec space
 * 1 = 50% mark & 50% space across 1 period
 *     1150 * 0.5 = 575 usec mark
 *     1150 - 575 = 575 usec space
 */
#define MAGIQUEST_ONE_MARK    575
#define MAGIQUEST_ONE_SPACE   575
#define MAGIQUEST_ZERO_MARK   288
#define MAGIQUEST_ZERO_SPACE  862

#define MAGIQUEST_MASK        (1ULL << (MAGIQUEST_BITS-1))

//+=============================================================================
//
#if SEND_MAGIQUEST
void IRsend::sendMagiQuest(unsigned long wand_id, unsigned int magnitude) {
    magiquest_t data;

    data.llword = 0;
    data.cmd.wand_id = wand_id;
    data.cmd.magnitude = magnitude;

    // Set IR carrier frequency
    enableIROut(38);

    // Data
    for (unsigned long long mask = MAGIQUEST_MASK; mask > 0; mask >>= 1) {
        if (data.llword & mask) {
            DBG_PRINT("1");
            mark(MAGIQUEST_ONE_MARK);
            space(MAGIQUEST_ONE_SPACE);
        } else {
            DBG_PRINT("0");
            mark(MAGIQUEST_ZERO_MARK);
            space(MAGIQUEST_ZERO_SPACE);
        }
    }
    DBG_PRINTLN("");

    // Footer
    mark(MAGIQUEST_ZERO_MARK);
    space(0);  // Always end with the LED off
}
#endif

//+=============================================================================
//
#if DECODE_MAGIQUEST
bool IRrecv::decodeMagiQuest() {
    magiquest_t data;  // Somewhere to build our code
    unsigned int offset = 1;  // Skip the gap reading

    unsigned int mark_;
    unsigned int space_;
    unsigned int ratio_;

#if DEBUG
    char bitstring[MAGIQUEST_BITS*2];
    memset(bitstring, 0, sizeof(bitstring));
#endif

    // Check we have enough data
    if (results.rawlen < 2 * MAGIQUEST_BITS) {
        DBG_PRINT("Not enough bits to be a MagiQuest packet (");
        DBG_PRINT(irparams.rawlen);
        DBG_PRINT(" < ");
        DBG_PRINT(MAGIQUEST_BITS*2);
        DBG_PRINTLN(")");
        return false;
    }

    // Read the bits in
    data.llword = 0;
    while (offset + 1 < results.rawlen) {
        mark_ = results.rawbuf[offset++];
        space_ = results.rawbuf[offset++];
        ratio_ = space_ / mark_;

        DBG_PRINT("mark=");
        DBG_PRINT(mark_ * MICROS_PER_TICK);
        DBG_PRINT(" space=");
        DBG_PRINT(space_ * MICROS_PER_TICK);
        DBG_PRINT(" ratio=");
        DBG_PRINTLN(ratio_);

        if (MATCH_MARK(space_ + mark_, MAGIQUEST_PERIOD)) {
            if (ratio_ > 1) {
                // It's a 0
                data.llword <<= 1;
#if DEBUG
                bitstring[(offset/2)-1] = '0';
#endif
            } else {
                // It's a 1
                data.llword = (data.llword << 1) | 1;
#if DEBUG
                bitstring[(offset/2)-1] = '1';
#endif
            }
        } else {
            DBG_PRINTLN("MATCH_MARK failed");
            return false;
        }
    }
#if DEBUG
    DBG_PRINTLN(bitstring);
#endif

    // Success
    results.decode_type = MAGIQUEST;
    results.bits = offset / 2;
    results.value = data.cmd.wand_id;
    results.magnitude = data.cmd.magnitude;

    DBG_PRINT("MQ: bits=");
    DBG_PRINT(results.bits);
    DBG_PRINT(" value=");
    DBG_PRINT(results.value);
    DBG_PRINT(" magnitude=");
    DBG_PRINTLN(results.magnitude);

    irparams.rawlen = 0;

    return true;
}
bool IRrecv::decodeMagiQuest(decode_results *aResults) {
    bool aReturnValue = decodeMagiQuest();
    *aResults = results;
    return aReturnValue;
}
#endif
