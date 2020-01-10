![banner](https://imgur.com/lfeylu5.jpg)

# Candle
A Game Engine (C89 ANSI, early development stages)

## External Dependencies
* no


## Getting Started

To learn how to use *Candle*, I will go through how to run a sample project. This documentation is incomplete, so if there are any questions, feel free to ask for help in the *issues*.

### Setup
Firstly, clone the sample project:

```git clone --recursive https://github.com/EvilPudding/twinpeaks```

Note the use of the flag ```--recursive```, although candle can be compiled as a dynamic library, this sample scene uses *git submodules* to include, compile and link candle statically. To update *Candle* to a newer version, just ```cd candle; git pull```

In ```twinpeaks/main.c```, we can see how entities are created.

### Compiling

On linux, if all dependencies are installed, run ```make```, additionally, you can run ```make run``` to compile and automatically run the result, or ```make gdb``` to compile a debug executable and run it through *gdb*.

On windows, it is recommended to have an *msys* environment with *mingw*, you can compile the project with ```make -f windows.mk```

On BSD systems, in order to be able to compile and run *Candle*, you have to install both its dependencies and GNU version of Make program, because the BSD version usually doesn't implement many required extensions. Therefore, instead of executing ```make``` you should use ```gmake``` on BSDs.

### Entities and Components

Candle loosely follows *Entity Component System* paradigm. Entities are a simple handle, which can be extended with the use of components.

```c
entity_t entity = entity_new({ /* component0, component1, ... */ });
```

For example, to create a cube object, the following code may be used:
```c
mat_t *material = mat_new("cube material");
mesh_t *mesh = mesh_new();
mesh_cube(mesh, 1, 1);
entity_t cube = entity_new({c_model_new(mesh, material, false, true)});
/* this creates a cube that does not cast shadows but is visible */
```

All components in *Candle* use ```c_``` prefix, and references to the component types use the ```ct_``` prefix.

As seen above, the entity requires a ```c_model``` component to specify which mesh and material to use. The ```c_model``` component depends on ```c_spatial``` component, which is used to determine an object's position, scale and rotation.
If I wish to change the cube's position, I can simply:
```c
c_spatial_t *sc = c_spatial(&cube); /* fetch the spatial component from the cube */
c_spatial_set_pos(sc, vec3(0, 1, 0)); /* move the cube upwards */
c_spatial_rotate_Y(sc, M_PI); /* rotate the cube around the Y axis by ~3.1415 radians */
```

If I wish to make cube the a light source, I can add to the entity a ```c_light``` component by:

```c
entity_add_component(cube, c_light_new(30.0f, vec4(1.0, 0.0, 0.0, 1.0)));
/* this creates a red light component of radius 30 */
```

This makes the cube emit light.

### Custom Components

Documentation on how to create custom components will be added in the repo's wiki, until then, to learn how to create custom components, look at the component directory in candle. Each component has it's own ```.h``` and ```.c``` file, the header specifies the structure and public methods, the source file implements the methods and registers listeners and signals for that component.


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
 * [mpv.candle](https://github.com/EvilPudding/mpv.candle) to be able to play videos

Each plugin may require specific external dependencies, and are added to a project through the use of *git submodules*, for example:

```git submodule add https://github.com/EvilPudding/assimp.candle```

The *Makefile* for the project will compile and link the plugin automatically.


## Contributing

Anyone is welcome to contribute, there are many features missing:
* Documentation is lacking.
* Compilation on windows is a work in progress.
* I don't use any modern engines, so feature suggestions are welcome.

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
