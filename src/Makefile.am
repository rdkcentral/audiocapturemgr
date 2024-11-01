##########################################################################
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2016 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
lib_LTLIBRARIES = libaudiocapturemgr.la
libaudiocapturemgr_la_SOURCES = audio_buffer.cpp  audio_capture_manager.cpp  music_id.cpp ip_out.cpp audio_converter.cpp socket_adaptor.cpp 
libaudiocapturemgr_la_CPPFLAGS = -std=c++0x -I$(top_srcdir)/include -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/media-utils/audioCapture/ -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/
libaudiocapturemgr_la_LIBADD = -lrmfAudioCapture -lpthread

bin_PROGRAMS = audiocapturemgr
audiocapturemgr_SOURCES =  acm_session_mgr.cpp acm_main.cpp 
audiocapturemgr_CPPFLAGS = -std=c++0x -I$(top_srcdir)/include -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/media-utils/audioCapture/ -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmbus/ -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/
audiocapturemgr_LDADD =  libaudiocapturemgr.la -L${RDK_FSROOT_PATH}/usr/lib -L${RDK_FSROOT_PATH}/usr/local/lib -lIARMBus
