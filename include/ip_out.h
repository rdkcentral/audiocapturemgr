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
#ifndef _IP_OUT_H_
#define _IP_OUT_H_
#include "audio_capture_manager.h"
#include <iostream>
#include <list>
#include <map>
#include <fstream>
#include <string>

class ip_out_client : public audio_capture_client
{
	public:
	typedef enum
	{
		EXIT = 0,
		CODE_MAX
	} control_code_t;

	private:
	std::string m_data_path;
	int m_listen_fd;
	int m_write_fd;
	int m_control_pipe[2];
	unsigned int m_num_connections;
	pthread_t m_thread;

	void process_new_connection();

	public:
	ip_out_client(q_mgr * manager);
	~ip_out_client();
	virtual int data_callback(audio_buffer *buf);
	virtual std::string get_data_path();
	virtual std::string open_output();
	virtual void close_output();
	void worker_thread();
};

#endif //_IP_OUT_H_
