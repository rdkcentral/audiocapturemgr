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
#include <iostream>
#include "audiocapturemgr_iarm.h"
#include "libIBus.h"
#include <pthread.h>
#include <fstream>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>
#include "safec_lib.h"

using namespace audiocapturemgr;
static const char * instance_name = NULL;
void print_menu(void)
{
	std::cout<<"\n--- audio capture test application menu ---\n";
	std::cout<<"1. open IPOUT session\n";
	std::cout<<"2. print session details\n";
	std::cout<<"3. get default props\n";
	std::cout<<"4. set audio props\n";
	std::cout<<"5. get output props\n";
	std::cout<<"6. start\n";
	std::cout<<"7. stop\n";
	std::cout<<"8. close IPOUT session.\n";
	std::cout<<"9. reconnect to known socket.\n";
	std::cout<<"10. get audio props.\n";
	std::cout<<"11. quit.\n";
}

static bool verify_result(IARM_Result_t ret, iarmbus_acm_arg_t &param)
{
	if(IARM_RESULT_SUCCESS != ret)
	{
		std::cout<<"Bus call failed.\n";
		return false;
	}
	if(0 != param.result)
	{
		std::cout<<"ACM implementation of bus call failed.\n";
		return false;
	}
	return true;
}

void * read_thread(void * data)
{
	std::string socket_path = *(static_cast <std::string *> (data));
	if(socket_path.empty())
	{
		std::cout<<"read thread returning as socket path is empty.\n";
		return NULL;
	}
	std::cout<<"Connecting to socket "<<socket_path<<std::endl;

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path.c_str(), (socket_path.size() + 1));

	int read_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(0 > read_fd)
	{
		std::cout<<"Couldn't create read socket. Exiting.\n";
		return NULL;
	}

	if(0 != connect(read_fd, (const struct sockaddr *) &addr, sizeof(addr)))
	{
		std::cout<<"Couldn't connect to the path. Exiting.\n";
		perror("read_thread");
		close(read_fd);
		return NULL;
	}

	std::cout<<"Connection established.\n";
	unsigned int recvd_bytes = 0;
	char buffer[1024];
	std::string filename = "/opt/acm_ipout_dump_";
	filename += instance_name;
	std::ofstream file_dump(filename.c_str(), std::ios::binary);
	while(true)
	{
		int ret = read(read_fd, buffer, 1024);
		if(0 == ret)
		{
			std::cout<<"Zero bytes read. Exiting.\n";
			break;
		}
		else if(0 > ret)
		{
			std::cout<<"Error reading from socket. Exiting.\n";
			perror("read error");
			break;
		}
		if((1024*1024*2) > recvd_bytes) //Write up to 2 MB to a file
		{
			file_dump.write(buffer, ret);	
		}
		recvd_bytes += ret;
	}
	
	close(read_fd);
	std::cout<<"Number of bytes read: "<<recvd_bytes<<std::endl;
	file_dump.seekp(0, std::ios_base::end);
	std::cout<<file_dump.tellp()<<" bytes written to file.\n";
	std::cout<<"Exiting read thread.\n";
	return NULL;
}

void connect_and_read_data(std::string &socket_path)
{
	pthread_t thread;
	int ret = pthread_create(&thread, NULL, read_thread, (void *) &socket_path);
	if(0 == ret)
	{
		std::cout<<"Successfully launched read thread.\n";
	}
	else
	{
		std::cout<<"Failed to launch read thread.\n";
	}
}

void launcher()
{
	bool keep_running = true;
	session_id_t session = -1;
	audio_properties_ifce_t props;
	std::string socket_path;

	while(keep_running)
	{
		int choice = 0;
		std::string filename;
		iarmbus_acm_arg_t param;
		IARM_Result_t ret;

		print_menu();
		std::cout<<"Enter command:\n";
		if(!(std::cin >> choice))
		{
			std::cout<<"Oops!\n";
			std::cin.clear();
			std::cin.ignore(10000, '\n');
			continue;
        }
		switch(choice)
		{
			case 1:
				param.details.arg_open.source = 0; //primary
				param.details.arg_open.output_type = REALTIME_SOCKET;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_OPEN, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
					break;
				}
				session = param.session_id;
				std::cout<<"Opened new session "<<session<<std::endl;
				break;

			case 2:
				std::cout<<"Session ID is "<<session<<std::endl;
				std::cout<<"Socket path is "<<socket_path<<std::endl;
				break;

			case 3:
				param.session_id = session;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_GET_DEFAULT_AUDIO_PROPS, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
					break;
				}
				INFO("Format: 0x%x, delay comp: %dms, fifo size: %d, threshold: %d\n", 
						param.details.arg_audio_properties.format,
						param.details.arg_audio_properties.delay_compensation_ms,
						param.details.arg_audio_properties.fifo_size,
						param.details.arg_audio_properties.threshold);
				props = param.details.arg_audio_properties;
				break;

			case 10:
				param.session_id = session;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_GET_AUDIO_PROPS, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
					break;
				}
				INFO("Format: 0x%x, delay comp: %dms, fifo size: %d, threshold: %d\n", 
						param.details.arg_audio_properties.format,
						param.details.arg_audio_properties.delay_compensation_ms,
						param.details.arg_audio_properties.fifo_size,
						param.details.arg_audio_properties.threshold);
				props = param.details.arg_audio_properties;
				break;

			case 4:
				param.session_id = session;
				param.details.arg_audio_properties = props;
				param.details.arg_audio_properties.delay_compensation_ms = 190;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_SET_AUDIO_PROPERTIES, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
					break;
				}
				break;

			case 5:
				param.session_id = session;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_GET_OUTPUT_PROPS, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
					break;
				}
				socket_path = std::string(param.details.arg_output_props.output.file_path);
				std::cout<<"Output path is "<<socket_path<<std::endl;
				break;

			case 6:
				if(socket_path.empty())
				{
					std::cout<<"No path to socket available.\n";
					break;
				}
				std::cout<<"Launching read thread.\n";
				connect_and_read_data(socket_path);
				param.session_id = session;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_START, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
					break;
				}
				std::cout<<"Start procedure complete.\n";
				break;

			case 7:
				param.session_id = session;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_STOP, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
					break;
				}
				std::cout<<"Stop procedure complete.\n";
				break;

			case 8:
				param.session_id = session;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_CLOSE, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
					break;
				}
				std::cout<<"Close procedure complete.\n";
				session = -1;
				socket_path.clear();
				break;

			case 9:
				if(socket_path.empty())
				{
					std::cout<<"No path to socket available.\n";
					break;
				}
				std::cout<<"Launching read thread.\n";
				connect_and_read_data(socket_path);
				break;
				
			case 11:
				std::cout<<"Exiting.\n";
				keep_running = false;
				break;

			default:
				std::cout<<"Unknown input!\n";
		}
	}
}


#define ACM_TESTAPP_NAME_PRFIX "acm_testapp_"
int main(int argc, char *argv[])
{
	if(2 > argc)
	{
		std::cout<<"Each exec session needs a name. Invoke it as $<exec name> <session-name>"<<std::endl;
		std::cout<<"For instance, $acm_ipout_testapp alpha\n";
		return -1;
	}
	errno_t rc = -1;
	instance_name = argv[1];
	char bus_registration_name[100];
	rc = sprintf_s(bus_registration_name, sizeof(bus_registration_name), "%s%s", ACM_TESTAPP_NAME_PRFIX, argv[1]);
	if( rc < EOK )
	{
		ERR_CHK(rc);
		return -1;
	}

    if(0 != IARM_Bus_Init(bus_registration_name))
	{
		std::cout<<"Unable to init IARMBus. Try another session name.\n";
		return -1;
	}
	if(0 != IARM_Bus_Connect())
	{
		std::cout<<"Unable to connect to IARBus\n";
		return -1;
	}
	
	launcher();

	IARM_Bus_Disconnect();
	IARM_Bus_Term();
    return 0;
}
