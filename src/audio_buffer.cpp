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
#include <audio_buffer.h>
#include <string.h>
#include <stdlib.h>
#include "basic_types.h"
#include <pthread.h>
#include "safec_lib.h"

audio_buffer::audio_buffer(const unsigned char *in_ptr, unsigned int in_size, unsigned int clip_length, unsigned int refcount) : m_size(in_size), m_clip_length(clip_length), m_refcount(refcount)
{
	DEBUG("Creating new buffer.\n");
	errno_t rc = -1;
	m_start_ptr = (unsigned char *)malloc(in_size);
	rc = memcpy_s(m_start_ptr, in_size, in_ptr, m_size);
	if(rc != EOK)
	{
		ERR_CHK(rc);
	}
}

audio_buffer::~audio_buffer()
{
	DEBUG("Deleting buffer.\n");
	free(m_start_ptr);
}


audio_buffer * create_new_audio_buffer(const unsigned char *in_ptr, unsigned int in_size, unsigned int clip_length, unsigned int refcount)
{
	return new audio_buffer(in_ptr, in_size, clip_length, refcount);
}

static pthread_mutex_t g_audio_buffer_mutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
void unref_audio_buffer(audio_buffer *ptr)
{
	/* Current implementation will go for a simple plan, which is to check the refcount of the buffer while protected by a mutex. This means
	 * every unref operation involving any buffer at all will be serialized. If this turns out to be a performance bottleneck, plan B is to launch
	 * a garbage-cleaning thread of sorts that will actually check the refcount and free the buffers. In that implementation, this funciton
	 * will simply post a message (the address of the buffer to be unreffed) to the GC thread and be done with it.*/
	REPORT_IF_UNEQUAL(0, pthread_mutex_lock(&g_audio_buffer_mutex));
	if(1 == ptr->m_refcount)
	{
		delete ptr;
	}
	else
	{
		ptr->m_refcount--;
	}
	REPORT_IF_UNEQUAL(0, pthread_mutex_unlock(&g_audio_buffer_mutex));
}

void free_audio_buffer(audio_buffer *ptr)
{
	delete ptr;
}
void audio_buffer_get_global_lock()
{
	REPORT_IF_UNEQUAL(0, pthread_mutex_lock(&g_audio_buffer_mutex));
}
void audio_buffer_release_global_lock()
{
	REPORT_IF_UNEQUAL(0, pthread_mutex_unlock(&g_audio_buffer_mutex));
}

