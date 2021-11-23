#pragma once

namespace wp
{

struct HashGrid
{
    float cell_width;
    float cell_width_inv;

    int* point_cells;   // cell id of a point
    int* point_ids;     // index to original point
    
    int* cell_starts;   // start index of a range of indices belonging to a cell, dim_x*dim_y*dim_z in length
    int* cell_ends;     // end index of a range of indices belonging to a cell, dim_x*dim_y*dim_z in length

    int dim_x;
    int dim_y;
    int dim_z;

    int num_points;
    int max_points;
};

// convert a virtual (world) cell coordinate to a physical one
CUDA_CALLABLE inline int hash_grid_index(const HashGrid& grid, int x, int y, int z)
{
    // offset to ensure positive coordinates
    const int origin = 1<<24;

    x += origin;
    y += origin;
    z += origin;

    assert(x >= 0);
    assert(y >= 0);
    assert(z >= 0);

    // compute physical cell
    int cx = x & (grid.dim_x-1);
    int cy = y & (grid.dim_y-1);
    int cz = z & (grid.dim_z-1);

    return cz*(grid.dim_x*grid.dim_y) + cy*grid.dim_x + cx;
}

CUDA_CALLABLE inline int hash_grid_index(const HashGrid& grid, const vec3& p)
{
    return hash_grid_index(grid, 
                           int(p.x*grid.cell_width_inv), 
                           int(p.y*grid.cell_width_inv),
                           int(p.z*grid.cell_width_inv));
}

// stores state required to traverse neighboring cells of a point
struct hash_grid_query_t
{
    CUDA_CALLABLE hash_grid_query_t() {}
    CUDA_CALLABLE hash_grid_query_t(int) {} // for backward pass

    int x_start;
    int y_start;
    int z_start;

    int x_end;
    int y_end;
    int z_end;

    int x;
    int y;
    int z;

    int cell;
    int cell_index;     // offset in the current cell (index into cell_indices)
    int cell_end;       // index following the end of this cell 
    
    HashGrid grid;
};

CUDA_CALLABLE inline hash_grid_query_t hash_grid_query(uint64_t id, wp::vec3 pos, float radius)
{
    hash_grid_query_t query;

    query.grid = *(const HashGrid*)(id);

    // convert coordinate to grid
    query.x_start = int((pos.x-radius)*query.grid.cell_width_inv);
    query.y_start = int((pos.y-radius)*query.grid.cell_width_inv);
    query.z_start = int((pos.z-radius)*query.grid.cell_width_inv);

    // do not want to visit any cells more than once, so limit large radius offset to one pass over each dimension
    query.x_end = min(int((pos.x+radius)*query.grid.cell_width_inv), query.x_start + query.grid.dim_x-1);
    query.y_end = min(int((pos.y+radius)*query.grid.cell_width_inv), query.y_start + query.grid.dim_y-1);
    query.z_end = min(int((pos.z+radius)*query.grid.cell_width_inv), query.z_start + query.grid.dim_z-1);

    query.x = query.x_start;
    query.y = query.y_start;
    query.z = query.z_start;

    const int cell = hash_grid_index(query.grid, query.x, query.y, query.z);
    query.cell_index = query.grid.cell_starts[cell];
    query.cell_end = query.grid.cell_ends[cell];

    return query;
}


CUDA_CALLABLE inline bool hash_grid_query_next(hash_grid_query_t& query, int& index)
{
    const HashGrid& grid = query.grid;

    while (1)
    {
        if (query.cell_index < query.cell_end)
        {
            // write output index
            index = grid.point_ids[query.cell_index++];            
            return true;
        }
        else
        {
            query.x++;
            if (query.x > query.x_end)
            {
                query.x = query.x_start;
                query.y++;
            }

            if (query.y > query.y_end)
            {
                query.y = query.y_start;
                query.z++;
            }

            if (query.z > query.z_end)
            {
                // finished lookup grid
                return false;
            }

            // update cell pointers
            const int cell = hash_grid_index(grid, query.x, query.y, query.z);

            query.cell_index = grid.cell_starts[cell];
            query.cell_end = grid.cell_ends[cell];        
        }
    }
}

CUDA_CALLABLE inline int hash_grid_point_id(uint64_t id, int& index)
{
    const HashGrid* grid = (const HashGrid*)(id);
    return grid->point_ids[index];
}

CUDA_CALLABLE inline void adj_hash_grid_query(uint64_t id, wp::vec3 pos, float radius, uint64_t& adj_id, wp::vec3& adj_pos, float& adj_radius, hash_grid_query_t& adj_res) {}
CUDA_CALLABLE inline void adj_hash_grid_query_next(hash_grid_query_t& query, int& index, hash_grid_query_t& adj_query, int& adj_index, bool& adj_res) {}
CUDA_CALLABLE inline void adj_hash_grid_point_id(uint64_t id, int& index, uint64_t & adj_id, int& adj_index, int& adj_res) {}


} // namespace wp
