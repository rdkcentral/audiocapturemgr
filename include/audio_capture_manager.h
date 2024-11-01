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
#ifndef _AUDIO_CAPTURE_MANAGER_H_
#define _AUDIO_CAPTURE_MANAGER_H_
#include <pthread.h>
#include <vector>
#include <semaphore.h>
#include <string>
#include <thread>
#include <condition_variable>
#include <mutex>
#include "audio_buffer.h"
#include "basic_types.h"
#include "media-utils/audioCapture/rmfAudioCapture.h"

/**
 * @defgroup AUDIO_CAPTURE_MANAGER audiocapturemgr
 *
 * - Audio capture manager presents the audio data to registered applications.
 * - Supports creation of audio clips from currently streaming content.
 * - Creates clips of audio from content currently viewing so that it can be used for song identification service.
 * - The Data Capture Service Manager API is used to create audio clip and post/stream to Music ID server (specified URL.)
 * - When Service Manager API is called to create audio clip, settop creates audio clip of specified duration.
 * - Used in Bluetooth audio streaming
 *
 * @defgroup AUDIO_CAPTURE_MANAGER_API Audio Capture Manager Data Types and API(s)
 * This file provides the data types and API(s) used by the audiocapturemgr.
 * @ingroup  AUDIO_CAPTURE_MANAGER
 *
 */

/**
 * @addtogroup AUDIO_CAPTURE_MANAGER_API
 * @{
 */

namespace audiocapturemgr
{
	typedef struct
	{
		racFormat format;
		racFreq sampling_frequency;
		size_t fifo_size;
		size_t threshold;
		unsigned int delay_compensation_ms;
	}audio_properties_t;
	void get_individual_audio_parameters(const audio_properties_t &audio_props, unsigned int &sampling_rate, unsigned int &bits_per_sample, unsigned int &num_channels);
	unsigned int calculate_data_rate(const audio_properties_t &audio_props);
	std::string get_suffix(unsigned int ticker);
}

class audio_capture_client;
class q_mgr
{
	private:
		std::vector <audio_buffer *> *m_current_incoming_q;
		std::vector <audio_buffer *> *m_current_outgoing_q;
		std::vector <audio_capture_client *> m_clients;
		audiocapturemgr::audio_properties_t m_audio_properties;
		unsigned int m_bytes_per_second;
		unsigned int m_inflow_byte_counter; // It's okay if this rolls over.
		unsigned int m_num_clients;
		pthread_mutex_t m_q_mutex;
		pthread_mutex_t m_client_mutex;
		sem_t m_sem;
		pthread_t m_thread;
		bool m_processing_thread_alive;
		bool m_notify_new_data;
		bool m_started;
		RMF_AudioCaptureHandle m_device_handle;
		unsigned int m_max_queue_size;

		std::thread m_data_monitor_thread;
		std::mutex m_data_monitor_mutex;
		std::condition_variable m_data_monitor_cv;
		bool m_stop_data_monitor;

	private:
		inline void lock(pthread_mutex_t &mutex);
		inline void unlock(pthread_mutex_t &mutex);
		inline void notify_data_ready();
		void swap_queues(); //caller must lock before invoking this.
		void flush_queue(std::vector <audio_buffer *> *q);
		void flush_system();
		void process_data();
		void update_buffer_references();
		void data_monitor();

	public:
		q_mgr();
		~q_mgr();

		/**
		 *  @brief This API is used to set the audio properties to the client device.
		 *
		 *  Properties like format,sampling_frequency,fifo_size,threshold,delay_compensation_ms.
		 *  Checks if audio playback is started, restarted the client device after settings are applied.
		 *
		 *  @param[in]  in_properties   Structure which holds the audio properties.
		 *
		 *  @return 0 on success, appropiate errorcode otherwise.
		 */
		int set_audio_properties(audiocapturemgr::audio_properties_t &in_properties);

		/**
		 *  @brief This API returns the current audio properties of the device.
		 *
		 *  @param[out]  out_properties  Structure which holds the audio properties.
		 */
		void get_audio_properties(audiocapturemgr::audio_properties_t &out_properties);

		/**
		 *  @brief This function will return default RMF_AudioCapture_Settings settings.
		 *
		 *  Once AudioCaptureStart gets called with RMF_AudioCapture_Status argument this
		 *  should still continue to return the default capture settings.
		 *
		 *  @param[out]  out_properties  Structure which holds the audio properties.
		 *
		 *  @return Device Settings error code
		 *  @retval dsERR_NONE Indicates dsGetAudioPort API was successfully called using iarmbus call.
		 *  @retval dsERR_GENERAL Indicates error due to general failure.
		 */
		void get_default_audio_properties(audiocapturemgr::audio_properties_t &out_properties);

		/**
		 * @brief Returns data rate in bytes per second.
		 *
		 * The data rate is a term to denote the transmission speed, or the number of bytes per second transferred.
		 * Datarate is calculated as  bits_per_sample * sampling_rate * num_channels / 8;
		 *
		 * @returns Number of bytes per second transferred.
		 */
		unsigned int get_data_rate();

		/**
		 * @brief This API creates new audio buffer and pushes the data to the queue.
		 *
		 * @param[in]  buf      Data to be inserted into the queue.
		 * @param[in]  size     size of the buffer.
		 *
		 */
		void add_data(unsigned char *buf, unsigned int size);

		/**
		 * @brief This API processes the audio buffers available in the queue.
		 *
		 */
		void data_processor_thread();

		/**
		 * @brief This API registers the client.
		 *
		 * Client is the consumer of audio data.
		 *
		 * @param[in]  client  Client info for registering.
		 *
		 * @return 0 on success, appropiate errorcode otherwise.
		 */
		int register_client(audio_capture_client *client);

		/**
		 * @brief Removes the client.
		 *
		 * @param[in]  client  Client info for registering.
		 *
		 * @return 0 on success, appropiate errorcode otherwise.
		 */
		int unregister_client(audio_capture_client *client);

		/**
		 * @brief This function will start the Audio capture.
		 *
		 * If Settings is not null, reconfigure the settings with the provided capture settings and starts the audio capture .
		 * If it is NULL, start with the default capture settings.
		 *
		 * @return Returns 0 on success, appropiate error code otherwise.
		 */
		int start();

		/**
		 *  @brief This function will stop the audio capture.
		 *
		 *  Start can be called again after a Stop, as long as Close has not been called.
		 *
		 *  @return Returns 0 on success, appropiate error code otherwise.
		 */
		int stop();

		/**
		 *  @brief This function invokes an API for adding data to the audio buffer.
		 *
		 *  @param[in]  context  data callback context
		 *  @param[in]  buf      Data to be inserted into the queue.
		 *  @param[in]  size     size of the buffer.
		 *
		 *  @return Returns RMF_SUCCESS on success, appropiate error code otherwise.
		 */
		static rmf_Error data_callback(void *context, void *buf, unsigned int size);
};

class audio_capture_client
{

	private:
		unsigned int m_priority;
		pthread_mutex_t m_mutex;

	protected:
		q_mgr * m_manager;
		void release_buffer(audio_buffer *ptr);
		void lock();
		void unlock();

	public:
		virtual int data_callback(audio_buffer *buf);
		audio_capture_client(q_mgr * manager);
		virtual ~audio_capture_client();
		unsigned int get_priority() {return m_priority;}
		void set_manager(q_mgr *manager);
		virtual int set_audio_properties(audiocapturemgr::audio_properties_t &properties);
		virtual void get_audio_properties(audiocapturemgr::audio_properties_t &properties);
		void get_default_audio_properties(audiocapturemgr::audio_properties_t &properties);
		virtual void notify_event(audio_capture_events_t event){}
		virtual int start();
		virtual int stop();
};

/**
 * @}
 */

#endif // _AUDIO_CAPTURE_MANAGER_h_
