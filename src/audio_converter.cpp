/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#include <string.h>
#include "audio_converter.h"
#include <stdint.h>
const unsigned int TEMPORARY_BUFFER_SIZE = 100 * 1024; //100kB

audio_converter::audio_converter(const audiocapturemgr::audio_properties_t &in_props, const audiocapturemgr::audio_properties_t &out_props, audio_converter_sink &sink) : m_in_props(in_props), m_out_props(out_props), m_sink(sink)
{
	m_downsample = false; //CID:88634 - Intialize bool variables
	m_downmix = false;
	process_conversion_params();
}


int audio_converter::process_conversion_params()
{
	int ret = -1;
	bool format_ok = false;
	bool sample_rate_ok = false;
	bool downmix = false;
	bool downsample = false;

	m_op = UNSUPPORTED_CONVERSION;

	do
	{
		if((racFormat_e16BitMono == m_out_props.format) && (racFormat_e16BitStereo == m_in_props.format))
		{
			INFO("Convert 16-bit stereo to mono.\n");
			format_ok = true;
			downmix = true;
			break;
		}

		if(m_out_props.format == m_in_props.format)
		{
			INFO("No format conversion.\n");
			format_ok = true;
			break;
		}

		if((racFormat_e16BitMono == m_out_props.format) && (
			(racFormat_e16BitMonoLeft == m_in_props.format) ||
			(racFormat_e16BitMonoRight == m_in_props.format)))
		{
			INFO("Convert Mono/mono-left/mono-right to mono.\n");
			format_ok = true;
			break;
		}
		ERROR("Unsupported format conversion: 0x%x to 0x%x.\n", m_in_props.format, m_out_props.format);
	
	} while(false);

	if(format_ok)
	{
		unsigned int in_sampling_rate, in_bits_per_sample, in_num_channels;
		unsigned int out_sampling_rate, out_bits_per_sample, out_num_channels;
		audiocapturemgr::get_individual_audio_parameters(m_in_props, in_sampling_rate, in_bits_per_sample, in_num_channels);
		audiocapturemgr::get_individual_audio_parameters(m_out_props, out_sampling_rate, out_bits_per_sample, out_num_channels);

		if(out_sampling_rate > in_sampling_rate)
		{
			ERROR("Cannot up-sample. %d to %d.\n", in_sampling_rate,  out_sampling_rate);
		}
		else if(0 != (in_sampling_rate % out_sampling_rate))
		{
			ERROR("Incompatible sampling rates: %d to %d.\n", in_sampling_rate,  out_sampling_rate);
		}
		else
		{
			sample_rate_ok = true;
			if(out_sampling_rate < in_sampling_rate)
			{
				downsample = true;
				INFO("Downsample from %d to %d\n", in_sampling_rate, out_sampling_rate);
			}
		}
	}

	if(format_ok && sample_rate_ok) 
	{
		if (downmix && downsample)
		{
			m_op = DOWNMIX_AND_DOWNSAMPLE;
		}
		else if(downmix && !downsample)
		{
			m_op = DOWNMIX;
		}
		else if (!downmix && downsample)
		{
			m_op = DOWNSAMPLE;
		}
		else
		{
			m_op = NO_CONVERSION;
		}
		ret = 0;
	}
	return ret;
}

int audio_converter::downsample_and_downmix(const std::list<audio_buffer *> &queue, int size)
{
	int ret = 0;
	
	unsigned int in_sampling_rate, in_bits_per_sample, in_num_channels;
	audiocapturemgr::get_individual_audio_parameters(m_in_props, in_sampling_rate, in_bits_per_sample, in_num_channels);

	unsigned int out_sampling_rate, out_bits_per_sample, out_num_channels;
	audiocapturemgr::get_individual_audio_parameters(m_out_props, out_sampling_rate, out_bits_per_sample, out_num_channels);

	unsigned int sample_size = in_bits_per_sample / 8;
	unsigned int frame_size = in_num_channels * sample_size; 
	unsigned int leap_value = frame_size * in_sampling_rate / out_sampling_rate;
	unsigned int write_length = (DOWNMIX_AND_DOWNSAMPLE == m_op ? sample_size : frame_size ); //Write only first sample of a frame if we're downmixing.

	int read_offset = 0;
	
	unsigned int temp_buffer_write_offset = 0;

	for(auto &entry: queue)
	{
		char * ptr = (char *)entry->m_start_ptr + read_offset;
		if(0 != (entry->m_size % frame_size))
		{
			WARN("Audio buffer not aligned with frame boundary!\n");
		}

		int buffer_size = entry->m_size;
		while (buffer_size > 0)
		{
			m_sink.write_data(ptr, write_length);
			buffer_size -= leap_value;
			ptr += leap_value;
		}

		/*If the value of buffer_size when exiting the above loop is a negative value (it can only be zero or negative), we need to skip that many bytes
		 * when processing the next buffer in the list. This is to maintain the continuity of 'frame-skipping' across buffer boundaries. read_offset will 
		 * now be a positive value that we advance the read pointer of the next buffer by.
		 * 
		 * Illustration:
		 * If we're skipping 3 samples for every 4 samples read (i. e. writing only one sample for every 4 samples read), and we ran out of data in the current
		 * buffer after skipping just one sample, the remaining 2 samples need to be skipped at the beginning of the next buffer.
		 * */
		read_offset = -1 * buffer_size;

		size -= entry->m_size;
		if(0 >= size)
		{
			break;
		}
	}
	return ret;
}


int audio_converter::downmix(const std::list<audio_buffer *> &queue, int size)
{
	int ret = 0;
	
	unsigned int in_sampling_rate, in_bits_per_sample, in_num_channels;
	audiocapturemgr::get_individual_audio_parameters(m_in_props, in_sampling_rate, in_bits_per_sample, in_num_channels);

	unsigned int sample_size = in_bits_per_sample / 8;
	unsigned int frame_size = in_num_channels * sample_size;
	
	audio_converter_memory_sink * memsink = dynamic_cast <audio_converter_memory_sink * > (&m_sink);

	/* Targeted optimizations to downmix 16-bit 2 chanel audio to 1-channel. */
	if(memsink && (16 == in_bits_per_sample) && (2 == in_num_channels))
	{
		INFO("Running special optimizations for 16-bit stereo to mono conversion.\n");
		int16_t * dptr = (int16_t *)memsink->get_buffer();
		unsigned int write_offset = memsink->get_size();
		for(auto &entry: queue)
		{
			int16_t * sptr = (int16_t *)entry->m_start_ptr;
			unsigned int data_remaining = entry->m_size;
			while(32 <= data_remaining) 
			{
				dptr[0] = sptr[0];
				dptr[1] = sptr[2];
				dptr[2] = sptr[4];
				dptr[3] = sptr[6];
				dptr[4] = sptr[8];
				dptr[5] = sptr[10];
				dptr[6] = sptr[12];
				dptr[7] = sptr[14];
				dptr += 8;
				sptr += 16;
				data_remaining -= 16 * 2;
				write_offset += 8 * 2;
			}
		
			/* If there is any remaining data making up less than 32 bytes, process that as well*/
			while(0 != data_remaining)
			{
				*dptr = *sptr;
				dptr += 1;
				sptr += 2;
				data_remaining -= 2 * 2;
				write_offset += 1 * 2;
			}

		}
		memsink->m_write_offset = write_offset;
		INFO("Final write offset is %d. Final value of dptr: %p\n", write_offset, dptr); //CID:128015 - Type cast
	}
	else
	{
		for(auto &entry: queue)
		{
			char * ptr = (char *)entry->m_start_ptr;
			if(0 != (entry->m_size % frame_size))
			{
				WARN("Audio buffer not aligned with frame boundary!\n");
			}

			unsigned int buffer_size = entry->m_size;
			while (buffer_size > 0)
			{
				m_sink.write_data(ptr, sample_size);		
				buffer_size -= frame_size;
				ptr += frame_size;
			}

			size -= entry->m_size;
			if(0 >= size)
			{
				break;
			}
		}
	}
	return ret;
}
#if 0
int audio_converter::downsample(std::list<audio_buffer *> &queue, int size)
{
	int ret = 0;
	
	unsigned int in_sampling_rate, in_bits_per_sample, in_num_channels;
	audiocapturemgr::get_individual_audio_parameters(m_in_props, in_sampling_rate, in_bits_per_sample, in_num_channels);

	unsigned int out_sampling_rate, out_bits_per_sample, out_num_channels;
	audiocapturemgr::get_individual_audio_parameters(m_out_props, out_sampling_rate, out_bits_per_sample, out_num_channels);

	unsigned int frame_size = in_num_channels * in_bits_per_sample * 8; 
	unsigned int leap_value = frame_size * in_sampling_rate / out_sampling_rate;
	int read_offset = 0;

	for(auto &entry: queue)
	{
		char * ptr = (char *)entry->m_start_ptr + read_offset;
		if(0 != (entry->m_size % frame_size))
		{
			WARN("Audio buffer not aligned with frame boundary!\n");
		}

		int buffer_size = entry->m_size;
		while (buffer_size > 0)
		{
			m_sink.write_data(ptr, frame_size); //TODO Check for errors
			buffer_size -= leap_value;
			ptr += leap_value;
		}
		read_offset = -1 * buffer_size;

		size -= entry->m_size;
		if(0 >= size)
		{
			break;
		}
	}
	return ret;
}
#endif 
int audio_converter::passthrough(const std::list<audio_buffer *> &queue, int size)
{
	int ret = -1;
	for(auto &entry: queue)
	{
		ret = m_sink.write_data((char *)entry->m_start_ptr, entry->m_size);
		if(0 > ret)
		{
			ERROR("Write error!\n");
			break;
		}
		size -= entry->m_size;
		if(0 > size)
		{
			break;
		}
	}
	return ret;
}

int audio_converter::convert(const std::list<audio_buffer *> &queue, unsigned int size)
{
	int ret = -1;
	INFO("Operation: 0x%x\n", m_op);
	switch(m_op)
	{
		case DOWNMIX_AND_DOWNSAMPLE:
			ret = downsample_and_downmix(queue, size);
			break;
		
		case DOWNMIX:
			ret = downmix(queue, size);
			break;

		case DOWNSAMPLE:
			ret = downsample_and_downmix(queue, size);
			break;

		case NO_CONVERSION:
			ret = passthrough(queue, size);
			break;

		default:
			ERROR("Unsupported conversion.\n");
			ret = -1;
	}
	
	return ret;
}

int audio_converter_file_sink::write_data(const char * ptr, unsigned int size)
{
	int ret = 0;
	m_file.write(ptr, size); //TODO: Add error handling
	return ret;
}


audio_converter_memory_sink::audio_converter_memory_sink(unsigned int max_size) : m_write_offset(0)
{
	m_buffer = new char[max_size];
	INFO("Created with size %d. ptr: %p, this: %p\n", max_size, m_buffer, this); //CID:127553 and CID:127680 - Type cast
}

audio_converter_memory_sink::~audio_converter_memory_sink()
{
	INFO("Destroying %p\n", this); //CID:127449- Type cast
	delete [] m_buffer;
}

int audio_converter_memory_sink::write_data(const char * ptr, unsigned int size)
{
	int ret = 0;
	memcpy(&m_buffer[m_write_offset], ptr, size);
	m_write_offset += size;
	return ret;
}
