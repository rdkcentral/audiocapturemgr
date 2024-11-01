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
#ifndef _GNU_SOURCE // CID 95868 : Parse warning (PW.BAD_MACRO_REDEF)
#define _GNU_SOURCE
#endif
#include "ip_out.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>
#include "safec_lib.h"

using namespace audiocapturemgr;
std::string SOCKNAME_PREFIX = "/tmp/acm_ip_out_";
static unsigned int ticker;
static const int PIPE_READ_FD = 0;
static const int PIPE_WRITE_FD = 1;
static const unsigned int MAX_CONNECTIONS = 1;

static bool g_one_time_init_complete = false;

typedef enum
{
	MSG_EXIT = 0,
	MSG_MAX
}ip_out_message_t;

static void * ip_out_thread_launcher(void * data)
{
    ip_out_client * ptr = (ip_out_client *) data;
    ptr->worker_thread();
    return NULL;
}

ip_out_client::ip_out_client(q_mgr *mgr) : audio_capture_client(mgr), m_listen_fd(-1), m_write_fd(-1), m_num_connections(0)
{
	INFO("Enter\n")
	if(!g_one_time_init_complete)
	{
		/*SIGPIPE must be ignored or process will exit when client closes connection*/
		struct sigaction sig_settings;
		sig_settings.sa_handler = SIG_IGN;
		sigemptyset(&sig_settings.sa_mask); //CID:80675 Intialize the uninit
		sig_settings.sa_flags = 0;  //CID:80675 Intialize the uninit
		sigaction(SIGPIPE, &sig_settings, NULL);
        m_control_pipe[PIPE_READ_FD] = 0; //CID:90206 -  Initialize m_control_pipe
        m_control_pipe[PIPE_WRITE_FD] = 0;
		g_one_time_init_complete = true;
	}
	REPORT_IF_UNEQUAL(0, pipe2(m_control_pipe, O_NONBLOCK));
	open_output();
}

ip_out_client::~ip_out_client()
{
	INFO("Enter\n")
	close_output();
	close(m_control_pipe[PIPE_WRITE_FD]);
	close(m_control_pipe[PIPE_READ_FD]);
}

int ip_out_client::data_callback(audio_buffer *buf)
{
	lock();
	if(0 < m_write_fd)
	{
		int ret = write(m_write_fd, buf->m_start_ptr, buf->m_size);
		if(0 > ret)
		{
			WARN("Write error! Closing socket. errno: 0x%x\n", errno);
			perror("ip_out_client::data_callback() ");
			close(m_write_fd);
			m_write_fd = -1;
			m_num_connections--;
			//audio_capture_client::stop();
		}
		else if(ret != buf->m_size)
		{
			WARN("Incomplete buffer write!\n");
		}
	}
	unlock();
	release_buffer(buf);
	return 0;  //CID:88863 ; Missing Return
}

std::string ip_out_client::get_data_path()
{
	return m_data_path;
}

std::string ip_out_client::open_output()
{
	lock();
	if(!m_data_path.empty())
	{
		WARN("Already open.\n");
		unlock();
		return m_data_path;	
	}

	/*Open new UNIX socket to transfer data*/
	m_listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0); //TODO: Does it really need to be non-blocking?
	if (m_listen_fd < 0) {
		ERROR("Could not open socket.\n");
		unlock();
		return m_data_path;
	}
	DEBUG("Socket created.\n");

	unsigned int num_retries = 6;
	errno_t rc = -1;
	while(num_retries)
	{
		num_retries--;
		std::string sockpath = SOCKNAME_PREFIX + get_suffix(ticker++);

		struct sockaddr_un bind_path;
		bind_path.sun_family = AF_UNIX;
		rc = strcpy_s(bind_path.sun_path, sizeof(bind_path.sun_path), sockpath.c_str());
		if(rc != EOK)
		{
			ERR_CHK(rc);
		}

		INFO("Binding to path %s\n", bind_path.sun_path);
		int ret = bind(m_listen_fd, (const struct sockaddr *) &bind_path, sizeof(bind_path));
		if(-1 == ret)
		{
			if(EADDRINUSE == errno)
			{
				WARN("Retrying as the path is already in use.\n");
				continue;
			}
			ERROR("Failed to bind to path. Error is %d\n", errno);
			perror("bind error");
			close(m_listen_fd);
			m_listen_fd = -1;
			break;
		}
		else
		{
			INFO("Bound successfully to path.\n");
			m_data_path = sockpath;
			REPORT_IF_UNEQUAL(0, listen(m_listen_fd, 3));
			REPORT_IF_UNEQUAL(0, pthread_create(&m_thread, NULL, ip_out_thread_launcher, (void *) this));
			break;
		}
	}

	unlock();
	return m_data_path;

}

void ip_out_client::close_output()
{
	INFO("Enter\n");
	lock();

	/*Shut down worker thread that listens to incoming connections.*/
	int message = MSG_EXIT;
	int ret = write(m_control_pipe[PIPE_WRITE_FD], &message, sizeof(message));
	if(ret != sizeof(message))
	{
		ERROR("Couldn't trigger worker thread shutdown.\n");
	}
	else
	{
		INFO("Waiting for worker thread to join.\n");
		REPORT_IF_UNEQUAL(0, pthread_join(m_thread, NULL));
		INFO("Worker thread has joined.\n");
	}
	if(!m_data_path.empty())
	{
		if(0 < m_write_fd)
		{
			close(m_write_fd);
			m_write_fd = -1;
			m_num_connections--;
			INFO("Closed write fd. Total active connections now is %d\n", m_num_connections);
		}
		INFO("Removing named socket %s.\n", m_data_path.c_str());
		unlink(m_data_path.c_str());
		m_data_path.clear();
	}
	unlock();
	INFO("Exit\n");
}

void ip_out_client::process_new_connection()
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
#if 0		
		if(!m_is_enabled)
		{
			INFO("Starting data delivery.\n");
			audio_capture_client::start();
		}
#endif
	}
	unlock();
	INFO("Exit\n");
	return;
}

void ip_out_client::worker_thread()
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
				INFO("Exiting monitor thread.\n");
				break;
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