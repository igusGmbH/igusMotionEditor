// µC protocol definition
// Author: Max Schwarz <max.schwarz@online.de>

#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <stdint.h>

namespace proto
{

// The extended protocol has to be activated through a CMD_INIT packet defined
// below. With this system old-style tools which want to talk directly
// to the motor controllers can do so without modifications.

const int VERSION = 10;
const int NUM_AXES = 8;
const int MAX_KEYFRAMES = 128;
const int NT_POSITION_BIAS = 16384;

enum Command
{
	CMD_INIT          =  0, //!< Enable extended protocol
	CMD_RESET         =  1, //!< Reset microcontroller (and enter bootloader)
	CMD_CONFIG        =  2, //!< Read/save axis configuration
	CMD_READ_KEYFRAME =  3, //!< Read keyframe
	CMD_SAVE_KEYFRAME =  4, //!< Save keyframe
	CMD_EXIT          =  5, //!< Exit extended protocol
	CMD_COMMIT        =  6, //!< Save motion sequence to EEPROM
	CMD_PLAY          =  7, //!< Play motion sequence
	CMD_STOP          =  8, //!< Stop
	CMD_FEEDBACK      =  9, //!< Get position feedback
	CMD_MOTION        = 10, //!< Execute single motion command

	CMD_COUNT
};

struct PacketHeader
{
	PacketHeader(uint8_t _command, uint8_t _payloadLength)
	 : start(0xFF)
	 , version(VERSION)
	 , command(_command)
	 , length(_payloadLength)
	{
	}

	uint8_t start;   //!< Fixed 0xFF
	uint8_t version; //!< Protocol version
	uint8_t command; //!< Command code
	uint8_t length;  //!< Payload length

	// Payload + 1 byte checksum + 1 byte end (0x0D) follow
} __attribute__((packed));

enum OutputCommand
{
	OC_NOP,   //!< Do nothing
	OC_SET,   //!< Set output
	OC_RESET, //!< Reset output

	OC_COUNT
};

struct Keyframe
{
	uint16_t duration;
	uint16_t ticks[NUM_AXES];
	uint8_t output_command;
} __attribute__((packed));

struct SaveKeyframe
{
	uint8_t index;
	Keyframe keyframe;
} __attribute__((packed));

struct ReadKeyframe
{
	uint8_t index;
} __attribute__((packed));

struct Config
{
	uint16_t num_keyframes;
	uint16_t active_axes;
	uint16_t enc_to_mot[NUM_AXES]; //!< encoder_velocity = mot_to_enc * motor_velocity
	uint16_t lookahead;
} __attribute__((packed));

enum FeedbackFlags
{
	FF_PLAYING = 1
};

struct Feedback
{
	uint8_t num_axes;
	uint8_t flags;
	int16_t positions[NUM_AXES];
} __attribute__((packed));

enum PlayFlags
{
	PF_LOOP = 1
};

struct Play
{
	uint8_t flags;
} __attribute__((packed));

const uint8_t RESET_KEY[8] = {0x0A, 0x65, 0x38, 0x47, 0x82, 0xAB, 0xBF};
struct Reset
{
	uint8_t key[8];
} __attribute__((packed));

struct Motion
{
	uint16_t ticks[NUM_AXES];
	uint16_t velocity[NUM_AXES];
	uint8_t num_axes;
	uint8_t output_command;
} __attribute__((packed));

inline uint8_t packetChecksum(const PacketHeader& header, const uint8_t* payload)
{
	uint8_t checksum = header.command + header.version + header.length;

    if(payload)
    {
        for(uint8_t i = 0; i < header.length; ++i)
            checksum += payload[i];
    }

	return ~checksum;
}

template<uint8_t Cmd>
struct SimplePacket
{
	SimplePacket()
	 : header(Cmd, 0)
	 , checksum(packetChecksum(header, 0))
	 , end(0x0D)
	{
	}

	inline uint8_t operator[](uint8_t idx) const
	{
		return ((const uint8_t*)this)[idx];
	}

    inline uint8_t currentChecksum() const
    {
        return packetChecksum(header, 0);
    }

	PacketHeader header;
	uint8_t checksum;
	uint8_t end;
};

template<uint8_t Cmd, class Payload>
struct Packet
{
	Packet()
	 : header(Cmd, sizeof(Payload))
	 , end(0x0D)
	{
	}

    inline uint8_t currentChecksum() const
    {
        return packetChecksum(header, (const uint8_t*)&payload);
    }

	inline void updateChecksum()
	{
        checksum = currentChecksum();
	}

	PacketHeader header;
	Payload payload;
	uint8_t checksum;
	uint8_t end;
} __attribute__((packed));

}

#endif
