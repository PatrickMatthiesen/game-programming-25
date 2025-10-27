# Notes for Game Programming 25

## Pull from remote

```sh
git pull gameprog main
```


## Pull submodules

```sh
git submodule update --init --recursive
```


## Week 3

### Kenny tiny dungeon packed atlas

- 12 columns
- 11 rows

#### Tile map structure

roof_red=0, roof_red_left_corner=1, roof_red_edge=2, roof_red_right_corner=3, roof_back_edge1=4, roof_back_edge2=5, pillar_top=6, pillar_body=7, brick_wall=pipe_off=8, brick_wall_pipe_on=9, brick_wall_small_opening=10, brick_wall_large_opening_left_11, brick_wall_large_opening_right=12, roof_red_textured=13, red_roof_edge_right=14, brick_wall_textured=15, red_roof_edge_left=16, ...

### fun things to add next

- A way to select multiple tiles at once when editing the tile map
- A way to save/load the tile map (i.e. to/from a file)
- A way to detach the camera from the player and move it around with the keyboard when in edit mode
- A way to switch between edit mode and play mode (e.g. pressing 'E' to toggle edit mode)

