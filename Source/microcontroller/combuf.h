// Communication buffers
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#ifndef COMBUF_H
#define COMBUF_H

#include <stdint.h>
#include <util/atomic.h>

const int BUFSIZE = 256;
const int BUFSIZE_MASK = BUFSIZE - 1;

class CircularBuffer
{
public:
	CircularBuffer()
	 : m_write_idx(0)
	 , m_read_idx(0)
	{
	}

	inline void put(uint8_t c)
	{
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			uint8_t newidx = (m_write_idx + 1) & BUFSIZE_MASK;
			if(newidx != m_read_idx)
			{
				m_buf[m_write_idx] = c;
				m_write_idx = newidx;
			}
		}
	}

	inline void puts(const char* str)
	{
		while(*str)
			put(*(str++));
	}

	inline void putData(const uint8_t* data, uint8_t length)
	{
		for(uint8_t i = 0; i < length; ++i)
			put(data[i]);
	}

	inline void flush()
	{
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			m_read_idx = m_write_idx;
		}
	}

	inline bool available()
	{
		bool ret;
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			ret = m_read_idx != m_write_idx;
		}
		return ret;
	}

	inline uint8_t get()
	{
		uint8_t c;

		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			c = m_buf[m_read_idx];
			if(m_read_idx != m_write_idx)
				m_read_idx = (m_read_idx + 1) & BUFSIZE_MASK;
			else
				PORTJ &= ~(1 << 7);
		}

		return c;
	}
private:
	uint8_t m_buf[BUFSIZE];
	uint8_t m_write_idx;
	uint8_t m_read_idx;
};

extern CircularBuffer com_buf_to_pc;
extern CircularBuffer com_buf_to_bot;
extern CircularBuffer com_buf_from_bot;

#endif
