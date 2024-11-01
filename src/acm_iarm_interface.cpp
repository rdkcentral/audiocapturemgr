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
#include "acm_iarm_interface.h"
#include "audiocapturemgr_iarm.h"
#include <string>
#include <string.h>
#include <sstream>
#include "libIARM.h"
#include "libIBus.h"
#include "safec_lib.h"

using namespace audiocapturemgr;



static acm_iarm_interface g_singleton;

static unsigned int ticker = 0;
std::string audio_filename_prefix = AUDIOCAPTUREMGR_FILENAME_PREFIX;
std::string audio_file_path = AUDIOCAPTUREMGR_FILE_PATH;
static std::string get_suffix()
{
    std::ostringstream stream;
    stream << ticker++;
    std::string outstring = stream.str();
    return outstring;
}

static void request_callback(void * data, std::string &file, int result)
{
	errno_t rc = -1;
	if(0 != result)
	{
		ERROR("Failed to grab sample.\n");
	}
	else
	{
		iarmbus_notification_payload_t payload;
		rc = strcpy_s(payload.dataLocator, sizeof(payload.dataLocator), file.c_str());
		if(rc == EOK)
		{
			int ret = IARM_Bus_BroadcastEvent(IARMBUS_AUDIOCAPTUREMGR_NAME, DATA_CAPTURE_IARM_EVENT_AUDIO_CLIP_READY, &payload, sizeof(payload));
			REPORT_IF_UNEQUAL(IARM_RESULT_SUCCESS, ret);
		}
		else
		{
			ERR_CHK(rc);
			WARN("Incoming filename is too big for payload buffer.\n");
		}
	}
}

static IARM_Result_t enable_capture(void * arg)
{
	g_singleton.enable_capture_handler(arg);
	return IARM_RESULT_SUCCESS;
}
static IARM_Result_t request_sample(void * arg)
{
	g_singleton.get_sample_handler(arg);
	return IARM_RESULT_SUCCESS;
}

acm_iarm_interface::acm_iarm_interface() : m_is_active(false), m_enable_audio_input(false),  m_client(NULL)
{
	INFO("Enter\n");
}
acm_iarm_interface::~acm_iarm_interface()
{
	INFO("Enter\n");
}

acm_iarm_interface * acm_iarm_interface::get_instance()
{
	return &g_singleton;
}

int acm_iarm_interface::activate(music_id_client * client)
{
	int ret;
	INFO("Enter\n");
	if(NULL != m_client)
	{
		ERROR("ACM IARM interface is already in use by another client.\n");
		ret = -1;
	}
	else
	{
		m_client = client;
	}

	//TODO: add early exit for each of the failures below
	ret = IARM_Bus_Init(IARMBUS_AUDIOCAPTUREMGR_NAME);
	REPORT_IF_UNEQUAL(IARM_RESULT_SUCCESS, ret);

	ret = IARM_Bus_Connect();
	REPORT_IF_UNEQUAL(IARM_RESULT_SUCCESS, ret);

	ret =  IARM_Bus_RegisterEvent(IARMBUS_MAX_ACM_EVENT);
	REPORT_IF_UNEQUAL(IARM_RESULT_SUCCESS, ret);

	ret = IARM_Bus_RegisterCall(IARMBUS_AUDIOCAPTUREMGR_ENABLE, enable_capture);
	REPORT_IF_UNEQUAL(IARM_RESULT_SUCCESS, ret);

	ret = IARM_Bus_RegisterCall(IARMBUS_AUDIOCAPTUREMGR_REQUEST_SAMPLE, request_sample);
	REPORT_IF_UNEQUAL(IARM_RESULT_SUCCESS, ret);

	return ret;
}
int acm_iarm_interface::deactivate()
{
	int ret;
	INFO("Enter\n");
	ret = IARM_Bus_Disconnect();
	REPORT_IF_UNEQUAL(IARM_RESULT_SUCCESS, ret);
	ret = IARM_Bus_Term();
	REPORT_IF_UNEQUAL(IARM_RESULT_SUCCESS, ret);
	if(m_enable_audio_input)
	{
		m_client->stop();
		m_enable_audio_input = false;
	}
	m_client = NULL;
	return ret;
}

int acm_iarm_interface::enable_capture_handler(void * arg)
{
	iarmbus_enable_payload_t *data = static_cast <iarmbus_enable_payload_t *> (arg);
	unsigned int incoming_duration = static_cast <int> (data->max_duration);
	unsigned int max_supported_duration = m_client->get_max_supported_duration();

	if(incoming_duration > max_supported_duration)
	{
		ERROR("Incoming duration %d is too long.\n", incoming_duration);
		data->result = max_supported_duration; // Return the max supported duration if incoming value is too high.
	}
	else
	{
		//TODO: if incoming is 0, turn off precapturing. If it isn't, turn it back on and set duration.	
		m_client->set_precapture_duration(incoming_duration);
		if(0 == incoming_duration)
		{
			if(true == m_enable_audio_input)
			{
				INFO("Disabling audio capture.\n");
				m_client->stop();
				m_enable_audio_input = false;
			}
		}
		else if(false == m_enable_audio_input)
		{
			INFO("Enabling audio capture.\n");
			m_client->start();
			m_enable_audio_input = true;
		}
		data->result = ACM_RESULT_SUCCESS;
	}
	return 0;
}


int acm_iarm_interface::get_sample_handler(void * arg)
{
	int ret = 0;
	iarmbus_request_payload_t *data = static_cast <iarmbus_request_payload_t *> (arg);
	if(false == m_enable_audio_input)
	{
		ERROR("Audio capture is currently disabled!\n");
		data->result = ACM_RESULT_DURATION_OUT_OF_BOUNDS;
		return 0;
	}
	data->result = ACM_RESULT_SUCCESS;
	std::string filename = audio_file_path + audio_filename_prefix + get_suffix();

	if(data->is_precapture)
	{
		ret = m_client->grab_precaptured_sample(filename);
		if(0 == ret)
		{
			/* Precapture is immediate. Notify now.*/
			request_callback(NULL, filename, 0);
		}
	}
	else
	{
		unsigned int duration = static_cast <unsigned int> (data->duration);	
		if(duration <= m_client->get_max_supported_duration())
		{
			ret = m_client->grab_fresh_sample(filename, duration, &request_callback, NULL);
		}
		else
		{
			ERROR("Duration out of bounds!\n");
			data->result = ACM_RESULT_DURATION_OUT_OF_BOUNDS;
		}
	}
	/*Note: tricky condition check. ret is also zero if it's an out-of-bounds error, 
	 * but in that case, result is already updated.*/
	if(0 != ret)
	{
		data->result = ACM_RESULT_GENERAL_FAILURE;
	}
	return 0;
}

void acm_iarm_interface::set_filename_prefix(std::string &prefix)
{
	audio_filename_prefix = prefix;	
}
