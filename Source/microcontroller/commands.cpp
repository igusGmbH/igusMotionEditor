// Command handling
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#include "commands.h"

#include "protocol.h"
#include "combuf.h"
#include "uart.h"
#include "mem.h"
#include "motion.h"

#include <string.h>
#include <util/delay.h>

const int PAYLOAD_BUFSIZE = 256;
uint8_t payloadBuffer[PAYLOAD_BUFSIZE];

bool g_extShouldQuit = false;

enum ParserState
{
	PS_START,
	PS_VERSION,
	PS_COMMAND,
	PS_LENGTH,
	PS_PAYLOAD,
	PS_CHECKSUM,
	PS_END
};

template<class T>
void writeAnswer(const T& data)
{
	com_buf_to_pc.putData((const uint8_t*)&data, sizeof(data));
	uart_pc->startTransmitting();
	while(com_buf_to_pc.available());
}

template<int CMD_CODE>
void writeFeedbackPacket()
{
	proto::Packet<CMD_CODE, proto::Feedback> answer;
	answer.payload.num_axes = mem_config.active_axes;

	answer.payload.flags = 0;
	if(motion_isPlaying())
		answer.payload.flags |= proto::FF_PLAYING;

	for(uint8_t i = 0; i < mem_config.active_axes; ++i)
		answer.payload.positions[i] = motion_feedback(i);

	answer.updateChecksum();
	writeAnswer(answer);
}

void handleCommand(uint8_t command, const uint8_t* payload, uint8_t length)
{
	switch(command)
	{
		case proto::CMD_INIT:
		{
			proto::SimplePacket<proto::CMD_INIT> answer;
			writeAnswer(answer);
		}
			break;
		case proto::CMD_EXIT:
		{
			proto::SimplePacket<proto::CMD_EXIT> answer;
			writeAnswer(answer);
		}
			g_extShouldQuit = true;
			break;
		case proto::CMD_SAVE_KEYFRAME:
		{
			const proto::SaveKeyframe& packet = *((const proto::SaveKeyframe*)payload);

			if(length != sizeof(packet) || motion_isPlaying())
				return;

			motion_writeToBuffer(packet.index, packet.keyframe);

			proto::SimplePacket<proto::CMD_SAVE_KEYFRAME> answer;
			writeAnswer(answer);
		}
			break;
		case proto::CMD_READ_KEYFRAME:
		{
			const proto::ReadKeyframe& packet = *((const proto::ReadKeyframe*)payload);
			if(length != sizeof(packet))
				return;

			proto::Packet<proto::CMD_READ_KEYFRAME, proto::Keyframe> answer;
			mem_readKeyframe(packet.index, &answer.payload);

			answer.updateChecksum();
			writeAnswer(answer);
		}
			break;
		case proto::CMD_CONFIG:
		{
			if(motion_isPlaying())
				return;

			if(length == sizeof(proto::Config))
			{
				mem_config = *((const proto::Config*)payload);

				// Trigger first output command if we are in the starting position
				motion_isInStartPosition();

				proto::SimplePacket<proto::CMD_CONFIG> answer;
				writeAnswer(answer);
			}
			else if(length == 0)
			{
				proto::Packet<proto::CMD_CONFIG, proto::Config> answer;
				answer.payload = mem_config;
				answer.updateChecksum();
				writeAnswer(answer);
			}
		}
			break;
		case proto::CMD_RESET:
			if(memcmp(payload, proto::RESET_KEY, sizeof(proto::RESET_KEY)) == 0)
			{
				cli();
				asm volatile ("jmp 0x3F800");
			}
			break;
		case proto::CMD_COMMIT:
			motion_commit();
			writeAnswer(proto::SimplePacket<proto::CMD_COMMIT>());
			break;
		case proto::CMD_PLAY:
		{
			const proto::Play& play = *((const proto::Play*)payload);
			writeAnswer(proto::SimplePacket<proto::CMD_PLAY>());

			if(!motion_isPlaying())
				motion_runSequence(play.flags & proto::PF_LOOP);
		}
			break;
		case proto::CMD_STOP:
			motion_stop();
			writeAnswer(proto::SimplePacket<proto::CMD_STOP>());
			break;
		case proto::CMD_MOTION:
			motion_executeSingleMotion(*((const proto::Motion*)payload));
			writeFeedbackPacket<proto::CMD_MOTION>();
			break;
		case proto::CMD_FEEDBACK:
			writeFeedbackPacket<proto::CMD_FEEDBACK>();
			break;
	}
}

uint8_t parser_state = PS_START;
uint8_t command = 0;
uint8_t payloadLength = 0;
uint8_t payloadIdx = 0;

bool cmd_input(uint8_t c)
{
	switch(parser_state)
	{
		case PS_START:
			payloadIdx = 0;
			if(c == 0xFF)
				parser_state = PS_VERSION;
			break;
		case PS_VERSION:
			if(c == proto::VERSION)
				parser_state = PS_COMMAND;
			else
				parser_state = PS_START;
			break;
		case PS_COMMAND:
			command = c;
			if(c < proto::CMD_COUNT)
				parser_state = PS_LENGTH;
			else
				parser_state = PS_START;
			break;
		case PS_LENGTH:
			payloadLength = c;
			if(c)
				parser_state = PS_PAYLOAD;
			else
				parser_state = PS_CHECKSUM;
			break;
		case PS_PAYLOAD:
			payloadBuffer[payloadIdx++] = c;
			if(payloadIdx == payloadLength)
				parser_state = PS_CHECKSUM;
			break;
		case PS_CHECKSUM:
		{
			uint8_t checksum = proto::VERSION + command + payloadLength;
			for(uint8_t i = 0; i < payloadLength; ++i)
				checksum += payloadBuffer[i];
			checksum = ~checksum;

			if(checksum == c)
				parser_state = PS_END;
			else
				parser_state = PS_START;
		}
			break;
		case PS_END:
			// Prepare parser for communication inside command
			parser_state = PS_START;

			if(c == 0x0D)
			{
				handleCommand(command, payloadBuffer, payloadLength);
				return true;
			}

			break;
		default:
			parser_state = PS_START;
			break;
	}

	return false;
}

void handleCommands()
{
	uint8_t ticks_since_last_msg = 0;
	while(!g_extShouldQuit)
	{
		while(com_buf_to_bot.available())
		{
			uint8_t c = com_buf_to_bot.get();
			if(cmd_input(c))
				ticks_since_last_msg = 0;
		}

		_delay_ms(1);
		if(++ticks_since_last_msg == 255)
			break;
	}

	g_extShouldQuit = false;
}
