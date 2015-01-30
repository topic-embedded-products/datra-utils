/*
 * dyploroute.cpp
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
#include <stdlib.h>
#include <iostream>
#include <getopt.h>
#include <vector>
#include <sstream>

static void usage(const char* name)
{
	std::cerr << "usage: " << name << " [-v] [-c] sn,sf,dn,df ...\n"
		" -v    verbose mode.\n"
		" -c    clear all routes first\n"
		" -n N  clear routes connected to node number N\n"
		" -l    list all routes\n"
		" sn,sf,dn,df	Source node, fifo, destination node and fifo.\n";
}

class ParseError: public std::exception
{
	std::string msg;
public:
	ParseError(const char *txt, const char *what):
		msg("Failed to parse: '")
	{
		msg += txt;
		msg += "' at ";
		msg += what;
	}
	const char* what() const throw()
	{
		return msg.c_str();
	}
	~ParseError() throw()
	{
	}
};

static bool is_digit(char c)
{
	return (c >= '0') && (c <= '9');
}

static int read_int(char const **txt)
{
	const char *d = *txt;
	int result = 0;
	while (is_digit(*d))
	{
		result = result * 10 + (*d - '0');
		++d;
	}
	while (*d && !is_digit(*d))
		++d; /* Skip separator */
	*txt = d;
	return result;
}

static dyplo::HardwareControl::Route parse_route(const char *txt)
{
	dyplo::HardwareControl::Route result;
	int v;
	const char* d = txt;
	v = read_int(&d);
	if (! *d)
		throw ParseError(txt, "srcNode");
	result.srcNode = v;
	v = read_int(&d);
	if (! *d)
		throw ParseError(txt, "srcFifo");
	result.srcFifo = v;
	v = read_int(&d);
	if (! *d)
		throw ParseError(txt, "dstNode");
	result.dstNode = v;
	v = read_int(&d);
	result.dstFifo = v;
	return result;
}


int main(int argc, char** argv)
{
	static struct option long_options[] = {
	   {"clear",	no_argument, 0, 'c' },
	   {"verbose",	no_argument, 0, 'v' },
	   {"list",		no_argument, 0, 'l' },
	   {0,          0,           0, 0 }
	};
	dyplo::HardwareContext context;
	bool verbose = false;
	bool list_routes = false;
	try
	{
		int option_index = 0;
		for (;;)
		{
			int c = getopt_long(argc, argv, "cln:v",
							long_options, &option_index);
			if (c < 0)
				break;
			switch (c)
			{
			case 'c':
				dyplo::HardwareControl(context).routeDeleteAll();
				break;
			case 'l':
				list_routes = true;
				break;
			case 'n':
				{
					int node = atoi(optarg);
					dyplo::HardwareControl(context).routeDelete(node);
				}
				break;
			case 'v':
				verbose = true;
				break;
			case '?':
				usage(argv[0]);
				return 1;
			}
		}
		std::vector<dyplo::HardwareControl::Route> routes;
		for (; optind < argc; ++optind)
		{
			if (verbose)
				std::cerr << argv[optind] << ": " << std::flush;
			dyplo::HardwareControl::Route route = parse_route(argv[optind]);
			if (verbose)
				std::cerr << " "
					<< (int)route.srcNode << "." << (int)route.srcFifo
					<< "->"
					<< (int)route.dstNode << "." << (int)route.dstFifo
					<< std::endl;
			routes.push_back(route);
		}
		if (!routes.empty())
		{
			dyplo::HardwareControl(context).routeAdd(&routes[0], routes.size());
		}
		if (list_routes)
		{
			routes.resize(256);
			int n_routes = dyplo::HardwareControl(context).routeGetAll(&routes[0], routes.size());
			if (n_routes < 0)
				throw dyplo::IOException();
			routes.resize(n_routes);
			for (std::vector<dyplo::HardwareControl::Route>::const_iterator route = routes.begin();
					route != routes.end(); ++route)
			{
				std::cout
					<< (int)route->srcNode << '.' << (int)route->srcFifo
					<< '-'
					<< (int)route->dstNode << '.' << (int)route->dstFifo
					<< '\n';
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
