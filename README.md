# candle
A C Game Engine (early development stages)

## Dependencies
* GLEW
* SDL2


## Getting Started

To learn how to use *Candle*, I will go through how to run a sample project. This documentation is incomplete, so if there are any questions, feel free to ask for help in the *issues*.

### Setup
Firstly, clone the sample project:

```git clone --recursive https://github.com/EvilPudding/twinpeaks```

Note the use of the flag ```--recursive```, although candle can be compiled as a dynamic library, this sample scene uses *git submodules* to include, compile and link candle statically. To update *Candle* to a newer version, just ```cd candle; git pull```

In ```twinpeaks/main.c```, we can see how entities are created.

### Compiling

On linux, if all dependencies are installed, run ```make```, additionally, you can run ```make run``` to compile and automatically run the result, or ```make gdb``` to compile a debug executable and run it through *gdb*.

On windows, it is recommended to have an *msys* environment with *mingw*, after installing *glew* and *SDL2*, you can compile the project with ```make -f windows.mk```

### Entities and Components

Candle loosely follows *Entity Component System* paradigm. Entities are a simple handle, which can be extended with the use of components.

```c
entity_t entity = entity_new( /* component0, component1, ... */ );
```

For example, to create a cube object, the following code may be used:
```c
mat_t *material = mat_new("cube material");
mesh_t *mesh = mesh_new();
mesh_cube(mesh, 1, 1);
entity_t cube = entity_new(c_model_new(mesh, material, 0, 1));
/* this creates a cube that does not cast shadows but is visible */
```

All components in *Candle* use ```c_``` prefix.

As seen above, the entity requires a ```c_model``` component to specify which mesh and material to use. The ```c_model``` component depends on ```c_spacial``` component, which is used to determine an object's position, scale and rotation.
If I wish to change the cube's position, I can simply:
```c
c_spacial_t *sc = c_spacial(&cube); /* fetch the spacial component from the cube */
c_spacial_set_pos(sc, vec3(0, 1, 0)); /* move the cube upwards */
c_spacial_rotate_Y(sc, M_PI); /* rotate the cube around the Y axis by ~3.1415 radians */
```

If I wish to make cube the a light source, I can add to the entity a ```c_light``` component by:

```c
entity_add_component(cube, c_light_new(30.0f, vec4(1.0, 0.0, 0.0, 1.0), 512));
/* this creates a red light component of radius 30 */
/* 512 is the resolution of the cubemap shadow */
```

This makes the cube emit light.

### Custom Components

Documentation on how to create custom components will be added in the repo's wiki, until then, to learn how to create custom components, view the component directory in candle. Each component has it's own ```.h``` and ```.c``` file, the header specifies the structure and public methods, the source file implements the methods and registers listeners and signals for that component.


### Custom Rendering

Further explanations on how to create custom render passes will be added in the wiki, until then, to learn how to create custom render passes, skim through the function ```renderer_default_pipeline``` in *candle/utils/renderer.c*, when I have the time, I will share example projects to demonstrate this functionality.

### Resources

Anything included in the directory ```resauces``` is copied into the build directory. To index a directory for texture, mesh, sound and material lookup, run:

```c
c_sauces_index_dir(c_sauces(&SYS), "name_of_directory");
```
after this, a texture that is within that directory can be accessed by:

```c
texture_t *texture = sauces("name_of_texture.png");
```


## Plugins
Features that aren't requirements for all projects, are split into plugins, the ones I've developed so far are the following:
 * [assimp.candle](https://github.com/EvilPudding/assimp.candle) for loading several file formats
 * [openal.candle](https://github.com/EvilPudding/openal.candle) for sounds
 * [bullet.candle](https://github.com/EvilPudding/bullet.candle) for physics
 * [filepicker.candle](https://github.com/EvilPudding/filepicker.candle) for a file picker dialog
 * [steam.candle](https://github.com/EvilPudding/steam.candle) for steam integration (currently only fetches nickname and profile picture)

Each plugin may require specific dependencies, and are added to a project through the use of *git submodules*, for example:

```git submodule clone --recursive https://github.com/EvilPudding/assimp.candle```

The *Makefile* for the project will compile and link the plugin automatically.


## Contributing

Anyone is welcome to contribute, there are many features missing:
* Documentation is lacking.
* Compilation on windows is a bit strange, and could be improved.
* I don't use any modern engines, so feature suggestions are welcome.
* The shaders don't run on AMD cards, and the card gives no debugging information, could be fixed empirically, but I do not have access to an AMD card.

## Screenshots
![Screenshot](https://i.imgur.com/E2Qxp4Q.png)
![Screenshot](https://i.imgur.com/9BInObF.jpg)
![Screenshot](https://i.imgur.com/X8JEI8x.png)
![Screenshot](https://i.imgur.com/UfvwsHN.png)
![Screenshot](https://i.imgur.com/jF5aFB7.png)
![Screenshot](https://i.imgur.com/OEQ3a6q.png)
![Screenshot](https://i.imgur.com/vcQJOib.png)
![Screenshot](https://i.imgur.com/TphwzIF.png)
![Screenshot](https://i.imgur.com/VFJsegd.png)
