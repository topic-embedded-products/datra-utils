#include "dyplo/hardware.hpp"
#include <unistd.h>
#include <iostream>
#include <getopt.h>

static void usage(const char* name)
{
	std::cerr << "usage: " << name << " [-v] [-f|-p] files ..\n"
		" -v    verbose mode.\n"
		" -f    Force full mode (erases PL)\n"
		" -o    Output to file\n"
		" -p	Force partial mode (default)\n"
		" files	Bitstreams to flash. May be in binary or bit format.\n";
}

#define DYPLO_MAGIC_PL_SIZE	4045564

int main(int argc, char** argv)
{
	bool verbose = false;
	bool forced_mode = false;
	const char* output_file = NULL;
	static struct option long_options[] = {
	   {"verbose",	no_argument, 0, 'v' },
	   {"full",		no_argument, 0, 'f' },
	   {"partial",	no_argument, 0, 'p' },
	   {0,          0,           0, 0 }
	};
	try
	{
		dyplo::HardwareContext ctrl;
		int option_index = 0;
		for (;;)
		{
			int c = getopt_long(argc, argv, "fo:pv",
							long_options, &option_index);
			if (c < 0)
				break;
			switch (c)
			{
			case 'v':
				verbose = true;
				break;
			case 'p':
				forced_mode = true;
				ctrl.setProgramMode(true);
				break;
			case 'o':
				output_file = optarg;
				break;
			case 'f':
				forced_mode = true;
				ctrl.setProgramMode(false);
				break;
			case '?':
				usage(argv[0]);
				return 1;
			}
		}
		for (; optind < argc; ++optind)
		{
			if (verbose)
				std::cerr << "Programming: " << argv[optind] << " " << std::flush;
			if (output_file == NULL)
			{
				if (!forced_mode)
				{
					bool is_partial = dyplo::File::get_size(argv[optind]) < DYPLO_MAGIC_PL_SIZE;
					ctrl.setProgramMode(is_partial);
				}
				if (verbose)
					std::cerr << (ctrl.getProgramMode() ? "(partial)" : "(full)") << "... " << std::flush;
			}
			unsigned int r;
			if (output_file == NULL)
			{
				r = ctrl.program(argv[optind]);
			}
			else
			{
				if (verbose)
					std::cerr << " to: " << output_file << std::flush;
				dyplo::File output(::open(output_file, O_WRONLY|O_TRUNC|O_CREAT, 0644));
				r = ctrl.program(output, argv[optind]);
			}
			if (verbose)
				std::cerr << r << " bytes." << std::endl;
		}
	}
	catch (const std::exception& ex)
	{
		std::cerr << "ERROR:\n" << ex.what() << std::endl;
		return 1;
	}
	return 0;
}
