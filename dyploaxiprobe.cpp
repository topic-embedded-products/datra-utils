/*
 * dyploaxiprobe.cpp
 *
 * Dyplo commandline utilities.
 *
 * (C) Copyright 2013,2014 Topic Embedded Products B.V. <Mike Looijmans> (http://www.topic.nl).
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
#include <dyplo/hardware.hpp>
#include <dyplo/mmapio.hpp>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include "benchmark.hpp"

static void usage(const char* name)
{
	std::cerr << "usage:\n"
		<< name << " [options] [-r] addr [addr..]\n"
		<< name << " [options] -w addr value [value..]\n"
		<< name << " [options] -b addr\n"
		" -r    Read and display contents (default)\n"
		" -w    Write to memory (dangerous)\n"
		" -b    Benchmark mode (read addr continuously)\n"
		"options:\n"
		" -v    verbose mode.\n"
		" -n #  Node (default is cfg, 0=cpu, >=1 hdl nodes)\n"
		" -c #  Count - number of words to read at addres\n"
		" -l    Long output\n"
		" -d    Output in decimal\n"
		" addr  Offset in memory map\n"
		" value Data to write (32-bit integer)\n";
}

#define PAGE_SIZE 4096

int main(int argc, char** argv)
{
	int verbose = 0;
	int node = -1;
	int access = O_RDONLY; // or O_RDWR;
	int count = 1;
	bool long_format = false;
	bool benchmark = false;
	const char* short_format = " %8x";
	static struct option long_options[] = {
	   {"node",	required_argument, 0, 'n' },
	   {"read",		no_argument, 0, 'r' },
	   {"verbose",	no_argument, 0, 'v' },
	   {"write",	no_argument, 0, 'w' },
	   {0,          0,           0, 0 }
	};
	try
	{
		dyplo::HardwareContext ctrl;
		int option_index = 0;
		for (;;)
		{
			int c = getopt_long(argc, argv, "bc:dln:rvw",
							long_options, &option_index);
			if (c < 0)
				break;
			switch (c)
			{
			case 'b':
				benchmark = true;
				break;
			case 'c':
				count = strtol(optarg, NULL, 0);
				break;
			case 'd':
				short_format = " %8d";
				break;
			case 'l':
				long_format = true;
				break;
			case 'n':
				node = strtol(optarg, NULL, 0);
				break;
			case 'r':
				access = O_RDONLY;
				break;
			case 'v':
				++verbose;
				break;
			case 'w':
				access = O_RDWR;
				break;
			case '?':
				usage(argv[0]);
				return 1;
			}
		}
		
		dyplo::File file(node < 0 ? ctrl.openControl(access) : ctrl.openConfig(node, access));

		if (access == O_RDONLY)
		{
			for (int index = optind; index < argc; ++index)
			{
				unsigned int addr = strtol(argv[index], NULL, 0);
				off_t page_location = addr & ~(PAGE_SIZE-1);
				unsigned int page_offset = addr & (PAGE_SIZE-1);
				size_t size = addr + (count * sizeof(unsigned int)) - page_location;
				
				if (verbose) printf("Addr: %#x (%d) offset=%#x+%#x - %#zx (%zu)\n",
					addr, addr, (unsigned int)page_location, page_offset, size, size);
				
				dyplo::MemoryMap mapping(file, page_location, size, PROT_READ);
				volatile unsigned int* data = (unsigned int*)(((char*)mapping.memory) + page_offset);
				
				if (benchmark)
				{
					unsigned int loops = 0;
					Stopwatch timer;
					if (count > 1)
					{
						size_t blocksize = count * sizeof(unsigned int);
						unsigned int dest[count];
						timer.start();
						do
						{
							for (unsigned int repeat = 64*1024u; repeat != 0; --repeat)
							{
								memcpy(dest, (void*)data, blocksize);
							}
							++loops;
							timer.stop();
						} while (timer.elapsed_us() < 1000000);
					}
					else
					{
						count = 1;
						timer.start();
						do
						{
							unsigned int dest;
							for (unsigned int repeat = 64*1024u; repeat != 0; --repeat)
							{
								dest += *data; /* force memory access */
							}
							++loops;
							timer.stop();
						} while (timer.elapsed_us() < 1000000);
					}
					unsigned int elapsed_us = timer.elapsed_us();
					unsigned int bytes = loops * count * (64u * 1024u * sizeof(unsigned int));
					printf("loops=%u us=%u bytes=%u hence %u MB/s\n",
						loops, elapsed_us, bytes, bytes / elapsed_us);
				}
				else
				{
					if (long_format)
					{
						for (int i = 0; i < count; ++i)
						{
							unsigned int value = data[i];
							printf("@0x%04x: %#10x (%d)\n",	(unsigned int)(addr+(i*sizeof(unsigned int))), value, (int)value);
						}
					}
					else
					{
						int j = 0;
						int lines = (count+3)/4;
						for (int line = 0; line < lines; ++line)
						{
							printf("@0x%04x: ", (unsigned int)(addr + (j*sizeof(unsigned int))));
							for (int i = j; i < count && i < j+4; ++i)
							{
								unsigned int value = data[i];
								printf(short_format, value);
							}
							printf("\n");
							j += 4;
						}
					}
				}
			}
		}
		else
		{
			if (argc - optind < 2)
				throw std::runtime_error("Too few arguments for write mode, need address and value(s)");
			unsigned int addr = strtol(argv[optind], NULL, 0);
			++optind;
			unsigned int values = argc - optind;
			off_t page_location = addr & ~(PAGE_SIZE-1);
			unsigned int page_offset = addr & (PAGE_SIZE-1);
			size_t size = addr + (values * sizeof(unsigned int)) - page_location;
				if (verbose) printf("Addr: %#x (%d) offset=%#x+%#x - %#zx (%zu)\n", addr, addr, (unsigned int)page_location, page_offset, size, size);
			dyplo::MemoryMap mapping(file, page_location, size, PROT_READ|PROT_WRITE);
			volatile unsigned int* data = (unsigned int*)(((char*)mapping.memory) + page_offset);
			unsigned int value[values];
			const size_t blocksize = values * sizeof(unsigned int);
			for (int index = 0; index < values; ++index)
				value[index] = strtoul(argv[optind+index], NULL, 0);
			if (verbose)
			{
				printf("transfer size: %d words, %zd bytes\n", values, blocksize);
				for (int index = 0; index < values; ++index)
					printf("%x (%d)\n", value[index], (int)value[index]);
			}
			if (benchmark)
			{
				unsigned int loops = 0;
				Stopwatch timer;
				timer.start();
				do
				{
					for (unsigned int repeat = 64*1024u; repeat != 0; --repeat)
					{
						memcpy((void*)data, value, blocksize);
					}
					++loops;
					timer.stop();
				} while (timer.elapsed_us() < 1000000);
				unsigned int elapsed_us = timer.elapsed_us();
				unsigned int bytes = loops * blocksize * (64u * 1024u);
				printf("loops=%u us=%u bytes=%u hence %u MB/s\n",
					loops, elapsed_us, bytes, bytes / elapsed_us);
			}
			else
			{
				memcpy((void*)data, value, blocksize);
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

