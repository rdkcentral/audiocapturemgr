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
#ifndef _AUDIO_CONVERTER_H_
#define _AUDIO_CONVERTER_H_

#include <fstream>
#include <list>
#include "audio_capture_manager.h"

class audio_converter_sink
{
	public:
	virtual int write_data(const char * ptr, unsigned int size) = 0;
};


class audio_converter
{
	public:
		typedef enum
		{
			NO_CONVERSION = 0,
			DOWNMIX,
			DOWNSAMPLE,
			DOWNMIX_AND_DOWNSAMPLE,
			UNSUPPORTED_CONVERSION,
		} conversion_ops_t;

	private:
	const audiocapturemgr::audio_properties_t &m_in_props;
	const audiocapturemgr::audio_properties_t &m_out_props;
	bool m_downmix;
	bool m_downsample;
	audio_converter_sink &m_sink;

	//virtual	int write_data(const char * ptr, unsigned int size){}
	int process_conversion_params();
	int downsample_and_downmix(const std::list<audio_buffer *> &queue, int size);
	int passthrough(const std::list<audio_buffer *> &queue, int size);

	protected:
	conversion_ops_t m_op;

	public:
	audio_converter(const audiocapturemgr::audio_properties_t &in_props,const audiocapturemgr::audio_properties_t &out_props, audio_converter_sink &sink);
	virtual ~audio_converter() {}
	virtual int convert(const std::list<audio_buffer *> &queue, unsigned int size);
	void convert(const audio_buffer * buffer) {} //TODO
	int downmix(const std::list<audio_buffer *> &queue, int size); //public because of the friend declaration 
};

class audio_converter_file_sink : public audio_converter_sink 
{
	private:
	std::ofstream &m_file;
	virtual int write_data(const char * ptr, unsigned int size) override;

	public:
	audio_converter_file_sink(std::ofstream &file) : m_file(file) {}
	virtual ~audio_converter_file_sink() {}
};



class audio_converter_memory_sink : public audio_converter_sink
{
	private:
	char * m_buffer;
	unsigned int m_write_offset;

	public:
	audio_converter_memory_sink(unsigned int max_size);
	virtual ~audio_converter_memory_sink();
	virtual int write_data(const char * ptr, unsigned int size) override;
	inline char * get_buffer() { return m_buffer; }
	inline unsigned int get_size() { return m_write_offset; }
	
	/* Direct access provided to downmix method in order to make optimizations
	 * that bypass the write method possible. */
	friend int audio_converter::downmix(const std::list<audio_buffer *> &queue, int size); 
};

#endif //_AUDIO_CONVERTER_H_
