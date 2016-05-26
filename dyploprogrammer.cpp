/*
 * dyploprogrammer.cpp
 *
 * Dyplo commandline utilities.
 *
 * (C) Copyright 2013-2016 Topic Embedded Products B.V. (http://www.topic.nl).
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
    std::cerr << "usage: " << name << " [-v] [-b bitstream_path] function N [N] ..\n"
        " -v        verbose mode.\n"
        " -b        Bitstream base path (default /usr/share/bitstreams)\n"
        " function  Function to be programmed\n"
        " N         Node index(es) to program the function to\n"
        "\n"
        "Programs functions into Dyplo's reconfigurable partitions.\n"
        "For example, to put an adder into nodes 1 and 2, and a fir into 3:\n"
        "  " << name << " adder 1 2 fir 3\n"
        "This requires bitstreams for these functions to be present.\n";
}

int main(int argc, char** argv)
{
    bool verbose = false;
    static struct option long_options[] = {
       {"verbose", no_argument, 0, 'v' },
       {0,         0,           0, 0 }
    };
    
    try
    {
        dyplo::HardwareContext ctx;
        dyplo::HardwareControl control(ctx);
        
        int option_index = 0;
        for (;;)
        {
            int c = getopt_long(argc, argv, "b:v",
                                long_options, &option_index);
            if (c < 0) 
            {
                break;
            }
            
            switch (c)
            {
            case 'b':
                ctx.setBitstreamBasepath(optarg);
                break;
            case 'v':
                verbose = true;
                break;
            case '?':
                usage(argv[0]);
                return 1;
            }
        }
        
        const char* function_name = NULL;

        for (; optind < argc; ++optind)
        {
            const char* arg = argv[optind];
            char *endptr;
            unsigned int node_index;

            node_index = strtoul(arg, &endptr, 0);
            if ((*endptr) == NULL) /* Valid number */
            {
                if (function_name == NULL)
                {
                    std::cerr << "Must set a function name before the number " << node_index << std::endl;
                    return 1;
                }
                
                std::string filename = ctx.findPartition(function_name, node_index);
                if (filename.empty())
                {
                    std::cerr << "Function " << function_name << " not available for node " << node_index << std::endl;
                    return 1;
                }
                
                if (verbose)
                {
                    std::cerr << "Programming '" << function_name << "' into " << node_index << " using " << filename << std::flush;
                }
                
                dyplo::File input_file(filename.c_str(), O_RDONLY);
                dyplo::HardwareConfig cfg(ctx, node_index);

                cfg.disableNode();
                unsigned int r = control.program(input_file);
                cfg.enableNode();

                if (verbose)
                {
                    std::cerr << " " << r << " bytes." << std::endl;
                }
            }
            else
            {
                function_name = arg;
            }
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "\nERROR: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
