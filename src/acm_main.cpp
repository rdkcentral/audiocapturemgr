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
#include <unistd.h>
#include "audio_capture_manager.h"
#include "music_id.h"
#include "acm_session_mgr.h"
#if defined(DROP_ROOT_PRIV)
#include "cap.h"
#endif

#if defined(DROP_ROOT_PRIV)
static bool drop_root()
{
    bool ret = false,retval = false;
    cap_user appcaps = {{0, 0, 0, '\0', 0, 0, 0, '\0'}};
    ret = isBlocklisted();
    if(ret)
    {
         INFO("NonRoot feature is disabled\n");
    }
    else
    {
         INFO("NonRoot feature is enabled\n");
         appcaps.caps = NULL;
         appcaps.user_name = NULL;
         if(init_capability() != NULL) {
            if(drop_root_caps(&appcaps) != -1) {
               if(update_process_caps(&appcaps) != -1) {
                   read_capability(&appcaps);
                   retval = true;
               }
            }
         }
    }
    return retval;
}
#endif

void launcher()
{
	acm_session_mgr *mgr = acm_session_mgr::get_instance();
	mgr->activate();
	/* Hold here until application is terminated.*/
	pause();
	mgr->deactivate();
}


int main(int argc, char *argv[])
{
	setlinebuf(stdout); //otherwise logs may take forever to get flushed to journald
#if defined(DROP_ROOT_PRIV)
        if(!drop_root())
        {
           ERROR("drop_root function failed!\n");
        }
#endif
	launcher();
	return 0;
}
