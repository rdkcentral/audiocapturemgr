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
#include <string.h>
#include "audio_capture_manager.h"
#include "music_id.h"
#include <sstream>

void print_menu(void)
{
	std::cout<<"\n--- audio capture test application menu ---\n";
	std::cout<<"1. q_mgr start (non-essential step)\n";
	std::cout<<"2. q_mgr stop (non-essential step)\n";
	std::cout<<"3. dump precaptured sample\n";
	std::cout<<"4. set precapture length\n";
	std::cout<<"5. capture next N seconds\n";
	std::cout<<"6. Start client\n";
	std::cout<<"7. Stop client\n";
	std::cout<<"9. Quit.\n";
}

static std::string callback_payload = "Uninitialized";
static unsigned int ticker = 0;
static std::string get_suffix()
{
	std::ostringstream stream;
	stream << ticker++;
    stream << ".wav";
	std::string outstring = stream.str();
	return outstring;
}

void launcher()
{
	bool keep_running = true;
    q_mgr manager;
	music_id_client client(&manager, music_id_client::FILE_OUTPUT);
	client.enable_wav_header(true);
	
	while(keep_running)
	{
		int choice = 0;
		std::string filename;
		int duration = 0;
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
				manager.start();
				break;
			case 2:
				manager.stop();
				break;
			case 3:
				filename = "/tmp/precap-"+ get_suffix();
				client.grab_precaptured_sample(filename);
				std::cout<<"Output file "<<filename<<std::endl;
				break;
			case 4:
				{
					std::cout<<"Enter precapture duration in seconds:\n";
					if(!(std::cin>>duration))
					{
						std::cout<<"Whoops! Bad input.\n";
						std::cin.clear();
						std::cin.ignore(10000, '\n');
					}
					else
					{
						client.set_precapture_duration(duration);
					}
					break;
				}
			case 5:
				{
					std::cout<<"Enter capture duration in seconds:\n";
					if(!(std::cin>>duration))
					{
						std::cout<<"Whoops! Bad input.\n";
						std::cin.clear();
						std::cin.ignore(10000, '\n');
					}
					else
					{
						filename = "/tmp/freshcap-"+ get_suffix();
						client.grab_fresh_sample(duration, filename);
						std::cout<<"Output file "<<filename<<std::endl;
					}
					break;
				}
			case 6:
				{
					client.start();
					break;
				}
			case 7:
				{
					client.stop();
					break;
				}
			case 9:
				{
					keep_running = false;
					std::cout<<"Quitting.\n";
					break;
				}
			default:
				std::cout<<"Unknown input!\n";
		}
	}
}


int main(int argc, char *argv[])
{
	launcher();
    return 0;
}
