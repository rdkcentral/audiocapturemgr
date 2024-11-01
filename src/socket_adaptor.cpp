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
#ifndef _GNU_SOURCE // CID 95094 : Parse warning (PW.BAD_MACRO_REDEF)
#define _GNU_SOURCE
#endif
#include "socket_adaptor.h"
#include "basic_types.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <errno.h>
#include "safec_lib.h"

#include <fcntl.h>
#include <unistd.h>

static const int PIPE_READ_FD = 0;
static const int PIPE_WRITE_FD = 1;
static const unsigned int MAX_CONNECTIONS = 1;

static bool g_one_time_init_complete = false;

socket_adaptor::socket_adaptor() : m_listen_fd(-1), m_write_fd(-1), m_num_connections(0), m_callback(nullptr), m_callback_data(nullptr) // CID 80575 : Uninitialized pointer field (UNINIT_CTOR)
{
	INFO("Enter\n")
	if(!g_one_time_init_complete)
	{
		/*SIGPIPE must be ignored or process will exit when client closes connection*/
		struct sigaction sig_settings;
		sig_settings.sa_handler = SIG_IGN;
		sig_settings.sa_flags = 0;  //CID:81611 - Initialize uninit
		sigemptyset(&sig_settings.sa_mask); //CID:81611 - Initialize uninit
		sigaction(SIGPIPE, &sig_settings, NULL);
		g_one_time_init_complete = true;
	}
	REPORT_IF_UNEQUAL(0, pipe2(m_control_pipe, O_NONBLOCK));
}

socket_adaptor::~socket_adaptor()
{
	INFO("Enter\n")
	stop_listening();
	close(m_control_pipe[PIPE_WRITE_FD]);
	close(m_control_pipe[PIPE_READ_FD]);
}

int socket_adaptor::write_data(const char * buffer, const unsigned int size)
{
	int ret = write(m_write_fd, buffer, size);
	if(0 > ret)
	{
		WARN("Write error! Closing socket. errno: 0x%x\n", errno);
		perror("socket_adaptor::data_callback() ");

		close(m_write_fd);
		lock();
		if(0 < m_write_fd)
		{
			m_write_fd = -1;
			m_num_connections--;
		}
		unlock();
	}
	else if(ret != size)
	{
		WARN("Incomplete buffer write!\n");
	}
	return ret;
}

std::string& socket_adaptor::get_path()
{
	return m_path;
}

int socket_adaptor::start_listening(const std::string &path)
{
	int ret = 0;
	m_path = path;
	errno_t rc = -1;
	/*Open new UNIX socket to transfer data*/
	m_listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0); //TODO: Does it really need to be non-blocking?
	if(0 < m_listen_fd)
	{
		DEBUG("Socket created.\n");

		for(int retries = 0; retries <  2; retries++) //If first attempt fails due to path in use, unlink it and try again.
		{
			struct sockaddr_un bind_path;
			bind_path.sun_family = AF_UNIX;
			rc = strcpy_s(bind_path.sun_path, sizeof(bind_path.sun_path), m_path.c_str());
			if(rc != EOK)
			{
				ERR_CHK(rc);
			}
			INFO("Binding to path %s\n", bind_path.sun_path);
			ret = bind(m_listen_fd, (const struct sockaddr *) &bind_path, sizeof(bind_path));
			if(-1 == ret)
			{
				if(EADDRINUSE == errno)
				{
					WARN("path is already in use. Using brute force.\n");
					unlink(m_path.c_str());
				}
				else
				{
					ERROR("Failed to bind to path. Error is %d\n", errno);
					perror("bind error");
					close(m_listen_fd);
					m_listen_fd = -1;
					break;
				}
			}
			else
			{
				INFO("Bound successfully to path.\n");
				REPORT_IF_UNEQUAL(0, listen(m_listen_fd, 3));
				m_thread = std::thread(&socket_adaptor::worker_thread, this);
				break;
			}
		}
	}
	else
	{
		ERROR("Could not open socket.\n");
		ret = -1;
	}
	return ret;
}

void socket_adaptor::stop_listening()
{
	INFO("Enter\n");
	lock();

	/*Shut down worker thread that listens to incoming connections.*/
	if(m_thread.joinable())
	{
		int message = EXIT;
		int ret = write(m_control_pipe[PIPE_WRITE_FD], &message, sizeof(message));
		if(ret != sizeof(message))
		{
			ERROR("Couldn't trigger worker thread shutdown.\n");
		}
		else
		{
			INFO("Waiting for worker thread to join.\n");
			unlock();
			m_thread.join();
			lock();
			INFO("Worker thread has joined.\n");
		}
	}
	if(!m_path.empty())
	{
		if(0 < m_write_fd)
		{
			close(m_write_fd);
			m_write_fd = -1;
			m_num_connections--;
			INFO("Closed write fd. Total active connections now is %d\n", m_num_connections);
		}
		INFO("Removing named socket %s.\n", m_path.c_str());
		unlink(m_path.c_str());
		m_path.clear();
	}
	unlock();
	INFO("Exit\n");
}

void socket_adaptor::process_new_connection()
{
	INFO("Enter\n");
	lock();
	if(0 < m_write_fd)
	{
		WARN("Trashing old write socket in favour of new one.\n");
		close(m_write_fd);
		m_write_fd = -1;
		m_num_connections--;
	}	
	
	DEBUG("Setting up a new connection.\n");
	struct sockaddr_un incoming_addr;
	int addrlen = sizeof(incoming_addr);
	m_write_fd = accept(m_listen_fd, (sockaddr *) &incoming_addr, (socklen_t *)&addrlen);

	if(0 > m_write_fd)
	{
		ERROR("Error accepting connection.\n");
	}
	else
	{
		m_num_connections++;
		INFO("Connected to new client.\n");
	}
	unlock();

	if(m_callback)
	{
		m_callback(m_callback_data);
	}
	return;
}

void socket_adaptor::worker_thread()
{
	INFO("Enter\n");
	int control_fd = m_control_pipe[PIPE_READ_FD];
	int max_fd = (m_listen_fd > control_fd ? m_listen_fd : control_fd);


	bool check_fds = true;

	while(check_fds)
	{
		fd_set poll_fd_set;
		FD_ZERO(&poll_fd_set);
		FD_SET(m_listen_fd, &poll_fd_set);
		FD_SET(control_fd, &poll_fd_set);

		int ret = select((max_fd + 1), &poll_fd_set, NULL, NULL, NULL);
		DEBUG("Unblocking now. ret is 0x%x\n", ret);
		if(0 == ret)
		{
			ERROR("select() returned 0.\n");
		}
		else if(0 < ret)
		{
			//Some activity was detected. Process event further.
			if(0 != FD_ISSET(control_fd, &poll_fd_set))
			{
				control_code_t message;
				REPORT_IF_UNEQUAL((sizeof(message)), read(m_control_pipe[PIPE_READ_FD], &message, sizeof(message)));
				if(EXIT == message)
				{
					INFO("Exiting monitor thread.\n");
					break;
				}
				else
				{
					process_control_message(message);
				}
			}
			if(0 != FD_ISSET(m_listen_fd, &poll_fd_set))
			{
				process_new_connection();
			}
		}
		else
		{
			ERROR("Error polling monitor FD!\n");
			break;
		}
	}

	INFO("Exit\n");
}

void socket_adaptor::terminate_current_connection()
{
	lock();
	if(0 < m_write_fd)
	{
		close(m_write_fd);
		m_write_fd = -1;
		m_num_connections--;
		INFO("Terminated current connection.\n");
	}
	unlock();
}


void socket_adaptor::lock()
{
	m_mutex.lock();
}

void socket_adaptor::unlock()
{
	m_mutex.unlock();
}

void socket_adaptor::process_control_message(control_code_t message)
{
	if(NEW_CALLBACK == message)
	{
		lock();
		auto num_connections = m_num_connections;
		auto callback = m_callback;
		auto callback_data = m_callback_data;
		unlock();

		if(callback && (0 != num_connections))
		{
			callback(callback_data);
		}
	}
}


unsigned int socket_adaptor::get_active_connections() 
{
	lock();
	unsigned int num_conn = m_num_connections;
	unlock();
	return m_num_connections;
}

void socket_adaptor::register_data_ready_callback(socket_adaptor_cb_t callback, void *data)
{
	lock();
	m_callback = callback;
	m_callback_data = data;
	unlock();

	if(nullptr != callback)
	{
		control_code_t message = NEW_CALLBACK;
		int ret = write(m_control_pipe[PIPE_WRITE_FD], &message, sizeof(message));
	}
}
