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
#include "audio_capture_manager.h"
#include "music_id.h"
#include "ip_out.h"
#include "audiocapturemgr_iarm.h"
#include <vector>
#include <list>

/**
 * @addtogroup AUDIO_CAPTURE_MANAGER_API
 * @{
 */

typedef struct
{
	int session_id;
	audio_capture_client * client;
	q_mgr * source;
	bool enable;
	audiocapturemgr::iarmbus_output_type_t output_type;
}acm_session_t;

class acm_session_mgr
{
	private:
	std::list <acm_session_t *> m_sessions;
	std::vector <q_mgr *> m_sources;
	pthread_mutex_t m_mutex;
	int m_session_counter;

	public:
	acm_session_mgr();
	~acm_session_mgr();
	static acm_session_mgr * get_instance();

    /**
     *  @brief This API initializes the message bus, registers event, RPC methods to be used
     *  by other applications.
     *
     *  RPC methods like
     *     - request audiocapture sample
     *     - open
     *     - close
     *     - start
     *     - stop
     *     - get default audio properties
     *     - get audio properties
     *     - get output properties
     *     - set audio  properties
     *     - set output properties
     *
     *  @return Returns 0 on success, appropriate error code otherwise.
     */
	int activate();

    /**
     *  @brief This API disconnects application from the message bus, releases memory.
     *
     *  Also, unregisters the client from the application.
     *
     *  @return Returns 0 on success, appropriate error code otherwise.
     */
	int deactivate();

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

	int generic_handler(void * arg);

    /**
     *  @brief This API creates the music id session. It's also used by bluetooth manager as an audio source. 
     *
     *  Accepts only one instance of music id client per source type.
     *
     *  @param[in] arg  Structure variable that holds the details like audio source, output types etc.
     *
     *  @return Returns 0 on success, appropriate error code otherwise.
     */
	int open_handler(void * arg);

    /**
     *  @brief This API destroys the music id session, bluetooth manager session.
     *
     *  @param[in] arg  Structure variable that holds the details like audio source, output types etc.
     *
     *  @return Returns 0 on success, appropriate error code otherwise.
     */
	int close_handler(void * arg);

    /**
     *  @brief This API returns default capture settings.
     *
     *  Settings like format, sampling frequency, FIFO size, threshold etc.
     *
     *  @param[in] arg Indicates the audio properties.
     *
     *  @return Returns 0 on success, appropriate error code otherwise.
     */
	int get_default_audio_props_handler(void * arg);

    /**
     *  @brief This API returns the audio properties of a current session.
     *
     *  @param[in] arg Indicates the audio properties.
     *
     *  @return Returns 0 on success, appropriate error code otherwise.
     */
	int get_audio_props_handler(void * arg);

    /**
     *  @brief This API returns the output  properties of a current session.
     *
     *  Depending on the output type socket or file output.
     *
     *  @param[in] arg Indicates the output  properties.
     *
     *  @return Returns 0 on success, appropriate error code otherwise.
     */
	int get_output_props_handler(void * arg);

    /**
     *  @brief This API is used to set the audio properties of a current session.
     *
     *  Properties like Format, Frequency, FIFO size, threshold, delay etc.
     *
     *  @param[in] arg Indicates the audio  properties to  set.
     *
     *  @return Returns 0 on success, appropriate error code otherwise.
     */
	int set_audio_props_handler(void * arg);

    /**
     *  @brief This API is to set precapture duration of a client device.
     *
     *  @param[in] arg Indicates the output type.
     *
     *  @return Returns 0 on success, appropriate error code otherwise.
     */
	int set_output_props_handler(void * arg);

    /**
     *  @brief This API starts the client session.
     *
     *  @param[in] arg IARM bus arguments.
     *
     *  @return Returns 0 on success, appropriate error code otherwise.
     */
	int start_handler(void * arg);

    /**
     *  @brief This API stops the current session.
     *
     *  @param[in] arg IARM bus arguments.
     *
     *  @return Returns 0 on success, appropriate error code otherwise.
     */
	int stop_handler(void * arg);

    /**
     *  @brief Function to add prefix to the audio filename.
     *
     *  @param[in] prefix Prefix to be added.
     */
	void set_filename_prefix(std::string &prefix);

	private:
	q_mgr * get_source(int source);
	void lock();
	void unlock();
	acm_session_t * get_session(int session_id);
};

/**
 * @}
 */


