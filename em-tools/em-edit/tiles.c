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
#include <memory.h>
#include <math.h>
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
#include "worker.h"
#include "main_lock.h"
#include "interface.h"
#include "main.h"


#define TILEWIDTH 200
#define TILEHEIGHT 200


struct tile_t *clean_tile0 = NULL;
struct tile_t *dirty_tile0 = NULL;
struct tile_t *scaling_tile = NULL;
	
struct surface_t *out_surface = NULL;
	
int tile_collation_pending = 0;

void add_tile(struct tile_t **tile0, struct tile_t *tile)
{
	// check tile0 is not NULL

	if(!tile0 || !tile)
		return;

	LL_ADD(struct tile_t, tile0, tile);
}


void delete_tile(struct tile_t **tile0, struct tile_t *tile)
{
	if(!tile0 || !*tile0 || !tile)
		return;

	if(*tile0 == tile)
	{
		*tile0 = (*tile0)->next;
	}
	else
	{
		struct tile_t *ctile = *tile0;
		
		while(ctile->next)
		{
			if(ctile->next == tile)
			{
				ctile->next = ctile->next->next;
				break;
			}

			ctile = ctile->next;
		}
	}

	LL_REMOVE_ALL(struct conn_pointer_t, &tile->connp0);
	free_surface(tile->surface);
	free_surface(tile->scaled_surface);
	free(tile);
}


void make_dirty_tiles()		// always called when not working
{
	// dump current dirty tiles

	struct tile_t *ctile = dirty_tile0;

	while(ctile)
	{
		LL_REMOVE_ALL(struct conn_pointer_t, &ctile->connp0);
		LL_REMOVE_ALL(struct node_pointer_t, &ctile->nodep0);
		LL_REMOVE_ALL(struct fill_pointer_t, &ctile->fillp0);
		free_surface(ctile->surface);
		ctile = ctile->next;
	}

	LL_REMOVE_ALL(struct tile_t, &dirty_tile0);

	// make new ones

	struct tile_t tile;
	ctile = clean_tile0;

	while(ctile)
	{
		tile.x1 = ctile->x1;
		tile.y1 = ctile->y1;
		tile.x2 = ctile->x2;
		tile.y2 = ctile->y2;
		tile.connp0 = NULL;
		tile.nodep0 = NULL;
		tile.fillp0 = NULL;

		struct conn_pointer_t *connp = ctile->connp0;

		while(connp)
		{
			add_conn_pointer(&tile.connp0, connp->conn);
			connp = connp->next;
		}

		struct node_pointer_t *nodep = ctile->nodep0;

		while(nodep)
		{
			add_node_pointer(&tile.nodep0, nodep->node);
			nodep = nodep->next;
		}

		struct fill_pointer_t *fillp = ctile->fillp0;

		while(fillp)
		{
			add_fill_pointer(&tile.fillp0, fillp->fill);
			fillp = fillp->next;
		}

		tile.surface = duplicate_surface(ctile->surface);
		tile.scaled_surface = duplicate_surface(ctile->scaled_surface);

		LL_ADD(struct tile_t, &dirty_tile0, &tile);
		ctile = ctile->next;
	}
}


void make_clean_tiles()		// always called when not working
{
	// dump current clean tiles

	struct tile_t *ctile = clean_tile0;

	while(ctile)
	{
		LL_REMOVE_ALL(struct conn_pointer_t, &ctile->connp0);
		LL_REMOVE_ALL(struct node_pointer_t, &ctile->nodep0);
		LL_REMOVE_ALL(struct fill_pointer_t, &ctile->fillp0);
		free_surface(ctile->surface);
		ctile = ctile->next;
	}

	LL_REMOVE_ALL(struct tile_t, &clean_tile0);


	// make new ones

	struct tile_t tile;
	ctile = dirty_tile0;

	while(ctile)
	{
		tile.x1 = ctile->x1;
		tile.y1 = ctile->y1;
		tile.x2 = ctile->x2;
		tile.y2 = ctile->y2;
		tile.connp0 = NULL;
		tile.nodep0 = NULL;
		tile.fillp0 = NULL;

		struct conn_pointer_t *connp = ctile->connp0;

		while(connp)
		{
			add_conn_pointer(&tile.connp0, connp->conn);
			connp = connp->next;
		}

		struct node_pointer_t *nodep = ctile->nodep0;

		while(nodep)
		{
			add_node_pointer(&tile.nodep0, nodep->node);
			nodep = nodep->next;
		}
		
		struct fill_pointer_t *fillp = ctile->fillp0;

		while(fillp)
		{
			add_fill_pointer(&tile.fillp0, fillp->fill);
			fillp = fillp->next;
		}

		tile.surface = duplicate_surface(ctile->surface);
		tile.scaled_surface = duplicate_surface(ctile->scaled_surface);

		LL_ADD(struct tile_t, &clean_tile0, &tile);
		ctile = ctile->next;
	}
}


void mark_conns_with_tile_as_untiled(struct tile_t *tile)		// always called when not working
{
	struct conn_pointer_t *cconnp = tile->connp0;

	while(cconnp)
	{
		cconnp->conn->tiled = 0;
		cconnp = cconnp->next;
	}
}


void mark_nodes_with_tile_as_untiled(struct tile_t *tile)		// always called when not working
{
	struct node_pointer_t *cnodep = tile->nodep0;

	while(cnodep)
	{
		cnodep->node->texture_tiled = 0;
		cnodep = cnodep->next;
	}
}


void mark_fills_with_tile_as_untiled(struct tile_t *tile)		// always called when not working
{
	struct fill_pointer_t *cfillp = tile->fillp0;

	while(cfillp)
	{
		cfillp->fill->texture_tiled = 0;
		cfillp = cfillp->next;
	}
}


void delete_images_of_tiles_with_conn(struct conn_t *conn)		// always called when not working
{
	struct tile_t *ctile = clean_tile0;

	while(ctile)
	{
		struct conn_pointer_t *cconnp = ctile->connp0;

		while(cconnp)
		{
			if(cconnp->conn == conn)
			{
				free_surface(ctile->surface);
				ctile->surface = NULL;
				free_surface(ctile->scaled_surface);
				ctile->scaled_surface = NULL;
				break;
			}

			cconnp = cconnp->next;
		}

		ctile = ctile->next;
	}
}


void delete_images_of_tiles_with_node(struct node_t *node)		// always called when not working
{
	struct tile_t *ctile = clean_tile0;

	while(ctile)
	{
		struct node_pointer_t *cnodep = ctile->nodep0;

		while(cnodep)
		{
			if(cnodep->node == node)
			{
				free_surface(ctile->surface);
				ctile->surface = NULL;
				free_surface(ctile->scaled_surface);
				ctile->scaled_surface = NULL;
				break;
			}

			cnodep = cnodep->next;
		}

		ctile = ctile->next;
	}
}


void delete_images_of_tiles_with_fill(struct fill_t *fill)		// always called when not working
{
	struct tile_t *ctile = clean_tile0;

	while(ctile)
	{
		struct fill_pointer_t *cfillp = ctile->fillp0;

		while(cfillp)
		{
			if(cfillp->fill == fill)
			{
				free_surface(ctile->surface);
				ctile->surface = NULL;
				free_surface(ctile->scaled_surface);
				ctile->scaled_surface = NULL;
				break;
			}

			cfillp = cfillp->next;
		}

		ctile = ctile->next;
	}
}


void remove_conn_from_tiles(struct conn_t *conn)		// always called when not working
{
	struct tile_t *ctile = clean_tile0;

	while(ctile)
	{
		struct conn_pointer_t *cconnp = ctile->connp0;

		while(cconnp)
		{
			if(cconnp->conn == conn)
			{
				free_surface(ctile->surface);
				ctile->surface = NULL;
				free_surface(ctile->scaled_surface);
				ctile->scaled_surface = NULL;
				LL_REMOVE(struct conn_pointer_t, &ctile->connp0, cconnp);
				break;
			}

			cconnp = cconnp->next;
		}
		
		if(!ctile->connp0 && !ctile->nodep0 && !ctile->fillp0)
		{
			struct tile_t *temp = ctile->next;
			LL_REMOVE(struct tile_t, &clean_tile0, ctile);
			ctile = temp;
		}
		else
		{
			ctile = ctile->next;
		}
	}
}


void remove_node_from_tiles(struct node_t *node)		// always called when not working
{
	struct tile_t *ctile = clean_tile0;

	while(ctile)
	{
		struct node_pointer_t *cnodep = ctile->nodep0;

		while(cnodep)
		{
			if(cnodep->node == node)
			{
				free_surface(ctile->surface);
				ctile->surface = NULL;
				free_surface(ctile->scaled_surface);
				ctile->scaled_surface = NULL;
				LL_REMOVE(struct node_pointer_t, &ctile->nodep0, cnodep);
				break;
			}

			cnodep = cnodep->next;
		}
		
		if(!ctile->connp0 && !ctile->nodep0 && !ctile->fillp0)
		{
			struct tile_t *temp = ctile->next;
			LL_REMOVE(struct tile_t, &clean_tile0, ctile);
			ctile = temp;
		}
		else
		{
			ctile = ctile->next;
		}
	}
}


void remove_fill_from_tiles(struct fill_t *fill)		// always called when not working
{
	struct tile_t *ctile = clean_tile0;

	while(ctile)
	{
		struct fill_pointer_t *cfillp = ctile->fillp0;

		while(cfillp)
		{
			if(cfillp->fill == fill)
			{
				free_surface(ctile->surface);
				ctile->surface = NULL;
				free_surface(ctile->scaled_surface);
				ctile->scaled_surface = NULL;
				LL_REMOVE(struct fill_pointer_t, &ctile->fillp0, cfillp);
				break;
			}

			cfillp = cfillp->next;
		}
		
		if(!ctile->connp0 && !ctile->nodep0 && !ctile->fillp0)
		{
			struct tile_t *temp = ctile->next;
			LL_REMOVE(struct tile_t, &clean_tile0, ctile);
			ctile = temp;
		}
		else
		{
			ctile = ctile->next;
		}
	}
}


void invalidate_all_scaled_tiles()
{
	struct tile_t *tile = clean_tile0;

	while(tile)
	{
		free_surface(tile->scaled_surface);
		tile->scaled_surface = NULL;
		tile = tile->next;
	}
}


int conn_fully_rendered(struct conn_t *conn)
{
	struct tile_t *ctile = clean_tile0;

	if(!ctile)
		return 0;

	if(zoom == 1.0)
	{
		while(ctile)
		{
			if(!ctile->surface && conn_in_connp_list(ctile->connp0, conn))
				return 0;
	
			ctile = ctile->next;
		}
	}
	else
	{
		while(ctile)
		{
			if(!ctile->scaled_surface && conn_in_connp_list(ctile->connp0, conn))
				return 0;
	
			ctile = ctile->next;
		}
	}

	return 1;
}


void draw_tiles()
{
	struct blit_params_t params;
	params.dest = s_backbuffer;
	
	struct tile_t *tile = clean_tile0;

	while(tile)
	{
		int minx, miny, maxx, maxy;

		world_to_screen(tile->x1, tile->y1, &minx, &maxy);
		world_to_screen(tile->x2, tile->y2, &maxx, &miny);

		if(tile->surface)
		{
			params.dest_x = minx;
			params.dest_y = miny;

			if(zoom == 1.0)
				params.source = tile->surface;
			else
				params.source = tile->scaled_surface;
			
			blit_surface(&params);
		}
		
		tile = tile->next;
	}
}


void draw_boxes()
{
	struct tile_t *tile = clean_tile0;

	while(tile)
	{
		draw_world_clipped_line(tile->x1, tile->y2, tile->x2, tile->y2);
		draw_world_clipped_line(tile->x2, tile->y2, tile->x2, tile->y1);
		draw_world_clipped_line(tile->x2, tile->y1, tile->x1, tile->y1);
		draw_world_clipped_line(tile->x1, tile->y1, tile->x1, tile->y2);

		tile = tile->next;
	}
}


struct tile_t *get_tile(double x, double y)
{
	struct tile_t *tile = clean_tile0;

	while(tile)
	{
		if(x >= tile->x1 && x <= tile->x2 && 
			y >= tile->y1 && y <= tile->y2)
			return tile;

		tile = tile->next;
	}

	return NULL;
}


int count_tiles()
{
	struct tile_t *tile = clean_tile0;
	int count = 0;

	while(tile)
	{
		count++;
		tile = tile->next;
	}

	return count;
}


void delete_all_tiles()		// always called when not working
{
	struct tile_t *ctile = dirty_tile0;

	while(ctile)
	{
		free_surface(ctile->surface);
		free_surface(ctile->scaled_surface);
		LL_REMOVE_ALL(struct conn_pointer_t, &ctile->connp0);
		LL_REMOVE_ALL(struct node_pointer_t, &ctile->nodep0);
		LL_REMOVE_ALL(struct fill_pointer_t, &ctile->fillp0);

		ctile = ctile->next;
	}

	ctile = clean_tile0;

	while(ctile)
	{
		free_surface(ctile->surface);
		free_surface(ctile->scaled_surface);
		LL_REMOVE_ALL(struct conn_pointer_t, &ctile->connp0);
		LL_REMOVE_ALL(struct node_pointer_t, &ctile->nodep0);
		LL_REMOVE_ALL(struct fill_pointer_t, &ctile->fillp0);

		ctile = ctile->next;
	}

	LL_REMOVE_ALL(struct tile_t, &dirty_tile0);
	LL_REMOVE_ALL(struct tile_t, &clean_tile0);
}


int check_for_unscaled_tiles()
{
	if(zoom != 1.0)
	{
		struct tile_t *ctile = clean_tile0;
		
		while(ctile)
		{
			if(ctile->surface && !ctile->scaled_surface)
			{
				make_dirty_tiles();
				return 1;
			}
			
			ctile = ctile->next;
		}
	}
	
	return 0;
}


int finished_scaling_tiles()
{
	make_clean_tiles();
	
	update_fully_rendered_conns();
	
	if(view_state & VIEW_TILES)
		return 1;
	
	return 0;
}

struct texture_verts_t *texture_verts0 = NULL;
struct texture_polys_t *texture_polys0 = NULL;
int width, height, top, left;

struct tile_t *rendering_tile;

int check_for_unrendered_tile()
{
	struct tile_t *ctile = clean_tile0;
	
	while(ctile)
	{
		if(!ctile->surface)
		{
			width = ctile->x2 - ctile->x1;
			height = ctile->y2 - ctile->y1;
			left = ctile->x1;
			top = ctile->y1;
			
			struct texture_verts_t texture_verts;
			struct texture_polys_t texture_polys;
			
			struct conn_pointer_t *connp = ctile->connp0;
			
			while(connp)
			{
				struct curve_t *curve = get_curve(connp->conn);
				assert(curve);
				
				assert(connp->conn->squished_texture);
				assert(connp->conn->squished_texture->width);
				assert(connp->conn->squished_texture->height);
				assert(connp->conn->verts);
				
				texture_verts.surface = duplicate_surface(connp->conn->squished_texture);
				texture_verts.verts = malloc(sizeof(struct vertex_t) * 
					(connp->conn->squished_texture->width + 1) * 
					(connp->conn->squished_texture->height + 1));
				memcpy(texture_verts.verts, connp->conn->verts, 
					(connp->conn->squished_texture->width + 1) * 
					(connp->conn->squished_texture->height + 1) * sizeof(struct vertex_t));
				
				LL_ADD(struct texture_verts_t, &texture_verts0, &texture_verts);
				
				connp = connp->next;
			}
			
			
			struct node_pointer_t *nodep = ctile->nodep0;
			
			while(nodep)
			{
				texture_verts.surface = duplicate_surface(nodep->node->texture_surface);
				texture_verts.verts = malloc(sizeof(struct vertex_t) * 
					(nodep->node->texture_surface->width + 1) * 
					(nodep->node->texture_surface->height + 1));
				memcpy(texture_verts.verts, nodep->node->texture_verts, 
					(nodep->node->texture_surface->width + 1) * 
					(nodep->node->texture_surface->height + 1) * sizeof(struct vertex_t));
				
				LL_ADD(struct texture_verts_t, &texture_verts0, &texture_verts);
				
				nodep = nodep->next;
			}
			
			
			struct fill_pointer_t *fillp = ctile->fillp0;
			
			while(fillp)
			{
				struct texture_verts_t *ctexture_verts = fillp->fill->texture_verts0;
					
				while(ctexture_verts)
				{
					texture_verts.surface = duplicate_surface(ctexture_verts->surface);
					convert_surface_to_24bitalpha8bit(texture_verts.surface);
					texture_verts.verts = malloc(sizeof(struct vertex_t) * 
						(ctexture_verts->surface->width + 1) * 
						(ctexture_verts->surface->height + 1));
					memcpy(texture_verts.verts, ctexture_verts->verts, 
						(ctexture_verts->surface->width + 1) * 
						(ctexture_verts->surface->height + 1) * sizeof(struct vertex_t));
							
					LL_ADD(struct texture_verts_t, &texture_verts0, &texture_verts);
					
					ctexture_verts = ctexture_verts->next;
				}
				
				struct texture_polys_t *ctexture_polys = fillp->fill->texture_polys0;
					
				while(ctexture_polys)
				{
					texture_polys.surface = duplicate_surface(ctexture_polys->surface);
					convert_surface_to_24bitalpha8bit(texture_polys.surface);
					
					texture_polys.polys = calloc(ctexture_polys->surface->width * 
						ctexture_polys->surface->height, sizeof(struct vertex_ll_t*));
					
					int i;
					
					struct vertex_ll_t **invertexp = ctexture_polys->polys;
					struct vertex_ll_t **outvertexp = texture_polys.polys;
						
					for(i = ctexture_polys->surface->width * ctexture_polys->surface->height; 
						i > 0; i--)
					{
						struct vertex_ll_t *cvertex = *invertexp;
							
						while(cvertex)
						{
							LL_ADD_TAIL(struct vertex_ll_t, outvertexp, cvertex)
							cvertex = cvertex->next;
						}

						invertexp++;
						outvertexp++;
					}
						
					LL_ADD(struct texture_polys_t, &texture_polys0, &texture_polys);

					ctexture_polys = ctexture_polys->next;
				}
				
				
				fillp = fillp->next;
				
			}
			
			rendering_tile = ctile;
			
			return 1;
		}
		
		ctile = ctile->next;
	}
	
	return 0;
}


int finished_rendering_tile()
{
	rendering_tile->surface = out_surface;
	
	update_fully_rendered_conns();
	
	if(zoom == 1.0)
		return 1;
	
	return 0;
}


int check_for_tiling()
{
	if(!check_for_untiled_conns())
	{
		if(!check_for_untiled_node_textures())
		{
			if(!check_for_untiled_fills())
			{
				return 0;
			}
		}
	}
	
	make_dirty_tiles();
	
	return 1;
}


void finished_tiling()
{
	finished_tiling_all_conns();
	finished_tiling_all_node_textures();
	finished_tiling_all_fills();
	make_clean_tiles();
}


//
// WORKER THREAD FUNCTIONS
//


void collate_all_tiles()
{
	int no_changes;

	do
	{
		no_changes = 1;

		struct tile_t *tile1 = dirty_tile0;

		while(tile1)
		{
			struct tile_t *tile2 = dirty_tile0;

			while(tile2)
			{
				if(check_stop_callback())
					return;

				if(tile1 == tile2)
				{
					tile2 = tile2->next;
					continue;
				}

				// combine tiles

				if(tile1->x2 == tile2->x1 && 
					tile1->y1 == tile2->y1 &&
					tile1->y2 == tile2->y2)
				{
					struct tile_t tile;

					tile.x1 = tile1->x1;
					tile.y1 = tile1->y1;
					tile.x2 = tile2->x2;
					tile.y2 = tile1->y2;

					tile.surface = NULL;
					tile.scaled_surface = NULL;

					tile.connp0 = NULL;
					tile.nodep0 = NULL;
					tile.fillp0 = NULL;

					struct conn_pointer_t *connp = tile1->connp0;

					while(connp)
					{
						add_conn_pointer(&tile.connp0, connp->conn);
						connp = connp->next;
					}

					connp = tile2->connp0;

					while(connp)
					{
						add_conn_pointer(&tile.connp0, connp->conn);
						connp = connp->next;
					}

					struct node_pointer_t *nodep = tile1->nodep0;

					while(nodep)
					{
						add_node_pointer(&tile.nodep0, nodep->node);
						nodep = nodep->next;
					}

					nodep = tile2->nodep0;

					while(nodep)
					{
						add_node_pointer(&tile.nodep0, nodep->node);
						nodep = nodep->next;
					}

					struct fill_pointer_t *fillp = tile1->fillp0;

					while(fillp)
					{
						add_fill_pointer(&tile.fillp0, fillp->fill);
						fillp = fillp->next;
					}

					fillp = tile2->fillp0;

					while(fillp)
					{
						add_fill_pointer(&tile.fillp0, fillp->fill);
						fillp = fillp->next;
					}

					add_tile(&dirty_tile0, &tile);

					LL_REMOVE_ALL(struct conn_pointer_t, &tile1->connp0);
					LL_REMOVE_ALL(struct conn_pointer_t, &tile2->connp0);
					LL_REMOVE_ALL(struct node_pointer_t, &tile1->nodep0);
					LL_REMOVE_ALL(struct node_pointer_t, &tile2->nodep0);
					LL_REMOVE_ALL(struct fill_pointer_t, &tile1->fillp0);
					LL_REMOVE_ALL(struct fill_pointer_t, &tile2->fillp0);

					delete_tile(&dirty_tile0, tile1);
					delete_tile(&dirty_tile0, tile2);

					no_changes = 0;
					break;
				}


				if(tile1->y2 == tile2->y1 && 
					tile1->x1 == tile2->x1 &&
					tile1->x2 == tile2->x2)
				{
					struct tile_t tile;

					tile.x1 = tile1->x1;
					tile.y1 = tile1->y1;
					tile.x2 = tile1->x2;
					tile.y2 = tile2->y2;

					tile.surface = NULL;
					tile.scaled_surface = NULL;

					tile.connp0 = NULL;
					tile.nodep0 = NULL;
					tile.fillp0 = NULL;

					struct conn_pointer_t *connp = tile1->connp0;

					while(connp)
					{
						add_conn_pointer(&tile.connp0, connp->conn);
						connp = connp->next;
					}

					connp = tile2->connp0;

					while(connp)
					{
						add_conn_pointer(&tile.connp0, connp->conn);
						connp = connp->next;
					}

					struct node_pointer_t *nodep = tile1->nodep0;

					while(nodep)
					{
						add_node_pointer(&tile.nodep0, nodep->node);
						nodep = nodep->next;
					}

					nodep = tile2->nodep0;

					while(nodep)
					{
						add_node_pointer(&tile.nodep0, nodep->node);
						nodep = nodep->next;
					}
					
					struct fill_pointer_t *fillp = tile1->fillp0;

					while(fillp)
					{
						add_fill_pointer(&tile.fillp0, fillp->fill);
						fillp = fillp->next;
					}

					fillp = tile2->fillp0;

					while(fillp)
					{
						add_fill_pointer(&tile.fillp0, fillp->fill);
						fillp = fillp->next;
					}
					
					add_tile(&dirty_tile0, &tile);

					LL_REMOVE_ALL(struct conn_pointer_t, &tile1->connp0);
					LL_REMOVE_ALL(struct conn_pointer_t, &tile2->connp0);
					LL_REMOVE_ALL(struct node_pointer_t, &tile1->nodep0);
					LL_REMOVE_ALL(struct node_pointer_t, &tile2->nodep0);
					LL_REMOVE_ALL(struct fill_pointer_t, &tile1->fillp0);
					LL_REMOVE_ALL(struct fill_pointer_t, &tile2->fillp0);

					delete_tile(&dirty_tile0, tile1);
					delete_tile(&dirty_tile0, tile2);

					no_changes = 0;
					break;
				}

				tile2 = tile2->next;
			}

			if(!no_changes)
				break;

			tile1 = tile1->next;
		}

	} while(!no_changes);
}


struct tile_t *intile(int x1, int y1, int x2, int y2)
{
	struct tile_t *tile = dirty_tile0;

	while(tile)
	{
		if(x1 >= tile->x1 && x2 <= tile->x2 && 
			y1 >= tile->y1 && y2 <= tile->y2)
			return tile;

		tile = tile->next;
	}

	return NULL;
}



void render_tile()
{
	assert(width);
	assert(height);
	
	out_surface = multiple_resample(texture_verts0, texture_polys0, 
		width, height, left, top);
	
	if(out_surface != NULL)
	{
		convert_surface_to_24bitalpha8bit(out_surface);
		surface_flip_vert(out_surface);
	}

	
	struct texture_verts_t *st = texture_verts0;

	while(st)
	{
		free_surface(st->surface);
		free(st->verts);
		st = st->next;
	}
	
	LL_REMOVE_ALL(struct texture_verts_t, &texture_verts0);
		
	
	struct texture_polys_t *texture_polys = texture_polys0;
	
	while(texture_polys)
	{
		int num = texture_polys->surface->width * texture_polys->surface->height;
		
		free_surface(texture_polys->surface);
		
		struct vertex_ll_t **vertex = texture_polys->polys;
		
		while(num)
		{
			LL_REMOVE_ALL(struct vertex_ll_t, vertex);
			vertex++;
			num--;
		}
			
		texture_polys = texture_polys->next;
	}
				
	LL_REMOVE_ALL(struct texture_polys_t, &texture_polys0);
}


void scale_tiles()		// called from worker thread
{
	if(!worker_try_enter_main_lock())
		return;

	struct tile_t *ctile = dirty_tile0;
	
	while(ctile)
	{
		if(ctile->surface && !ctile->scaled_surface)
		{
			int minx, miny, maxx, maxy;
			
			world_to_screen(ctile->x1, ctile->y1, &minx, &maxy);
			world_to_screen(ctile->x2, ctile->y2, &maxx, &miny);
			
//			if(crap_zoom)
				;//ctile->scaled_surface = crap_resize(ctile->surface, maxx - minx, maxy - miny);
//			else
				ctile->scaled_surface = leave_main_lock_and_resize_surface(
					ctile->surface, maxx - minx, maxy - miny);
			
			if(!ctile->scaled_surface)
				return;
			
			if(!worker_try_enter_main_lock())
				return;
		}
		
		ctile = ctile->next;
	}
	
	leave_main_lock();
}


int tile_node(struct node_t *node)
{
	if(!worker_try_enter_main_lock())
		return 0;

	if(!node->texture_surface)
	{
		worker_leave_main_lock();
		return 1;
	}
	
	
	int x, y, box_y, box_x;
	
	int width = node->texture_surface->width + 1;
	
	for(y = 0; y < node->texture_surface->height; y++)
	{
		for(x = 0; x < node->texture_surface->width; x++)
		{
			struct vertex_t *vert = &node->texture_verts[y * width + x];

			float minx = vert[0].x;
			if(vert[1].x < minx) minx = vert[1].x;
			if(vert[width].x < minx) minx = vert[width].x;
			if(vert[width + 1].x < minx) minx = vert[width + 1].x;

			float miny = vert[0].y;
			if(vert[1].y < miny) miny = vert[1].y;
			if(vert[width].y < miny) miny = vert[width].y;
			if(vert[width + 1].y < minx) miny = vert[width + 1].y;

			float maxx = vert[0].x;
			if(vert[1].x < maxx) maxx = vert[1].x;
			if(vert[width].x < maxx) maxx = vert[width].x;
			if(vert[width + 1].x < maxx) maxx = vert[width + 1].x;

			float maxy = vert[0].y;
			if(vert[1].y < maxy) maxy = vert[1].y;
			if(vert[width].y < maxy) maxy = vert[width].y;
			if(vert[width + 1].y < maxx) maxy = vert[width + 1].y;

			int box_minx = (int)(floorf(minx / TILEWIDTH));
			int box_miny = (int)(floorf(miny / TILEHEIGHT));
			int box_maxx = (int)(floorf(maxx / TILEWIDTH));
			int box_maxy = (int)(floorf(maxy / TILEHEIGHT));

			for(box_y = box_miny; box_y <= box_maxy; box_y++)
			{
				for(box_x = box_minx; box_x <= box_maxx; box_x++)
				{
					// check that there isn't already a block here

					struct tile_t *oldtile = intile(box_x * TILEWIDTH, box_y * TILEHEIGHT, 
						(box_x + 1) * TILEWIDTH, (box_y + 1) * TILEHEIGHT);

					if(oldtile)
					{
						if(add_node_pointer(&oldtile->nodep0, node))
						{
							free_surface(oldtile->surface);
							free_surface(oldtile->scaled_surface);
							oldtile->surface = NULL;
							oldtile->scaled_surface = NULL;
						}

						continue;
					}

					struct tile_t tile;

					tile.x1 = box_x * TILEWIDTH;
					tile.y1 = box_y * TILEHEIGHT;
					tile.x2 = (box_x + 1) * TILEWIDTH;
					tile.y2 = (box_y + 1) * TILEHEIGHT;

					tile.surface = NULL;
					tile.scaled_surface = NULL;

					tile.connp0 = NULL;
					tile.nodep0 = NULL;
					tile.fillp0 = NULL;
					
					add_node_pointer(&tile.nodep0, node);
					add_tile(&dirty_tile0, &tile);
				}
			}
		}
		
		if(in_lock_check_stop_callback())
			return 0;
	}

	worker_leave_main_lock();

	return 1;
}


int tile_fill(struct fill_t *fill)
{
	if(!worker_try_enter_main_lock())
		return 0;

/*	if(!fill->texture_surface)
	{
		worker_leave_main_lock();
		return 1;
	}
*/	
	
	int x, y, box_y, box_x;
	
	struct texture_verts_t *texture_verts = fill->texture_verts0;
	
	while(texture_verts)
	{
		int width = texture_verts->surface->width + 1;
		
		for(y = 0; y < texture_verts->surface->height; y++)
		{
			for(x = 0; x < texture_verts->surface->width; x++)
			{
				struct vertex_t *vert = &texture_verts->verts[y * width + x];
	
				float minx = vert[0].x;
				if(vert[1].x < minx) minx = vert[1].x;
				if(vert[width].x < minx) minx = vert[width].x;
				if(vert[width + 1].x < minx) minx = vert[width + 1].x;
	
				float miny = vert[0].y;
				if(vert[1].y < miny) miny = vert[1].y;
				if(vert[width].y < miny) miny = vert[width].y;
				if(vert[width + 1].y < minx) miny = vert[width + 1].y;
	
				float maxx = vert[0].x;
				if(vert[1].x < maxx) maxx = vert[1].x;
				if(vert[width].x < maxx) maxx = vert[width].x;
				if(vert[width + 1].x < maxx) maxx = vert[width + 1].x;
	
				float maxy = vert[0].y;
				if(vert[1].y < maxy) maxy = vert[1].y;
				if(vert[width].y < maxy) maxy = vert[width].y;
				if(vert[width + 1].y < maxx) maxy = vert[width + 1].y;
	
				int box_minx = (int)(floorf(minx / TILEWIDTH));
				int box_miny = (int)(floorf(miny / TILEHEIGHT));
				int box_maxx = (int)(floorf(maxx / TILEWIDTH));
				int box_maxy = (int)(floorf(maxy / TILEHEIGHT));
	
				for(box_y = box_miny; box_y <= box_maxy; box_y++)
				{
					for(box_x = box_minx; box_x <= box_maxx; box_x++)
					{
						// check that there isn't already a block here
	
						struct tile_t *oldtile = intile(box_x * TILEWIDTH, box_y * TILEHEIGHT, 
							(box_x + 1) * TILEWIDTH, (box_y + 1) * TILEHEIGHT);
	
						if(oldtile)
						{
							if(add_fill_pointer(&oldtile->fillp0, fill))
							{
								free_surface(oldtile->surface);
								free_surface(oldtile->scaled_surface);
								oldtile->surface = NULL;
								oldtile->scaled_surface = NULL;
							}
						
							continue;
						}
	
						struct tile_t tile;
	
						tile.x1 = box_x * TILEWIDTH;
						tile.y1 = box_y * TILEHEIGHT;
						tile.x2 = (box_x + 1) * TILEWIDTH;
						tile.y2 = (box_y + 1) * TILEHEIGHT;
	
						tile.surface = NULL;
						tile.scaled_surface = NULL;
	
						tile.connp0 = NULL;
						tile.nodep0 = NULL;
						tile.fillp0 = NULL;
						
						add_fill_pointer(&tile.fillp0, fill);
						add_tile(&dirty_tile0, &tile);
					}
				}
			}
			
			if(in_lock_check_stop_callback())
				return 0;
		}
		
		texture_verts = texture_verts->next;
	}
	
	
	struct texture_polys_t *texture_polys = fill->texture_polys0;
		
	while(texture_polys)
	{
		struct vertex_ll_t **vertex = texture_polys->polys;
			
		for(y = 0; y < texture_polys->surface->height; y++)
		{
			for(x = 0; x < texture_polys->surface->width; x++)
			{
				struct vertex_ll_t *cvertex = *vertex;
				
				if(cvertex)
				{
					float minx = cvertex->x;
					float miny = cvertex->y;
					float maxx = cvertex->x;
					float maxy = cvertex->y;
					
					cvertex = cvertex->next;
							
					while(cvertex)
					{
						if(cvertex->x < minx) minx = cvertex->x;
						if(cvertex->y < miny) miny = cvertex->y;
						if(cvertex->x > maxx) maxx = cvertex->x;
						if(cvertex->y > maxy) maxy = cvertex->y;
							
						cvertex = cvertex->next;
					}
			
			
					int box_minx = (int)(floorf(minx / TILEWIDTH));
					int box_miny = (int)(floorf(miny / TILEHEIGHT));
					int box_maxx = (int)(floorf(maxx / TILEWIDTH));
					int box_maxy = (int)(floorf(maxy / TILEHEIGHT));
		
					for(box_y = box_miny; box_y <= box_maxy; box_y++)
					{
						for(box_x = box_minx; box_x <= box_maxx; box_x++)
						{
							// check that there isn't already a block here
		
							struct tile_t *oldtile = intile(box_x * TILEWIDTH, box_y * TILEHEIGHT, 
								(box_x + 1) * TILEWIDTH, (box_y + 1) * TILEHEIGHT);
		
							if(oldtile)
							{
								if(add_fill_pointer(&oldtile->fillp0, fill))
								{
									free_surface(oldtile->surface);
									free_surface(oldtile->scaled_surface);
									oldtile->surface = NULL;
									oldtile->scaled_surface = NULL;
								}
							
								continue;
							}
		
							struct tile_t tile;
		
							tile.x1 = box_x * TILEWIDTH;
							tile.y1 = box_y * TILEHEIGHT;
							tile.x2 = (box_x + 1) * TILEWIDTH;
							tile.y2 = (box_y + 1) * TILEHEIGHT;
		
							tile.surface = NULL;
							tile.scaled_surface = NULL;
		
							tile.connp0 = NULL;
							tile.nodep0 = NULL;
							tile.fillp0 = NULL;
							
							add_fill_pointer(&tile.fillp0, fill);
							add_tile(&dirty_tile0, &tile);
						}
					}
				}

				vertex++;
			}
		}
		
		if(in_lock_check_stop_callback())
			return 0;
		
		texture_polys = texture_polys->next;
	}

	worker_leave_main_lock();

	return 1;
}


int tile_conn(struct conn_t *conn)
{
	if(!worker_try_enter_main_lock())
		return 0;

	struct curve_t *curve = get_curve(conn);
	assert(curve);

	assert(conn->squished_texture);
	assert(conn->squished_texture->width);
	if(!conn->squished_texture->height)
	{
		leave_main_lock();
		return 1;
	}


	int x, y, box_y, box_x;

	int width = conn->squished_texture->width + 1;
	
	for(y = 0; y != conn->squished_texture->height; y++)
	{
		for(x = 0; x != conn->squished_texture->width; x++)
		{
			struct vertex_t *vert = &conn->verts[y * width + x];

			float minx = vert[0].x;
			if(vert[1].x < minx) minx = vert[1].x;
			if(vert[width].x < minx) minx = vert[width].x;
			if(vert[width + 1].x < minx) minx = vert[width + 1].x;

			float miny = vert[0].y;
			if(vert[1].y < miny) miny = vert[1].y;
			if(vert[width].y < miny) miny = vert[width].y;
			if(vert[width + 1].y < minx) miny = vert[width + 1].y;

			float maxx = vert[0].x;
			if(vert[1].x < maxx) maxx = vert[1].x;
			if(vert[width].x < maxx) maxx = vert[width].x;
			if(vert[width + 1].x < maxx) maxx = vert[width + 1].x;

			float maxy = vert[0].y;
			if(vert[1].y < maxy) maxy = vert[1].y;
			if(vert[width].y < maxy) maxy = vert[width].y;
			if(vert[width + 1].y < maxx) maxy = vert[width + 1].y;

			int box_minx = (int)(floorf(minx / TILEWIDTH));
			int box_miny = (int)(floorf(miny / TILEHEIGHT));
			int box_maxx = (int)(floorf(maxx / TILEWIDTH));
			int box_maxy = (int)(floorf(maxy / TILEHEIGHT));

			for(box_y = box_miny; box_y <= box_maxy; box_y++)
			{
				for(box_x = box_minx; box_x <= box_maxx; box_x++)
				{
					// check that there isn't already a block here

					struct tile_t *oldtile = intile(box_x * TILEWIDTH, box_y * TILEHEIGHT, 
						(box_x + 1) * TILEWIDTH, (box_y + 1) * TILEHEIGHT);

					if(oldtile)
					{
						if(add_conn_pointer(&oldtile->connp0, conn))
						{
							free_surface(oldtile->surface);
							free_surface(oldtile->scaled_surface);
							oldtile->surface = NULL;
							oldtile->scaled_surface = NULL;
						}

						continue;
					}

					struct tile_t tile;

					tile.x1 = box_x * TILEWIDTH;
					tile.y1 = box_y * TILEHEIGHT;
					tile.x2 = (box_x + 1) * TILEWIDTH;
					tile.y2 = (box_y + 1) * TILEHEIGHT;

					tile.surface = NULL;
					tile.scaled_surface = NULL;

					tile.connp0 = NULL;
					tile.nodep0 = NULL;
					tile.fillp0 = NULL;
					
					add_conn_pointer(&tile.connp0, conn);
					add_tile(&dirty_tile0, &tile);
				}
			}
		}
		
		if(in_lock_check_stop_callback())
			return 0;
	}

	worker_leave_main_lock();

	return 1;
}


void tile()
{
	if(!tile_all_conns())
		return;
	
	if(!tile_all_node_textures())
		return;
	
	if(!tile_all_fills())
		return;
	
	collate_all_tiles();
}
