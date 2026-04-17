/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifdef LINUX
#define _GNU_SOURCE
#ifndef _REENTRANT
#define _REENTRANT
#endif
#endif

#include <stdint.h>

#include <zlib.h>

#include "objects.h"

void gzwrite_floating_images(gzFile file)
{
	int c = count_object_floating_images();
	
	gzwrite(file, &c, 4);
	
	gzwrite_object_floating_images(file);
}
