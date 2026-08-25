#ifndef AMA_TRANSPORT_V1_H
#define AMA_TRANSPORT_V1_H

#define PACKET_INVALID_LENGTH 0xFFFF
#define AMA_HEADER_VERSION_OFFSET 12
#define AMA_HEADER_VERSION_MASK 0xF000
#define AMA_HEADER_STREAM_ID_OFFSET 7
#define AMA_HEADER_STREAM_ID_MASK 0x0F80
#define AMA_HEADER_LENTGH_MASK 0x0001

#define AMA_TRANSPORT_STREAM_ID_CONTROL 0x0
#define AMA_TRANSPORT_STREAM_ID_VOICE 0x1

/*! \brief Get transport header length
    \param payload_len - length of payload
    \return header length
*/
uint16 AmaTransport_VersionGetHeaderSize(const uint16 payload_len);

#endif // AMA_TRANSPORT_V1_H
