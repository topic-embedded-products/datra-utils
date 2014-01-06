/*
 * benchmark.hpp
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
#include <time.h>

class Stopwatch
{
public:
	struct timespec m_start;
	struct timespec m_stop;
	
	Stopwatch()
	{
		clock_gettime(CLOCK_MONOTONIC, &m_start);
	}
	
	void start()
	{
		clock_gettime(CLOCK_MONOTONIC, &m_start);
	}
	
	void stop()
	{
		clock_gettime(CLOCK_MONOTONIC, &m_stop);
	}

	unsigned int elapsed_us()
	{
		return 
			((m_stop.tv_sec - m_start.tv_sec) * 1000000) +
				(m_stop.tv_nsec - m_start.tv_nsec) / 1000;
	}
};
