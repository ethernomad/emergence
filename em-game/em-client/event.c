/**
 * @file
 * @copyright 1998-2004 Jonathan Brown <jbrown@bluedroplet.com>
 * @license https://www.gnu.org/licenses/gpl-3.0.html
 * @homepage https://github.com/bluedroplet/emergence
 */

#ifdef LINUX
#define _GNU_SOURCE
#define _REENTRANT
#endif

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory.h>

#include "types.h"
#include "stringbuf.h"
#include "buffer.h"
#include "llist.h"
#include "network.h"
#include "sgame.h"

#include "game.h"
#include "tick.h"
