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
#ifndef _socket_adaptor_H_
#define _socket_adaptor_H_
#include <fstream>
#include <string>
#include <thread>
#include <mutex>

/**
 * @addtogroup AUDIO_CAPTURE_MANAGER_API
 * @{
 */

typedef void (*socket_adaptor_cb_t)(void * data);

class socket_adaptor
{
	public:
	typedef enum
	{
		EXIT = 0,
		NEW_CALLBACK,
		CODE_MAX
	} control_code_t;

	private:
	std::string m_path;
	int m_listen_fd;
	int m_write_fd;
	int m_control_pipe[2];
	unsigned int m_num_connections;
	std::thread m_thread;
	std::mutex m_mutex;
	socket_adaptor_cb_t m_callback;
	void * m_callback_data;

	void process_new_connection();
	void process_control_message(control_code_t message);
	void stop_listening();
	void lock();
	void unlock();
	void worker_thread();

	public:
	socket_adaptor();
	~socket_adaptor();

    /**
     *  @brief This function makes the audiocapturemgr listen for incoming unix domain connections to the given path.
     *
     *  @param[in] path  binding path.
     *
     *  @return Returns 0 on success, appropiate error code otherwise.
     */
	int start_listening(const std::string &path);

    /**
     *  @brief This api returns the data path.
     *
     *  It is the path of unix domain server that ip-out clients must connect to in order to receive real-time audio data.
     *
     *  @return Returns the data path in string.
     */
	std::string& get_path();

    /**
     *  @brief This api invokes unix write() to write data to the socket.
     *
     *  @param[in] buffer Data buffer.
     *  @param[in] size   Size of the buffer
     *
     *  @return Returns 0 on success, appropiate errorcode otherwise.
     */
	int write_data(const char * buffer, const unsigned int size);

    /**
     *  @brief This api invokes  close() to terminate the current connection.
     */
	void terminate_current_connection(); //handy to simulate EOS at the other end.

    /**
     *  @brief This api returns the number of active connections.
     *
     *  @return Returns the number of active connections.
     */
	unsigned int get_active_connections();
	void register_data_ready_callback(socket_adaptor_cb_t cb, void *data);
};
#endif //_socket_adaptor_H_

/**
 * @}
 */

