/*
 * dyploproxy.cpp
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
#include <dyplo/hardware.hpp>
#include <dyplo/filequeue.hpp>
#include <stdlib.h>
#include <iostream>
#include <getopt.h>
#include <vector>
#include <sstream>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

using dyplo::IOException;

static void usage(const char* name)
{
	std::cerr << "usage: " << name << " [-s blocksize] [-v] function [function ...]\n"
		"Runs data from stdin/stdout via Dyplo hardware. Automatically allocates\n"
		"and programs partitions. Multiple functions will be linked in hardware.\n"
		" -v    verbose mode.\n"
		" -s .. Blocksize in bytes, default is 4k.\n"
		"Example: mpg123 -s music.mp3 | " << name << " lowPass reverb | aplay -f cd\n";
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

class NotFoundError: public std::exception
{
	std::string msg;
public:
	NotFoundError(const char *txt, const char *what):
		msg(txt)
	{
		if (what)
		{
			msg += ": ";
			msg += what;
		}
	}
	const char* what() const throw()
	{
		return msg.c_str();
	}
	~NotFoundError() throw()
	{
	}
};

static int openAvailableFifo(dyplo::HardwareContext &context, unsigned char* id, int access)
{
	for (int index = 0; index < 32; ++index)
	{
		int result = context.openFifo(index, access);
		if (result != -1)
		{
			*id = (unsigned char)index;
			return result;
		}
		if (errno != EBUSY)
			throw IOException();
	}
	throw IOException(ENODEV);
}


int main(int argc, char** argv)
{
	static struct option long_options[] = {
	   {"verbose",	no_argument, 0, 'v' },
	   {0,          0,           0, 0 }
	};
	unsigned int blocksize = 4096;
	bool verbose = false;
	try
	{
		int option_index = 0;
		for (;;)
		{
			int c = getopt_long(argc, argv, "bns:v",
							long_options, &option_index);
			if (c < 0)
				break;
			switch (c)
			{
			case 's':
				blocksize = atoi(optarg);
				if (blocksize <= 0)
					throw ParseError("Invalid blocksize", optarg);
				break;
			case 'v':
				verbose = true;
				break;
			case '?':
				usage(argv[0]);
				return 1;
			}
		}
		if (!argc)
		{
			usage(argv[0]);
			return 1;
		}
		dyplo::HardwareContext context;
		dyplo::HardwareControl control(context);
		std::vector<dyplo::HardwareControl::Route> routes;
		dyplo::HardwareControl::Route route;
		/* entry route */
		route.srcNode = 0;
		dyplo::File to_hardware(openAvailableFifo(context, &route.srcFifo, O_WRONLY));
		dyplo::set_non_blocking(to_hardware);
		/* Set up hardware resources and routes */
		for (; optind < argc; ++optind)
		{
			// ... open node, program ...
			unsigned int candidates = context.getAvailablePartitions(argv[optind]);
			if (candidates == 0)
				throw NotFoundError("Function does not exist", argv[optind]);
			unsigned int mask = 1;
			for (int id = 1; id < 32; ++id)
			{
				mask <<= 1;
				if ((mask & candidates) != 0)
				{
					int handle = context.openConfig(id, O_RDWR);
					if (handle == -1)
					{
						if (errno != EBUSY) /* Non existent? Bail out, last node */
							throw NotFoundError("Function not available", argv[optind]);
					}
					else
					{
						control.disableNode(id);
						std::string filename =
								context.findPartition(argv[optind], id);
						context.setProgramMode(true); /* partial */
						context.program(filename.c_str());
						control.enableNode(id);
						route.dstNode = id;
						route.dstFifo = 0;
						if (verbose)
							std::cerr << argv[optind]
								<< " handle=" << handle << " id=" << id
								<< " "
								<< (int)route.srcNode << "." << (int)route.srcFifo
								<< "->"
								<< (int)route.dstNode << "." << (int)route.dstFifo
								<< std::endl;
						routes.push_back(route);
						route.srcNode = route.dstNode;
						route.srcFifo = route.dstFifo;
						break;
					}
				}
			}
		}
		/* Setup routes from hw to sw */
		route.dstNode = 0;
		dyplo::File from_hardware(openAvailableFifo(context, &route.dstFifo, O_RDONLY));
		dyplo::set_non_blocking(from_hardware);
		routes.push_back(route);
		/* Send route table to driver */
		control.routeAdd(&routes[0], routes.size());
		/* Run the transfer loop */
		std::vector<char> buffer_in(blocksize);
		std::vector<char> buffer_out(blocksize);
		char* in_pos;
		ssize_t in_avail = 0;
		char* out_pos;
		ssize_t out_avail = 0;
		dyplo::set_non_blocking(0);
		dyplo::set_non_blocking(1);
		struct pollfd fds[4];
		fds[0].fd = 0;
		fds[1].fd = to_hardware;
		fds[2].fd = from_hardware;
		fds[3].fd = 1;
		bool input_eof = false;
		for (;;)
		{
			if (in_avail)
			{
				fds[0].events = 0;
				fds[1].events = POLLOUT | POLLERR | POLLHUP | POLLNVAL;
			}
			else
			{
				fds[0].events = input_eof ? 0 : POLLIN | POLLRDHUP | POLLERR | POLLHUP | POLLNVAL;
				fds[1].events = 0;
			}
			if (out_avail)
			{
				fds[2].events = 0;
				fds[3].events = POLLOUT | POLLERR | POLLHUP | POLLNVAL;
			}
			else
			{
				fds[2].events = POLLIN | POLLRDHUP | POLLERR | POLLHUP | POLLNVAL;
				fds[3].events = 0;
			}
			int result;
			if (input_eof)
				result = ::poll(fds + 1, 3, 500);
			else
				result = ::poll(fds, 4, -1);
			if (result == -1)
				throw IOException("poll");
			if (result == 0)
			{
				if (input_eof)
				{
					if (verbose)
						std::cerr << "Timeout after EOF in stdin" << std::endl;
					break;
				}
			}
			if (in_avail)
			{
				if (fds[1].revents)
				{
					ssize_t bytes = ::write(to_hardware, in_pos, in_avail);
					if (bytes <= 0)
					{
						if (bytes == 0)
							throw dyplo::EndOfOutputException();
						else if (errno != EAGAIN)
							throw IOException("to hardware");
					}
					else
					{
						in_avail -= bytes;
						in_pos += bytes;
					}
				}
				fds[1].revents = 0;
			}
			else
			{
				if (fds[0].revents)
				{
					in_pos = &buffer_in[0];
					ssize_t bytes = ::read(0, in_pos, blocksize);
					if (bytes <= 0)
					{
						if (bytes == 0)
						{
							if (verbose)
								std::cerr << "EOF on stdin" << std::endl;
							input_eof = true;
						}
						else if (errno != EAGAIN)
							throw IOException("to hardware");
					}
					else
						in_avail = bytes;
					fds[0].revents = 0;
				}
			}
			if (out_avail)
			{
				if (fds[3].revents)
				{
					ssize_t bytes = ::write(1, out_pos, out_avail);
					if (bytes <= 0)
					{
						if (bytes == 0)
							throw dyplo::EndOfOutputException();
						else if (errno != EAGAIN)
							throw IOException("to stdout");
					}
					else
					{
						out_avail -= bytes;
						out_pos += bytes;
					}
					fds[3].revents = 0;
				}
			}
			else
			{
				if (fds[2].revents)
				{
					out_pos = &buffer_out[0];
					ssize_t bytes = ::read(from_hardware, out_pos, blocksize);
					if (bytes < 0)
					{
						if (errno != EAGAIN)
							throw IOException("from hardware");
					}
					else
						out_avail = bytes;
					fds[2].revents = 0;
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
