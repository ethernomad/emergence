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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "llist.h"
#include "vertex.h"
#include "polygon.h"
#include "gsub.h"
#include "nodes.h"
#include "conns.h"
#include "curves.h"
#include "points.h"
#include "fills.h"
#include "tiles.h"
#include "map.h"
#include "cameron.h"
#include "bsp.h"
#include "objects.h"
#include "main.h"
#include "interface.h"
#include "worker_thread.h"
#include "main_lock.h"
#include "worker.h"


int job_pending = 0;			// start_working has been called, 
								// but job_complete_callback has not been reached yet
int stopping = 0;				// stop_working has been called, 
								// but job_complete_callback has not been reached yet
int restarting = 0;				// when job_complete_callback is reached, start another job
int job_type = JOB_TYPE_SLEEP;	// the job to be / being done in worker thread
int compiled = 1;				// map compilation waits until necessary all work has been done

int all_work_done = 0;


int check_stop_callback()
{
	if(!worker_try_enter_main_lock())
		return 1;

	worker_leave_main_lock();

	return 0;
}


int in_lock_check_stop_callback()
{
	worker_leave_main_lock();
	
	if(!worker_try_enter_main_lock())
		return 1;

	return 0;
}


void start()
{
	switch(job_type)
	{
	case JOB_TYPE_BSP:
		generate_bsp_tree();
		break;
	
	case JOB_TYPE_UI_BSP:
		generate_ui_bsp_tree();
		break;
	
	case JOB_TYPE_RESAMPLING_OBJECT:
		resample_object();
		break;
	
	case JOB_TYPE_SCALING_OBJECT:
		scale_object();
		break;
	
	case JOB_TYPE_SCALING:
		scale_tiles();
		break;
	
	case JOB_TYPE_CONN_VERTICIES:
		generate_verticies_for_all_conns();
		break;
	
	case JOB_TYPE_CONN_SQUISH:
		generate_squished_textures_for_all_conns();
		break;
	
	case JOB_TYPE_FILL_VERTICIES:
		generate_fill_verticies();
		break;
	
	case JOB_TYPE_NODE_VERTICIES:
		generate_verticies_for_all_nodes();
		break;

	case JOB_TYPE_TILING:
		tile();
		break;
	
	case JOB_TYPE_RENDERING:
		render_tile();
		break;
	
	case JOB_TYPE_CAMERON:
		scale_cameron();
		break;
	}	
	
	post_job_finished();
}


void check_job_ui_bsp()
{
	if(generate_ui_bsp)
	{
		if(job_type != JOB_TYPE_UI_BSP)
		{
			printf("generating ui bsp tree\n");
			all_work_done = 0;
		}
		
		job_type = JOB_TYPE_UI_BSP;
		call_start();
		job_pending = 1;
		compiled = 0;
		return;
	}
}


void check_job_bsp()
{
	if(generate_bsp)
	{
		if(job_type != JOB_TYPE_BSP)
		{
			printf("generating bsp tree\n");
			all_work_done = 0;
		}
		
		job_type = JOB_TYPE_BSP;
		call_start();
		job_pending = 1;
		compiled = 0;
		return;
	}
}


void check_job_resample_object()
{
	if(check_for_unresampled_objects())
	{
		if(job_type != JOB_TYPE_RESAMPLING_OBJECT)
		{
			printf("resampling objects\n");
			all_work_done = 0;
		}
		
		job_type = JOB_TYPE_RESAMPLING_OBJECT;
		call_start();
		job_pending = 1;
		return;
	}
}


void check_job_scale_object()
{
	if(check_for_unscaled_objects())
	{
		if(job_type != JOB_TYPE_SCALING_OBJECT)
		{
			printf("scaling objects\n");
			all_work_done = 0;
		}
		
		job_type = JOB_TYPE_SCALING_OBJECT;
		call_start();
		job_pending = 1;
		return;
	}
}


void check_job_scaling()
{
	if(check_for_unscaled_tiles())
	{
		if(job_type != JOB_TYPE_SCALING)
		{
			printf("scaling tiles\n");
			all_work_done = 0;
		}
		
		job_type = JOB_TYPE_SCALING;
		call_start();
		job_pending = 1;
		return;
	}
}


void check_job_conn_verticies()
{
	if(check_for_unverticied_conns())
	{
		if(job_type != JOB_TYPE_CONN_VERTICIES)
		{
			printf("generating conn verticies\n");
			all_work_done = 0;
		}
		
		job_type = JOB_TYPE_CONN_VERTICIES;
		call_start();
		job_pending = 1;
		compiled = 0;
		return;
	}
}


void check_job_conn_squish()
{
	if(check_for_unsquished_conns())
	{
		if(job_type != JOB_TYPE_CONN_SQUISH)
		{
			printf("squishing conn textures\n");
			all_work_done = 0;
		}
		
		job_type = JOB_TYPE_CONN_SQUISH;
		call_start();
		job_pending = 1;
		compiled = 0;
		return;
	}
}


void check_job_fill_verticies()
{
	if(check_for_unverticied_fills())
	{
		if(job_type != JOB_TYPE_FILL_VERTICIES)
		{
			printf("generating fill verticies\n");
			all_work_done = 0;
		}
		
		job_type = JOB_TYPE_FILL_VERTICIES;
		call_start();
		job_pending = 1;
		compiled = 0;
		return;
	}
}


void check_job_node_verticies()
{
	if(check_for_unverticied_nodes())
	{
		if(job_type != JOB_TYPE_NODE_VERTICIES)
		{
			printf("generating node verticies\n");
			all_work_done = 0;
		}
		
		job_type = JOB_TYPE_NODE_VERTICIES;
		call_start();
		job_pending = 1;
		compiled = 0;
		return;
	}
}


void check_job_tiling()
{
	if(check_for_tiling())
	{
		if(job_type != JOB_TYPE_TILING)
		{
			printf("tiling\n");
			all_work_done = 0;
		}
		
		job_type = JOB_TYPE_TILING;
		call_start();
		job_pending = 1;
		compiled = 0;
		return;
	}
}


void check_job_rendering()
{
	if(check_for_unrendered_tile())
	{
		if(job_type != JOB_TYPE_RENDERING)
		{
			printf("rendering tiles\n");
			all_work_done = 0;
		}
		
		job_type = JOB_TYPE_RENDERING;
		call_start();
		job_pending = 1;
		compiled = 0;
		return;
	}
}


void check_job_cameron()
{
	if(cameron_pending)
	{
		if(job_type != JOB_TYPE_CAMERON)
		{
			printf("scaling splash screen\n");
			all_work_done = 0;
		}
		
		job_type = JOB_TYPE_CAMERON;
		call_start();
		job_pending = 1;
		return;
	}
}


//
// INTERFACE FUNCTIONS
//


void start_working()
{
	if(stopping)
	{
		restarting = 1;
		return;
	}
	
	if(job_pending)
		return;

	
	// find a job to do
	
	check_job_ui_bsp();	if(job_pending) return;
	
	if(view_state & VIEW_OBJECTS)
	{
		check_job_resample_object();	if(job_pending) return;
		check_job_scale_object();		if(job_pending) return;
	}
	
	check_job_scaling();			if(job_pending) return;
	check_job_conn_verticies();		if(job_pending) return;
	check_job_conn_squish();		if(job_pending) return;
	check_job_fill_verticies();		if(job_pending) return;
	check_job_node_verticies();		if(job_pending) return;
	check_job_tiling();				if(job_pending) return;
	check_job_rendering();			if(job_pending) return;
	
	check_job_resample_object();	if(job_pending) return;		// break me up?

	check_job_bsp();				if(job_pending) return;

	
	// there is nothing to be done
	
	if(compiling)
	{
		destroy_compile_dialog();
		compile();
		compiling = 0;
	}
	
	compiled = 1;
	
	check_job_scale_object();		if(job_pending) return;
	check_job_cameron();			if(job_pending) return;
	
	if(!all_work_done)
	{
		printf("all work done\n");
		all_work_done = 1;
	}
	
	job_type = JOB_TYPE_SLEEP;
}


void stop_working()
{
	if(job_pending)
	{
		if(!stopping)
		{
			enter_main_lock();
			stopping = 1;
		}
		
		restarting = 0;
	}
}


void job_complete_callback()
{
	int update = 0;

	if(job_pending)
	{
		if(stopping)
		{
			stopping = 0;
			leave_main_lock();
		}
		else
		{
			switch(job_type)
			{
			case JOB_TYPE_BSP:
				finished_generating_bsp_tree();
				if(view_state & VIEW_BSP_TREE)
					update = 1;
				break;
						
			case JOB_TYPE_UI_BSP:
				finished_generating_ui_bsp_tree();
				break;
						
			case JOB_TYPE_RESAMPLING_OBJECT:
				finished_resampling_object();
				if(view_state & VIEW_OBJECTS)
					update = 1;
				break;
							
			case JOB_TYPE_SCALING_OBJECT:
				finished_scaling_object();
				if(view_state & VIEW_OBJECTS)
					update = 1;
				break;

			case JOB_TYPE_SCALING:
				update = finished_scaling_tiles();
				break;
						
			case JOB_TYPE_CONN_VERTICIES:
				break;
				
			case JOB_TYPE_CONN_SQUISH:
				break;
						
			case JOB_TYPE_FILL_VERTICIES:
				break;
				
			case JOB_TYPE_NODE_VERTICIES:
				break;

			case JOB_TYPE_TILING:
				finished_tiling();
				if(view_state & (VIEW_BOXES | VIEW_TILES))
					update = 1;
				break;
						
			case JOB_TYPE_RENDERING:
				update = finished_rendering_tile();
				break;
				
			case JOB_TYPE_CAMERON:
				cameron_finished();
				update = 1;
				break;
			}
			
			restarting = 1;
		}
		
		job_pending = 0;
	}
	
	if(restarting)
	{
		start_working();
		restarting = 0;
	}
	
	if(update)
		update_client_area();
}


int init_worker()
{
	gsub_callback = check_stop_callback;
	
	create_main_lock();
	start_worker_thread();
	
	return 1;
}


void kill_worker()
{
	stop_working();
	kill_worker_thread();
}
