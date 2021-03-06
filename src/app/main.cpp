/***************************************************************************
 *  main.cpp - The tool's main binary
 *
 *  Created:   Mon 19 Apr 16:20:10 CEST 2021
 *  Copyright  2021  Till Hofmann <hofmann@kbsg.rwth-aachen.de>
 ****************************************************************************/
/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.md file.
 */

#include "app/app.h"

#include <spdlog/spdlog.h>

#include <stdexcept>

int
main(int argc, const char *const argv[])
{
	try {
		app::Launcher launcher{argc, argv};
		launcher.run();
		return 0;
	} catch (const std::exception &e) {
		SPDLOG_CRITICAL("Exception: {}", e.what());
		return 1;
	}
}
