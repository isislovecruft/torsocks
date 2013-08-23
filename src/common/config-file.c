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

#include <arpa/inet.h>
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "config-file.h"
#include "defaults.h"
#include "log.h"
#include "utils.h"

/*
 * These are the torsocks.conf string values.
 */
static const char *conf_toraddr_str = "TorAddress";
static const char *conf_torport_str = "TorPort";
static const char *conf_onion_str = "OnionAddrRange";
static const char *conf_loglevel_str = "LogLevel";
static const char *conf_logtime_str = "LogTimestamps";

/*
 * Set the onion pool address range in the configuration object using the value
 * found in the conf file.
 *
 * Return 0 on success or else a negative value.
 */
static int set_onion_info(const char *addr, struct configuration *config)
{
	int ret;
	unsigned long bit_mask;
	char *ip = NULL, *mask = NULL;
	in_addr_t net;

	assert(addr);
	assert(config);

	ip = strchr(addr, '/');
	if (!ip) {
		ERR("[config] Invalid %s value for %s", addr, conf_onion_str);
		ret = -EINVAL;
		goto error;
	}

	mask = strdup(addr + (ip - addr) + 1);
	ip = strndup(addr, ip - addr);
	if (!ip || !mask) {
		PERROR("[config] strdup onion addr");
		ret = -ENOMEM;
		goto error;
	}

	net = inet_addr(ip);
	if (net == INADDR_NONE) {
		ERR("[config] Invalid IP subnet %s for %s", ip, conf_onion_str);
		ret = -EINVAL;
		goto error;
	}

	/* Expressed in base 10. */
	bit_mask = strtoul(mask, NULL, 10);
	if (bit_mask == ULONG_MAX) {
		ERR("[config] Invalid mask %s for %s", mask, conf_onion_str);
		ret = -EINVAL;
		goto error;
	}

	memcpy(&config->conf_file.onion_base, &net,
			sizeof(config->conf_file.onion_base));
	config->conf_file.onion_mask = (uint8_t) bit_mask;

	DBG("[config] Onion address range set to %s", addr);
	ret = 0;

error:
	free(ip);
	free(mask);
	return ret;
}

/*
 * Set the given string port in a configuration object.
 *
 * Return 0 on success or else a negative EINVAL if the port is equal to 0 or
 * over 65535.
 */
static int set_tor_port(const char *port, struct configuration *config)
{
	int ret = 0;
	char *endptr;
	unsigned long _port;

	assert(port);
	assert(config);

	/* Let's avoid a integer overflow here ;). */
	_port = strtoul(port, &endptr, 10);
	if (_port == 0 || _port > 65535) {
		ret = -EINVAL;
		ERR("Config file invalid port: %s", port);
		goto error;
	}

	config->conf_file.tor_port = (in_port_t) _port;

	DBG("Config file setting tor port to %lu", _port);

error:
	return ret;
}

/*
 * Set the given string address in a configuration object.
 *
 * Return 0 on success or else a negative value. On error, the address was not
 * recognized.
 */
static int set_tor_address(const char *addr, struct configuration *config)
{
	int ret;

	assert(addr);
	assert(config);

	ret = utils_is_address_ipv4(addr);
	if (ret == 1 ) {
		config->conf_file.tor_domain = CONNECTION_DOMAIN_INET;
	} else {
		ret = utils_is_address_ipv6(addr);
		if (ret != 1) {
			/* At this point, the addr is either v4 nor v6 so error. */
			ERR("Config file unknown tor address: %s", addr);
			goto error;
		}
		config->conf_file.tor_domain = CONNECTION_DOMAIN_INET6;
	}
	config->conf_file.tor_address = strdup(addr);

	DBG("Config file setting tor address to %s", addr);
	ret = 0;

error:
	return ret;
}

static int set_log_level(const char *level, struct configuration *config)
{
    int ret;
    char *endptr, *endptr2;
    int _level;

    assert(level);
    assert(config);

    _level = strtoimax(*level, &endptr, 10);
    if (_level == 0) {
        /* If the level is not set, set it to the default. */
        WARN("[config] Log level not set.");
        WARN("Using default level: %s", DEFAULT_LOG_LEVEL);
        _level = strtoimax(DEFAULT_LOG_LEVEL, &endptr2, 16);
    } else {
        if (_level > 5) {
            /* If the log level was set to higher than we allow, or if
             * strtoimax returned INTMAX_MAX or UINTMAX_MAX, then set it to
             * the highest we allow. */
            ERR("[config] Log level %s not understood.", level);
            WARN("[config] Setting log level to highest value: 5");
            _level = 5;
        }
    }
    assert( 5 >= _level >= 1);

    config->conf_file.log_level = _level;
    DBG("Config file setting log level to %d", _level);
    ret = 0;

end:
error:
    return ret;
}

static int set_log_time(const char *ltime, struct configuration *config)
{
    int ret;
    char *endptr;
    int _ltime;

    assert(ltime);
    assert(config);

    ret = 0;
    _ltime = strtoul(ltime, &endptr, 10);
    if (_ltime != 0 && _ltime != 1) {
        ERR("[config] LogTimestamps must be 0 or 1.");
        // XXX strtoul will set to 0 if unset, which *isn't* the default.
        _ltime = strtoul(DEFAULT_LOG_TIME_STATUS, &endptr, 16);
        ret = 1;
    }
    assert( (_ltime == 0) || (_ltime == 1) );

    config->conf_file.log_timestamps = _ltime;
    goto end;

end:
error:
    return ret;
}

/*
 * Parse a single line of from a configuration file and set the value found in
 * the configuration object.
 *
 * Return 0 on success or else a negative value.
 */
static int parse_config_line(const char *line, struct configuration *config)
{
	int ret, nb_token;
	char *tokens[DEFAULT_MAX_CONF_TOKEN];

	assert(line);
	assert(config);

	/*
	 * The line is tokenized and each token is NULL terminated.
	 */
	nb_token = utils_tokenize_ignore_comments(line, sizeof(tokens), tokens);
	if (nb_token <= 0) {
		/* Nothing on this line that is useful to parse. */
		ret = 0;
		goto end;
	}

	if (!strcmp(tokens[0], conf_toraddr_str)) {
		ret = set_tor_address(tokens[1], config);
		if (ret < 0) {
			goto error;
		}
	} else if (!strcmp(tokens[0], conf_torport_str)) {
		ret = set_tor_port(tokens[1], config);
		if (ret < 0) {
			goto error;
		}
	} else if (!strcmp(tokens[0], conf_onion_str)) {
		ret = set_onion_info(tokens[1], config);
		if (ret < 0) {
			goto error;
		}
	} else {
		WARN("Config file contains unknown value: %s", line);
	}

	/* Everything went well. */
	ret = 0;

end:
error:
	return ret;
}

/*
 * Parse the configuration file with the given file pointer into the
 * configuration object.
 *
 * Return 0 on success or else a negative value.
 */
static int parse_config_file(FILE *fp, struct configuration *config)
{
	int ret = -1;
	/* Usually, this value is 8192 on most Unix systems. */
	char line[BUFSIZ];

	assert(fp);
	assert(config);

	while (fgets(line, sizeof(line), fp) != NULL) {
		/*
		 * Remove the \n at the end of the buffer and replace it by a NULL
		 * bytes so we handle the line without this useless char.
		 */
		if (strlen(line) > 0) {
			line[strlen(line) - 1] = '\0';
		}

		ret = parse_config_line(line, config);
		if (ret < 0) {
			goto error;
		}
	}

error:
	return ret;
}

/*
 * Read and populate the given config parsed data structure.
 *
 * Return 0 on success or else a negative value.
 */
int config_file_read(const char *filename, struct configuration *config)
{
	int ret;
	FILE *fp;

	assert(config);

	/* Clear out the structure */
	memset(config, 0x0, sizeof(*config));

	/* If a filename wasn't provided, use the default. */
	if (!filename) {
		filename = DEFAULT_CONF_FILE;
		DBG("Config file not provided by TORSOCKS_CONF_FILE. Using default %s",
				filename);
	}

	fp = fopen(filename, "r");
	if (!fp) {
		WARN("Config file not found: %s. Using default for Tor", filename);
		(void) set_tor_address(DEFAULT_TOR_ADDRESS, config);
		/*
		 * We stringify the default value here so we can print the debug
		 * statement in the function call to set port.
		 */
		(void) set_tor_port(XSTR(DEFAULT_TOR_PORT), config);

		ret = set_onion_info(
				DEFAULT_ONION_ADDR_RANGE "/" DEFAULT_ONION_ADDR_MASK, config);
		if (!ret) {
			/* ENOMEM is probably the only case here. */
			goto error;
		}
		goto end;
	}

	ret = parse_config_file(fp, config);
	if (ret < 0) {
		goto error;
	}

	DBG("Config file %s opened and parsed.", filename);

end:
error:
	if (fp) {
		fclose(fp);
	}
	return ret;
}

/*
 * Free everything inside a configuration file object. It is the caller
 * responsability to free the object if needed.
 */
void config_file_destroy(struct config_file *conf)
{
	assert(conf);

	free(conf->tor_address);
}
