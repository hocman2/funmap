# An OpenStreetMap 3D visualizer using Raylib
Fetches data from OSM, uses tinyXML to parse them, uses an Earcut triangulation algorithm to create 3D meshes from map data, renders.

This is a toy project, it doesn't cover all edge cases and the quality of rendering highly depends on the set location.
The **F** key does the following in repeated order:

1. Generate the initial chunk
2. Generate 8 surrounding chunks
3. Destroys all chunks

These three steps serve to test various aspect of the program.
The first step aims to test chunk generation
The second step aims to test asynchronous chunk generation
The third step aims to test if objects are deallocated correctly

Warning: is unstable ugly and buggy
