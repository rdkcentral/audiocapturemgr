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
#include <fstream>
#include <string.h>
#include "safec_lib.h"

using namespace audiocapturemgr;
static const char * instance_name = NULL;
void print_menu(void)
{
	std::cout<<"\n--- audio capture test application menu ---\n";
	std::cout<<"1. open music-id session\n";
	std::cout<<"2. print session details\n";
	std::cout<<"3. get default props\n";
	std::cout<<"4. set audio props\n";
	std::cout<<"5. get output props\n";
	std::cout<<"6. start\n";
	std::cout<<"7. stop\n";
	std::cout<<"8. close music session.\n";
	std::cout<<"9. set output props.\n";
	std::cout<<"10. get audio props.\n";
	std::cout<<"11. get precaptured sample.\n";
	std::cout<<"12. get post-captured sample.\n";
	std::cout<<"13. quit.\n";
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

void iarmEventHandler(const char *owner, IARM_EventId_t eventId, void *data, size_t len)
{
	std::cout<<"Received IARM event.\n";
	iarmbus_notification_payload_t * param = static_cast <iarmbus_notification_payload_t *> (data);
	std::string filename = param->dataLocator;
	std::cout<<"Sample "<<filename<<" is ready.\n";
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
				param.details.arg_open.output_type = BUFFERED_FILE_OUTPUT;
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
				std::cout<<"Max supported duration is "<<param.details.arg_output_props.output.max_buffer_duration<<std::endl;
				break;

			case 6:
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
				break;

			case 9:
				param.session_id = session;
				param.details.arg_output_props.output.buffer_duration = 10;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_SET_OUTPUT_PROPERTIES, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
					break;
				}
				break;
				
			case 11:
				param.session_id = session;
				param.details.arg_sample_request.duration = 5; //irrelevant.
				param.details.arg_sample_request.is_precapture = true;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_REQUEST_SAMPLE, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
					break;
				}
				std::cout<<"Requested precaptured sample.\n";
				break;

			case 12:
				param.session_id = session;
				param.details.arg_sample_request.duration = 10;
				param.details.arg_sample_request.is_precapture = false;
				ret = IARM_Bus_Call(IARMBUS_AUDIOCAPTUREMGR_NAME, IARMBUS_AUDIOCAPTUREMGR_REQUEST_SAMPLE, (void *) &param, sizeof(param));
				if(!verify_result(ret, param))
				{
					break;
				}
				std::cout<<"Requested post-captured sample.\n";
				break;

			case 13:
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
	
	IARM_Bus_RegisterEventHandler(IARMBUS_AUDIOCAPTUREMGR_NAME, DATA_CAPTURE_IARM_EVENT_AUDIO_CLIP_READY, iarmEventHandler);

	launcher();

	IARM_Bus_RemoveEventHandler(IARMBUS_AUDIOCAPTUREMGR_NAME, DATA_CAPTURE_IARM_EVENT_AUDIO_CLIP_READY, iarmEventHandler);
	IARM_Bus_Disconnect();
	IARM_Bus_Term();
    return 0;
}
