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
#include "music_id.h"

/**
 * @addtogroup AUDIO_CAPTURE_MANAGER_API
 * @{
 */

class acm_iarm_interface
{
	private:
	bool m_is_active;
	bool m_enable_audio_input;
	music_id_client * m_client;

	public:
	acm_iarm_interface();
	~acm_iarm_interface();
	static acm_iarm_interface * get_instance();

    /**
     *  @brief This API initialize the message bus, registers event, RPC methods that can be used
     *  by other applications.
     *
     *  @param[in] client Indicates the id ofthe client device.
     *
     *  @return Returns 0 on success, appropriate error code otherwise.
     */
	int activate(music_id_client * client);

    /**
     *  @brief This API disconnects application from the message bus, releases memory.
     *
     *  Also, unregisters the client from the application.
     *
     *  @return Returns 0 on success, appropriate error code otherwise.
     */
	int deactivate();

    /**
     *  @brief This API starts the audio capture.
     *
     *  If incoming duration is 0, turn off precapturing otherwise turn it back on and set duration.
     *
     *  @param[in] arg  Payload data
     *
     *  @return Returns ACM_RESULT_SUCCESS on success, appropiate error code otherwise.
     */
	int enable_capture_handler(void * arg);

    /**
     *  @brief This API grabs the precaptured sample, if the requested data has precapture flag true.
     *  Otherwise, capture fresh sample.
     *
     *  @param[in] arg  Payload data
     *
     *  @return Returns ACM_RESULT_GENERAL_FAILURE on failure, 0 on success.
     *
     */
	int get_sample_handler(void * arg);

    /**
     *  @brief Function to add prefix to the audio filename.
     *
     *  @param[in] prefix Prefix to be added.
     */
	void set_filename_prefix(std::string &prefix);
};

/**
 * @}
 */


