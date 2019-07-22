/*
 * dyplolicense.cpp
 *
 * Dyplo commandline utilities.
 *
 * (C) Copyright 2014 Topic Embedded Products B.V. <Mike Looijmans> (http://www.topic.nl).
 * All rights reserved.
 *
 * This file is part of dyplo-utils.
 * dyplo-utils is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * dyplo-utils is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with <product name>.  If not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA or see <http://www.gnu.org/licenses/>.
 *
 * You can contact Topic by electronic mail via info@topic.nl or via
 * paper mail at the following address: Postbus 440, 5680 AK Best, The Netherlands.
 */
#include "dyplo/hardware.hpp"
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <getopt.h>

static void usage(const char* name)
{
	std::cerr << "usage: " << name << " [-a|-b] [-o offset] [-v] [-w key] file\n"
		     "       " << name << " {-r|-i}\n"
		" -a    ASCII mode\n"
		" -b    binary mode (default)\n"
		" -o    offset in binary file\n"
		" -r    read key from driver and write to stdout in hex\n"
		" -i    read device ID from driver and write to stdout in hex\n"
		" -v    verbose mode\n"
		" -w    write key to file instead of reading it\n"
		" file  file (or device) to read key from or to write it to\n"
		"Activates a Dyplo license by writing it to the hardware. Must be called\n"
		"early at boot. File can be a regular file or e.g. an EEPROM device.\n"
		"When -r is specified, it reads back the key from hardware instead.\n";
}

int main(int argc, char** argv)
{
	bool ascii_mode = false;
	bool verbose = false;
	bool write_mode = false;
	bool read_license = false;
	bool read_deviceid = false;
	unsigned long long key;
	off_t offset = 0;

	try
	{
		static struct option long_options[] = {
		   {"ascii",	no_argument, 0, 'a' },
		   {"binary",	no_argument, 0, 'b' },
		   {"offset", required_argument, 0, 'o' },
		   {"read",	no_argument, 0, 'r' },
		   {"id",	no_argument, 0, 'i' },
		   {"verbose",	no_argument, 0, 'v' },
		   {"write", required_argument, 0, 'w' },
		   {0,          0,           0, 0 }
		};
		int option_index = 0;
		for (;;)
		{
			int c = getopt_long(argc, argv, "abio:rvw:",
							long_options, &option_index);
			if (c < 0)
				break;
			switch (c)
			{
			case 'a':
				ascii_mode = true;
				break;
			case 'b':
				ascii_mode = false;
				break;
			case 'i':
				read_deviceid = true;
				break;
			case 'o':
				offset = strtoll(optarg, NULL, 0);
				break;
			case 'r':
				read_license = true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'w':
				write_mode = true;
				key = strtoll(optarg, NULL, 0);
				break;
			case '?':
				usage(argv[0]);
				return 1;
			}
		}
		if (write_mode)
		{
			if (optind < argc)
			{
				if (verbose)
					std::cerr << std::hex << "Writing key " << key << " to " << argv[optind] << " at " << std::dec << offset << std::endl;
				dyplo::File output(::open(argv[optind], O_WRONLY|O_CREAT, 0644));
				if (offset)
					output.seek(offset);
				output.write(&key, sizeof(key));
			}
			else
			{
				if (verbose)
					std::cerr << std::hex << "Programming key " << key << std::endl;
				dyplo::HardwareContext context;
				dyplo::HardwareControl control(context);
				control.writeDyploLicense(key);
			}
		}
		else
		{
			dyplo::HardwareContext context;
			dyplo::HardwareControl control(context);

			if (read_license)
			{
				key = control.readDyploLicense();
				if (verbose)
					std::cout << "License: ";
				std::cout << std::hex << "0x" << key << std::endl;
			}

			if (read_deviceid)
			{
				key = control.readDyploDeviceID();
				if (verbose)
					std::cout << "Device ID: ";
				std::cout << std::hex << "0x" << key << std::endl;
			}

			if (optind < argc)
			{
				if (ascii_mode)
				{
					control.writeDyploLicenseFile(argv[optind]);
				}
				else
				{
					{
						dyplo::File input(argv[optind], O_RDONLY);
						if (offset)
							input.seek(offset);
						input.read(&key, sizeof(key));
					}
					if (verbose)
							std::cerr << std::hex << "Programming key " << key << std::endl;
					control.writeDyploLicense(key);
				}
			}
		}
	}
	catch (const std::exception& ex)
	{
		std::cerr << "ERROR:\n" << ex.what() << std::endl;
		return 1;
	}
	return 0;
}
