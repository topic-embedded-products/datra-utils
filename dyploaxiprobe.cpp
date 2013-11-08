#include <dyplo/hardware.hpp>
#include <dyplo/mmapio.hpp>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <iostream>

static void usage(const char* name)
{
	std::cerr << "usage: " << name << " [-v] [-l] [-n #] [-c #] address\n"
		" -v    verbose mode.\n"
		" -n #  Node (default is cfg, 0=cpu, >=1 hdl nodes)\n"
		" -c #  Count - number of words to read at addres\n"
		" -l    Long output\n"
		" -d    Output in decimal\n"
		" addr  Offset in memory map\n";
}

#define PAGE_SIZE 4096

int main(int argc, char** argv)
{
	int verbose = 0;
	int node = -1;
	int access = O_RDONLY; // or O_RDWR;
	int count = 1;
	bool long_format = false;
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
			int c = getopt_long(argc, argv, "c:dln:v",
							long_options, &option_index);
			if (c < 0)
				break;
			switch (c)
			{
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
			case 'v':
				++verbose;
				break;
			case '?':
				usage(argv[0]);
				return 1;
			}
		}
		for (int index = optind; index < argc; ++index)
		{
			unsigned int addr = strtol(argv[index], NULL, 0);
			off_t page_location = addr & ~(PAGE_SIZE-1);
			unsigned int page_offset = addr & (PAGE_SIZE-1);
			size_t size = addr + (count * sizeof(unsigned int)) - page_location;
			
			if (verbose) printf("Addr: %#x (%d) offset=%#x+%#x - %#x (%d)\n", addr, addr, page_location, page_offset, size, size);
			
			dyplo::File file(node < 0 ? ctrl.openControl(access) : ctrl.openConfig(node, access));
			dyplo::MemoryMap mapping(file, page_location, size, access == O_RDONLY ? PROT_READ : PROT_READ|PROT_WRITE);
			volatile unsigned int* data = (unsigned int*)(((char*)mapping.memory) + page_offset);
			
			if (long_format)
			{
				for (int i = 0; i < count; ++i)
				{
					unsigned int value = data[i];
					printf("@0x%04x: %#10x (%d) \n", addr+i, value, value);
				}
			}
			else
			{
				int j = 0;
				int lines = (count+3)/4;
				for (int line = 0; line < lines; ++line)
				{
					printf("@0x%04x: ", addr + j);
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
	catch (const std::exception& ex)
	{
		std::cerr << "ERROR:\n" << ex.what() << std::endl;
		return 1;
	}
	return 0;
}

