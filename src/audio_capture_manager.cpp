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
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include "audio_capture_manager.h"
#include <algorithm>
#include <chrono>
#include <unistd.h>
#include "rmfAudioCapture.h"

using namespace audiocapturemgr;

const unsigned int QUEUE_CHECK_LOOP_TIME_US = 1000 * 50; //50ms
static const size_t DEFAULT_FIFO_SIZE = 64 * 1024;
static const size_t	DEFAULT_THRESHOLD = 8 * 1024;
static const unsigned int DEFAULT_DELAY_COMPENSATION = 0;
static const unsigned int MAX_QMGR_BUFFER_DURATION_S = 30; //safe maximum, beyond which the queue will be flushed without waiting for buffers to be consumed.

static void * q_mgr_thread_launcher(void * data)
{
	q_mgr * ptr = (q_mgr *) data;
	ptr->data_processor_thread();
	return NULL;
}

rmf_Error q_mgr::data_callback(void *context, void *buf, unsigned int size)
{
	((q_mgr *)context)->add_data((unsigned char*)buf, size);
	return RMF_SUCCESS;
}


inline void q_mgr::lock(pthread_mutex_t &mutex)
{
	REPORT_IF_UNEQUAL(0, pthread_mutex_lock(&mutex));
}
inline void q_mgr::unlock(pthread_mutex_t &mutex)
{
	REPORT_IF_UNEQUAL(0, pthread_mutex_unlock(&mutex));
}
inline void q_mgr::notify_data_ready()
{
	DEBUG("Enter.\n");
	if(m_notify_new_data)
	{
		DEBUG("Posting semaphore.\n");
		REPORT_IF_UNEQUAL(0, sem_post(&m_sem));
		m_notify_new_data = false;
	}
}
void q_mgr::swap_queues() //caller must lock before invoking this.
{
	if(!m_current_outgoing_q->empty())
	{
		WARN("Outgoing queue wasn't empty. Flushing it now.\n");
		flush_queue(m_current_outgoing_q);
	}
	std::vector <audio_buffer *> * temp = m_current_incoming_q;
	m_current_incoming_q = m_current_outgoing_q;
	m_current_outgoing_q = temp;
}

void q_mgr::flush_queue(std::vector <audio_buffer *> *q)
{
	DEBUG("Flushing queue.\n");
	std::vector <audio_buffer *>::iterator iter;
	for(iter = q->begin(); iter != q->end(); iter++)
	{
		free_audio_buffer(*iter);	
	}
	q->clear();
}

namespace audiocapturemgr
{
	void get_individual_audio_parameters(const audio_properties_t &audio_props, unsigned int &sampling_rate, unsigned int &bits_per_sample, unsigned int &num_channels)
	{
		bits_per_sample = 0;
		sampling_rate= 0;
		num_channels = 0;

		switch(audio_props.sampling_frequency)
		{
			case racFreq_e16000:
				sampling_rate = 16000;
				break;
			case racFreq_e24000:
				sampling_rate = 24000;
				break;
			case racFreq_e32000:
				sampling_rate = 32000;
				break;
			case racFreq_e44100:
				sampling_rate = 44100;
				break;
			case racFreq_e48000:
				sampling_rate = 48000;
				break;
			default:
				ERROR("Bad sampling rate: 0x%x\n", audio_props.sampling_frequency);
				break;
		}

		switch(audio_props.format)
		{
			case racFormat_e16BitStereo:
				bits_per_sample = 16; num_channels = 2;
				break;
			case racFormat_e24BitStereo:
				bits_per_sample = 24; num_channels = 2;
				break;
			case racFormat_e16BitMonoLeft:
				bits_per_sample = 16; num_channels = 1;
				break;
			case racFormat_e16BitMonoRight:
				bits_per_sample = 16; num_channels = 1;
				break;
			case racFormat_e16BitMono:
				bits_per_sample = 16; num_channels = 1;
				break;
			case racFormat_e24Bit5_1:
				bits_per_sample = 24; num_channels = 6;
				break;
			default:
				ERROR("Unknown format 0x%x\n", audio_props.format);
		}
	}

	unsigned int calculate_data_rate(const audio_properties_t &audio_props)
	{
		unsigned int bits_per_sample = 0;
		unsigned int sampling_rate= 0;
		unsigned int num_channels = 0;
		get_individual_audio_parameters(audio_props, sampling_rate, bits_per_sample, num_channels); 	
		unsigned int data_rate = bits_per_sample * sampling_rate * num_channels / 8;
		INFO("Audio properties: %dkHz, %d bit, %d channel, byte rate: %d\n", sampling_rate, bits_per_sample, num_channels, data_rate);
		return data_rate;
	}
	
	std::string get_suffix(unsigned int ticker)
	{
		std::ostringstream stream;
		stream << ticker;
		std::string outstring = stream.str();
		return outstring;
	}
}

q_mgr::q_mgr() : m_inflow_byte_counter(0), m_num_clients(0), m_notify_new_data(false), m_started(false), m_device_handle(NULL), m_stop_data_monitor(true)
{
	INFO("Creating instance 0x%p.\n", static_cast <void *>(this));
	pthread_mutexattr_t mutex_attribute;
	REPORT_IF_UNEQUAL(0, pthread_mutexattr_init(&mutex_attribute));
	REPORT_IF_UNEQUAL(0, pthread_mutexattr_settype(&mutex_attribute, PTHREAD_MUTEX_ERRORCHECK));
	REPORT_IF_UNEQUAL(0, pthread_mutex_init(&m_q_mutex, &mutex_attribute));
	REPORT_IF_UNEQUAL(0, pthread_mutex_init(&m_client_mutex, &mutex_attribute));
	REPORT_IF_UNEQUAL(0, sem_init(&m_sem, 0, 0));
	m_current_incoming_q = new std::vector <audio_buffer *>;
	m_current_outgoing_q = new std::vector <audio_buffer *>; 
	m_processing_thread_alive = false; //CID:80565 :  Initialize bool variable

	REPORT_IF_UNEQUAL(0, pthread_create(&m_thread, NULL, q_mgr_thread_launcher, (void *) this));
	
	int ret = RMF_AudioCapture_Open(&m_device_handle);
	INFO("open() result is 0x%x\n", ret);
	
	RMF_AudioCapture_Settings settings;
	RMF_AudioCapture_GetDefaultSettings(&settings);
	m_audio_properties.format = settings.format;
	m_audio_properties.sampling_frequency = settings.samplingFreq;
	m_audio_properties.fifo_size = DEFAULT_FIFO_SIZE; 
	m_audio_properties.threshold = DEFAULT_THRESHOLD;
	m_audio_properties.delay_compensation_ms = DEFAULT_DELAY_COMPENSATION; 
	m_bytes_per_second = calculate_data_rate(m_audio_properties);
	m_max_queue_size = (MAX_QMGR_BUFFER_DURATION_S * m_bytes_per_second) / m_audio_properties.threshold;
	INFO("Max incoming queue size is now %d\n", m_max_queue_size);
}
q_mgr::~q_mgr()
{
	INFO("Deleting instance 0x%p.\n", static_cast <void *>(this));
	if(true == m_started)
	{
		stop();
	}
    if(NULL != m_device_handle)
    {
		int ret = RMF_AudioCapture_Close(m_device_handle);
		INFO("close() result is 0x%x\n", ret);
		m_device_handle = NULL;
    }
	

	m_processing_thread_alive = false;
	REPORT_IF_UNEQUAL(0, sem_post(&m_sem));
	REPORT_IF_UNEQUAL(0, pthread_join(m_thread, NULL));
	REPORT_IF_UNEQUAL(0, sem_destroy(&m_sem));
	REPORT_IF_UNEQUAL(0, pthread_mutex_destroy(&m_q_mutex));
	REPORT_IF_UNEQUAL(0, pthread_mutex_destroy(&m_client_mutex));
	flush_queue(m_current_incoming_q);
	flush_queue(m_current_outgoing_q);
	delete m_current_incoming_q;
	delete m_current_outgoing_q;
}


int q_mgr::set_audio_properties(audio_properties_t &in_properties)
{
	DEBUG("Enter.\n");
	lock(m_q_mutex);
	if(0 == memcmp(&m_audio_properties, &in_properties, sizeof(m_audio_properties)))
	{
		unlock(m_q_mutex);
		return 0; //No change to properties
	}

	/* Validate incoming parameters.*/
	if((racFormat_eMax <= in_properties.format) || (racFreq_eMax <= in_properties.sampling_frequency))
	{
		ERROR("Bad parameters. Format: 0x%x, sampling freq: 0x%x\n", in_properties.format, in_properties.sampling_frequency);
		unlock(m_q_mutex);
		return -1;
	}

	m_audio_properties = in_properties;

	/*Update data rate.*/
	m_bytes_per_second = calculate_data_rate(m_audio_properties);
	m_max_queue_size = (MAX_QMGR_BUFFER_DURATION_S * m_bytes_per_second) / m_audio_properties.threshold;
	INFO("Max incoming queue size is now %d\n", m_max_queue_size);
	unlock(m_q_mutex);

	lock(m_client_mutex);
	bool needs_restart = m_started;
	if(m_started)
	{
		stop();
	}
	std::vector<audio_capture_client *>::iterator iter;
	for(iter = m_clients.begin(); iter != m_clients.end(); iter++)
	{
		(*iter)->notify_event(AUDIO_SETTINGS_CHANGE_EVENT);
	}
	if(needs_restart)
	{
		INFO("Restarting audio after settings change.\n");
		start();
	}

	unlock(m_client_mutex);
	return 0;
}

void q_mgr::get_audio_properties(audio_properties_t &out_properties)
{
	out_properties = m_audio_properties;
}

void q_mgr::get_default_audio_properties(audio_properties_t &out_properties)
{
	RMF_AudioCapture_Settings settings;
	RMF_AudioCapture_GetDefaultSettings(&settings);
	out_properties.format = settings.format;
	out_properties.sampling_frequency = settings.samplingFreq;
	out_properties.fifo_size = settings.fifoSize;
	out_properties.threshold = settings.threshold;
	out_properties.delay_compensation_ms = settings.delayCompensation_ms;
}
unsigned int q_mgr::get_data_rate()
{
	return m_bytes_per_second;
}

void q_mgr::add_data(unsigned char *buf, unsigned int size)
{
	DEBUG("Adding data.\n");
	lock(m_q_mutex);
	audio_buffer * temp = create_new_audio_buffer(buf, size, 0, m_num_clients);
	if(m_max_queue_size < m_current_incoming_q->size())
	{
		WARN("Incoming queue size over limit. Flushing.\n");
		flush_queue(m_current_incoming_q);
	}
	m_current_incoming_q->push_back(temp);
	//TODO: guard against accumulating audio_buffers if there is no consumption.
	notify_data_ready();
	unlock(m_q_mutex);
	m_inflow_byte_counter += size;
}
void q_mgr::data_processor_thread()
{
	DEBUG("Launching.\n");
	m_processing_thread_alive = true;
	while(m_processing_thread_alive)
	{
		/*
		 * 1. If there are audio_buffers remaining in outbound queue, process them.
		 * 2. If no audio_buffers remain in outbound queue, wait for semaphore.
		 * 3. When semaphore is posted, if data is available in the other queue, swap queues.
		 * */
		DEBUG("Enter processing loop.\n");
		while(!m_current_outgoing_q->empty())
		{
			process_data();	
		}
		DEBUG("No data in outgoing queue.\n");
		/* No more data to be consumed. Gotta check the other queue */
		lock(m_q_mutex);
		if(!m_current_incoming_q->empty())
		{	
			DEBUG("Incoming queue has buffers. Swapping them.\n");
			swap_queues();
			unlock(m_q_mutex);
		}
		else
		{
			/*Block until data is available.*/
			m_notify_new_data = true;
			unlock(m_q_mutex);
			DEBUG("Incoming queue is empty as well. Waiting until a buffer arrives.\n");
			if(0 != sem_wait(&m_sem))
			{
				ERROR("Critical semaphore error. Exiting\n");
				return;
			}
			/* Just in case the semaphore was posted to shut down this thred... */
			if(!m_processing_thread_alive)
			{
				break;
			}

			lock(m_q_mutex);
			DEBUG("New data available in incoming queue. Swapping them.\n");
			swap_queues();
			unlock(m_q_mutex);
		}

	}
	DEBUG("Exiting.\n");
}

void q_mgr::update_buffer_references()
{
	std::vector <audio_buffer *>::iterator buffer_iter;
	audio_buffer_get_global_lock();
	for(buffer_iter = m_current_outgoing_q->begin(); buffer_iter != m_current_outgoing_q->end(); buffer_iter++)
	{
		set_ref_audio_buffer(*buffer_iter, m_num_clients);	
	}
	audio_buffer_release_global_lock();
}

void q_mgr::process_data()
{
	DEBUG("Processing %d buffers of data.\n", m_current_outgoing_q->size());

	lock(m_client_mutex);
	
	update_buffer_references();
	
	std::vector <audio_buffer *>::iterator buffer_iter;
	for(buffer_iter = m_current_outgoing_q->begin(); buffer_iter != m_current_outgoing_q->end(); buffer_iter++)
	{
		std::vector <audio_capture_client *>::iterator client_iter;
		for(client_iter = m_clients.begin(); client_iter != m_clients.end(); client_iter++)
		{
			(*client_iter)->data_callback(*buffer_iter);	
		}
	}
	unlock(m_client_mutex);
	m_current_outgoing_q->clear();
}

int q_mgr::register_client(audio_capture_client * client)
{
	DEBUG("Enter.\n");
	lock(m_client_mutex);
	if(std::find(m_clients.begin(), m_clients.end(), client) == m_clients.end()) //Add only if it is not already present.
	{
		m_clients.push_back(client);
		m_num_clients = m_clients.size();
		if(1 == m_num_clients)
		{
			start();
		}
	}
	else
	{
		INFO("Will not register client as it is already present.\n");
	}
	unlock(m_client_mutex);
	INFO("Total clients: %d.\n", m_num_clients);
	return 0;
}
int q_mgr::unregister_client(audio_capture_client * client)
{
	DEBUG("Enter.\n");
	lock(m_client_mutex);
	m_clients.erase(std::remove(m_clients.begin(), m_clients.end(), client), m_clients.end());
	m_num_clients = m_clients.size();
	if(0 == m_num_clients)
	{
		stop();
	}
	unlock(m_client_mutex);
	INFO("Total clients: %d.\n", m_num_clients);
	return 0;
}

void q_mgr::flush_system()
{
	/*
	 * Flush everything in incoming queue.
	 * Wait until outbound queue is exhausted.
	 * Return.
	 * */
	flush_queue(m_current_incoming_q);
	while(!m_current_outgoing_q->empty())
	{
		usleep(QUEUE_CHECK_LOOP_TIME_US);
	}
	INFO("Exit.\n");
}

static void log_settings(RMF_AudioCapture_Settings &settings)
{
	INFO("Format 0x%x, sampling freq: 0x%x, FIFO size: %d, threshold: %d, delay compensation: %d\n",
		settings.format, settings.samplingFreq, settings.fifoSize, settings.threshold, settings.delayCompensation_ms);
}

void q_mgr::data_monitor()
{
	INFO("data_monitor thread has launched.\n");
	unsigned int saved_byte_counter = 0;
	bool is_stalled = false;
	std::unique_lock<std::mutex> wlock(m_data_monitor_mutex);
	while(false == m_stop_data_monitor)
	{
		auto ret = m_data_monitor_cv.wait_for(wlock, std::chrono::seconds(5), [this](){return m_stop_data_monitor;});
		if(false == ret)
		{ //This indicates a timeout or spurious wake.
			if(saved_byte_counter == m_inflow_byte_counter)
			{
				if(false == is_stalled)
				{
					WARN("Data inflow has stalled at %u bytes for instance 0x%p.\n", saved_byte_counter, static_cast <void *>(this));
					is_stalled = true;
				}
				else
					continue; //already logged the stall once. 
			}
			else
			{
				saved_byte_counter = m_inflow_byte_counter;
				if(true == is_stalled)
				{
					INFO("Data inflow has resumed for instance 0x%p.\n", static_cast <void *>(this));
					is_stalled = false;
				}
			}
		}
	}
	INFO("data_monitor thread exiting.\n");
}

int q_mgr::start()
{
	if(true == m_started)
	{
		WARN("Looks like device was already started.\n");
		return 0;
	}
	
	m_inflow_byte_counter = 0;
	RMF_AudioCapture_Settings settings;
	memset (&settings, 0, sizeof(RMF_AudioCapture_Settings));
	RMF_AudioCapture_GetDefaultSettings(&settings);
	
	settings.cbBufferReady = &q_mgr::data_callback;
	settings.cbBufferReadyParm = (void *)this;
	settings.fifoSize = m_audio_properties.fifo_size; 
	settings.threshold = m_audio_properties.threshold;
	settings.delayCompensation_ms = m_audio_properties.delay_compensation_ms;
	settings.format = m_audio_properties.format;
	settings.samplingFreq = m_audio_properties.sampling_frequency;

	log_settings(settings);
	
	int ret = RMF_AudioCapture_Start(m_device_handle, &settings);
	INFO("start() result is 0x%x\n", ret);
	m_started = true;

	std::unique_lock<std::mutex> wlock(m_data_monitor_mutex);
	m_stop_data_monitor = false;
	m_data_monitor_thread = std::thread(&q_mgr::data_monitor, this);
	return ret;
}

int q_mgr::stop()
{
	if(false == m_started)
	{
		WARN("Looks like device is already stopped.\n");
		return 0;
	}
	
	{
		std::unique_lock<std::mutex> wlock(m_data_monitor_mutex);
		m_stop_data_monitor = true;
	}
	m_data_monitor_cv.notify_all();
	m_data_monitor_thread.join();

	int ret = RMF_AudioCapture_Stop(m_device_handle);
	INFO("stop() result is 0x%x\n", ret);
	m_started = false;
	return ret;
}

void audio_capture_client::release_buffer(audio_buffer *ptr)
{ 
	unref_audio_buffer(ptr);
} 

int audio_capture_client::data_callback(audio_buffer *buf)
{ 
	release_buffer(buf);
	return 0;
} 
audio_capture_client::audio_capture_client(q_mgr * manager): m_priority(0), m_manager(manager)
{ 
	pthread_mutexattr_t mutex_attribute;
	REPORT_IF_UNEQUAL(0, pthread_mutexattr_init(&mutex_attribute));
	REPORT_IF_UNEQUAL(0, pthread_mutexattr_settype(&mutex_attribute, PTHREAD_MUTEX_ERRORCHECK));
	REPORT_IF_UNEQUAL(0, pthread_mutex_init(&m_mutex, &mutex_attribute));
} 
audio_capture_client::~audio_capture_client()
{	
	REPORT_IF_UNEQUAL(0, pthread_mutex_destroy(&m_mutex));
} 

void audio_capture_client::set_manager(q_mgr *mgr)
{
	m_manager = mgr;
}

int audio_capture_client::set_audio_properties(audio_properties_t &properties)
{
	return m_manager->set_audio_properties(properties);
}

void audio_capture_client::get_audio_properties(audio_properties_t &properties)
{
	m_manager->get_audio_properties(properties);
}

void audio_capture_client::get_default_audio_properties(audio_properties_t &properties)
{
	m_manager->get_default_audio_properties(properties);
}

int audio_capture_client::start()
{
	return m_manager->register_client(this);
}

int audio_capture_client::stop()
{
	return m_manager->unregister_client(this);
}

void audio_capture_client::lock()
{
    REPORT_IF_UNEQUAL(0, pthread_mutex_lock(&m_mutex));
}

void audio_capture_client::unlock()
{
    REPORT_IF_UNEQUAL(0, pthread_mutex_unlock(&m_mutex));
}
