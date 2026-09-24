#ifndef PROTOCOL_VERSION_H
#define PROTOCOL_VERSION_H

/* The SPI contract between the boards. A frame whose version differs is ignored,
 * apart from the fields spi_protocol.h keeps at fixed positions. */
#define PROTOCOL_VERSION  2u

/* The G0 image version. The H7 compares the running G0's report against this value,
 * which is also the version of the image embedded in the H7, and reprograms the G0
 * on a mismatch. Bump it with every G0 change that should reach the board. */
#define G0_FW_VERSION     4u

#define H7_FW_VERSION     1u

#endif
