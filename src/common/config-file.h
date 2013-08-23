/*
 * Copyright (C) 2000-2008 - Shaun Clowes <delius@progsoc.org> 
 * 				 2008-2011 - Robert Hogan <robert@roberthogan.net>
 * 				 	  2013 - David Goulet <dgoulet@ev0ke.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 2 only, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef CONFIG_FILE_H
#define CONFIG_FILE_H

#include <netinet/in.h>

#include "connection.h"

/*
 * Represent the values in a configuration file (torsocks.conf). Basically,
 * this is the data structure of a parsed config file.
 */
struct config_file {
	/* The tor address is inet or inet 6. */
	enum connection_domain tor_domain;
	/* The IP of the Tor SOCKS. */
	char *tor_address;
	/* The port of the Tor SOCKS. */
	in_port_t tor_port;

	/*
	 * Base for onion address pool and the mask. In the config file, this is
	 * represented by BASE/MASK like so: 127.0.69.0/24
	 */
	in_addr_t onion_base;
	uint8_t onion_mask;

    /* The level to log at. */
    int log_level;
};

/*
 * Structure representing a complete parsed file.
 */
struct configuration {
	/*
	 * Parsed config file (torsocks.conf).
	 */
	struct config_file conf_file;

	/*
	 * Socks5 address so basically where to connect to Tor.
	 */
	struct connection_addr socks5_addr;
};

int config_file_read(const char *filename, struct configuration *config);
void config_file_destroy(struct config_file *conf);

#endif /* CONFIG_FILE_H */
