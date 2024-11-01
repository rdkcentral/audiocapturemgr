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
bin_PROGRAMS = audiocapturemgrtestapp acm_ipout_testapp acm_musicid_testapp
audiocapturemgrtestapp_SOURCES = rmfAudioCaptureTestApp.cpp
audiocapturemgrtestapp_CPPFLAGS =  -std=c++0x -I$(top_srcdir)/include -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/media-utils/audioCapture/ -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/
audiocapturemgrtestapp_LDADD =  ${top_builddir}/src/libaudiocapturemgr.la

acm_ipout_testapp_SOURCES = ipOutTestApp.cpp
acm_ipout_testapp_CPPFLAGS =  -std=c++0x -I$(top_srcdir)/include -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/media-utils/audioCapture/ -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmbus/ -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/
acm_ipout_testapp_LDADD =  -L${RDK_FSROOT_PATH}/usr/local/lib -L${RDK_FSROOT_PATH}/usr/lib -lpthread -lIARMBus

acm_musicid_testapp_SOURCES = musicIdTestApp.cpp 
acm_musicid_testapp_CPPFLAGS =  -std=c++0x -I$(top_srcdir)/include -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/media-utils/audioCapture/ -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/rdk/iarmbus/ -I$(PKG_CONFIG_SYSROOT_DIR)${includedir}/
acm_musicid_testapp_LDADD =  -L${RDK_FSROOT_PATH}/usr/local/lib -L${RDK_FSROOT_PATH}/usr/lib -lIARMBus
