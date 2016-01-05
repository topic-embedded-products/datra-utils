/*
 * dyploprogrammer.cpp
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
#include "dyplo/hardware.hpp"
#include <unistd.h>
#include <iostream>
#include <getopt.h>
#include <string.h>
#include <memory>

static void usage(const char* name)
{
	std::cerr << "usage: " << name << " [-v] [-f|-p] [-o fn] files ..\n"
		" -v    verbose mode.\n"
		" -f    Default to full mode (erases PL)\n"
		" -o    Output to file (use '-' for stdout)\n"
		" -p	Default to partial mode\n"
		" files	Bitstreams to flash. May be in binary or bit format.\n"
		"       (Use '-' for stdin, only in combination with either -f, -p or -o.)\n"
		"\n"
		"Detection of partial bitstreams is automatic for the 'bit' format, but when\n"
		"programming raw 'bin' streams, provide either -p or -f to properly set the\n"
		"mode in the driver.\n";
}

class DyploUserIDMismatchException: public std::exception
{
public:
	const char* what() const throw()
	{
		return "Partial bitstream and Dyplo ID mismatch";
	}
};

class HardwareProgrammer: public dyplo::ProgramTagCallback
{
protected:
	dyplo::HardwareContext &ctrl;
	unsigned int dyplo_user_id;
	bool current_partial_mode;
	bool dyplo_user_id_valid;
public:

	HardwareProgrammer(dyplo::HardwareContext &context):
		ctrl(context),
		current_partial_mode(ctrl.getProgramMode()),
		dyplo_user_id_valid(false)
	{
	}

	void setPartialMode(bool value)
	{
		if (value != current_partial_mode)
		{
			ctrl.setProgramMode(value);
			current_partial_mode = value;
		}
	}

	unsigned int getDyploUserID()
	{
		if (!dyplo_user_id_valid)
		{
			dyplo::HardwareControl c(ctrl);
			dyplo_user_id = c.readDyploStaticID();
			dyplo_user_id_valid = true;
		}
		return dyplo_user_id;
	}

	virtual void processTag(char tag, unsigned short size, const void *data)
	{
		if (tag == 'a')
		{
			unsigned int user_id;
			bool is_partial = current_partial_mode;
			bool has_user_id =
				dyplo::HardwareContext::parseDescriptionTag((const char*)data, size, &is_partial, &user_id);
			setPartialMode(is_partial);
			if (has_user_id && is_partial)
			{
				if (getDyploUserID() != user_id)
					throw DyploUserIDMismatchException();
			}
		}
	}
};

int main(int argc, char** argv)
{
	bool verbose = false;
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
				ctrl.setProgramMode(true);
				break;
			case 'o':
				output_file = optarg;
				break;
			case 'f':
				ctrl.setProgramMode(false);
				break;
			case '?':
				usage(argv[0]);
				return 1;
			}
		}
		std::auto_ptr<HardwareProgrammer> programmer;
		for (; optind < argc; ++optind)
		{
			if (verbose)
				std::cerr << "Programming: " << argv[optind] << " " << std::flush;
			dyplo::File input(strcmp(argv[optind], "-") ?
						::open(argv[optind], O_RDONLY) :
						dup(0));
			int output_file_handle;
			if (output_file == NULL)
			{
				if (!programmer.get())
					programmer.reset(new HardwareProgrammer(ctrl));
				const char* output_name = ctrl.getDefaultProgramDestination();
				output_file_handle = ::open(output_name, O_WRONLY);
				if (output_file_handle < 0)
					throw dyplo::IOException(output_name);
			}
			else
			{
				if (verbose)
					std::cerr << " to: " << output_file << std::flush;
				if (strcmp(output_file, "-"))
				{
					output_file_handle = ::open(output_file, O_WRONLY|O_TRUNC|O_CREAT, 0644);
					if (output_file_handle < 0)
						throw dyplo::IOException(output_file);
				}
				else
					output_file_handle = dup(1);
			}
			dyplo::File output(output_file_handle);
			unsigned int r = ctrl.program(output, input, programmer.get());
			if (verbose)
			{
				if (!output_file)
					std::cerr << (ctrl.getProgramMode() ? "(partial) " : "(full) ");
				std::cerr << r << " bytes." << std::endl;
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
