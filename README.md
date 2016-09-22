# RGKrt

A photorealistic physically-based renderer.

![Example render](http://i.imgur.com/HGONqUs.jpg)

More examples are showcased [here](http://cielak.org/phile/software/rgkrt).

External libraries used:
 - assimp
 - png++
 - glm
 - libjpeg
 - openEXR
 - ctpl (included with sources)
 - jsoncpp (included with sources)
 - stbi (HDR input only) (included with sources)

## Building

Create out-of-tree build dir:

    mkdir build && cd build

Prepare build files:

    cmake ..

Compile:

    make

## Running examples

There are various example scenes provided in the obj directory. To
render one of them, pass the `.rtc` config file as the command-line
argument, e.g.:

    ./RGKrt ../scenes/cornell-box.rtc

## Command-line options

 - `RGKrt` expects a config file as a command-line argument. The
   config file specifies all render data, including model file, camera
   config, and render strategy. The format of the config file is
   described in the following section.
 - `-p` renders a preview. It has 4 times smaller dimentions and uses
   2 times less samples per pixel, which results in 48 times faster
   render. The preview is saved to a separate output file
   (`*.preview.exr`).
 - `-t MINUTES`, `--timed MINUTES` enabled timed render mode. The
   rendering will take at least the specified number of minutes, and
   then will stop as soon as current render round finishes.
 - `-D DIR` overrides the output file directory.
 - `-v` and `-q` respectively increase and decrease verbosity
   levels. The default level is 2. At level 0, the program will have
   no output unless a critical error happens. Increased verbosity
   levels output various statistcs and diagnostic information.
 - `-r` renders a collection of 250 (currently unconfigurable) images
   where each has camera placed differently, resulting images form an
   animation of camera getting rotated around the lookat point. This
   feature will be furtner expanded into proper animation
   support. This option is particularly useful when coupled with
   `--no-override` when rendering on multiple machines that share
   filesystem, renderer instances will exclusively pick frames to
   share workload.
 - `-s FLOAT` sets a predetermined exposure scaling factor. This is
   useful when comparing brightness of multiple renders, or when
   rendering an animation.
 - Unless debug information was disabled compile-time, `-d X Y`
   displays detailed information about how the `X Y` pixel was
   rendered, which is useful for debugging.

## Config file format

The config file is a [JSON](http://http://json.org/) formatted plain
text file. Traditionally the files are named `*.json`, though any name
will do. Some values in the file are required, and some are optional
and use a predefined default value. If a required value is missing,
the program will fail to start, and will notify you about the missing
value. If you use a value that is not specified in the file format,
the program will run, but will warn you about the unused configuration
value (as it is likely a typo or reduncancy).

Contrary to what JSON format specification says, comments (lines
starting with `//` or blocks within `/*` and `*/`) are properly
recognized and ignored, so you can use them to verbosely describe and
document your input file.

The root of the config file is an Object, with following keys:

#### Output settings

 - `output-file`, *string*, REQUIRED - The name (or path) of the ouput
   file where the result will be stored. Since the output files use
   OpenEXR format, it is recommended to use `*.exr` files as output.
 - `output-height`, *int*, REQUIRED - The desired vertical output
   resolution.
 - `output-width`, *int*, REQUIRED - The desired horizontal output
     resolution.
 - `output-scale`, *float, or string: "auto"*, optional, default:
   "auto" - This sets the exposure scaling factor for the entire
   output. When set to auto, the output is uniformly scaled so that
   the brightest pixel of the image is set to 1.0. This option allows
   one to set a custom scaling factor.

#### Rendering parameters

 - `multisample`, *int*, optional, default: 1 - Number of paths per
   pixel to sample. Naturally, the rendering time scales
   proportionally to this number, but low values will result in a
   noisy image. The correct value for this key depends in the light
   distribution of a particular scene.
 - `recursion-max`, *int*, optional, default: 40 - Limits the maximum
   path length to the specified value. When set to 1, the program will
   behave as a ray tracer.
 - `russian`, *float*, optional, default: 0.75 - The russian roulette
   path continuation probability. At each path point, the path is
   terminated with this probability, and the light transmitted by the
   path is divided by this probability. This helps to shorten the
   paths (improving render times), without introducing bias to the
   integration. When set to 1, all paths will be always
   `recursion-max` steps long. When set to a negative value, russian
   roulette is disabled.
 - `rounds`, *int*, optional, default: 1 - A single run of the
   renderer will repeat the rendering process (sampling `multisample`
   paths per pixel) this many times, and average the result. After
   each round, the current progress is saved to the output file. This
   is useful when you wish to preview the output while it is being
   rendered, or if you expect to interrupt the renderer before it
   finishes.
 - `render-time`, *int*, optional - If this option is set, the
   renderer will keep repeating the process infinitely, and after the
   specified time (in minutes) had elapsed, it will stop after the next
   rendering round will finish. This option is an alterntive to
   `rounds` and they must not appear together.
 - `reverse`, *int*, optional, default: 0 - When 0, the renderer will
   work as a path tracer. When greater, it will behave as a
   bi-directional path tracer, this value sets the fixed light path
   length.
 - `clamp`, *float*, optional, default: +inf - At each path point, the
   transferred radiance is clamped to this value. This is only useful
   for removing 'butterfly' artefacts, which may appear if some paths
   may reach a very bright light source with very small probability.

#### Camera settings

 - `camera`, *Object*, REQUIRED - Camera configuration is specified in
   a subobject. The contents of the object shall be as listed:
  - `position`, *Array of 3 floats*, REQUIRED - The XYZ coordinates of
    camera position.
  - `lookat`, *Array of 3 floats*, REQUIRED - The XYZ coordinates of
    camera 'look at' point. The camera will be oriented so that this
    point is mapped to the center of output image.
  - `upvector`, *Array of 3 floats*, optional, default: [0, 1, 0] -
    This vector will be used as the 'up' direction of the camera, so
    that the top of output image will be oriented towards this vector.
  - `fov`, *float*, either this or `focal` is REQUIRED - The
    horizontal field of view for the camera, expressed as an angle in
    degrees. Values over 180 are not supported. Generaly, reasonable
    values are between 30 and 110.
  - `focal`, *float*, eother this or `fov` is REQUIRED - The
    *vertical* camera field of view, defined as the ratio of image
    height to focal distance.
  - `lens-size`, *float*, default: 0 - Camera lens size, in scene
    units. If 0, the camera will behave as a pinhole, positive values
    enable a depth-of-field effect. The greated the lens size, the
    shallower focus, but also more samples required to render a
    noise-free image.
  - `focus plane`, *float*, REQUIRED if `lens-size` non-zero - The
    distance from camera (in scene units) at which object should be
    fully focused, if camera is not a pinhole.

#### Scene description

There are two ways to provide a scene for rendering:

 - `model-file`, *string*, either this or `scene` is REQUIRED - The
   path to a single scene file in `.obj` + `.mtl` format.
 - `scene`, *Array of Objects*, either this or `model-file` is
   REQUIRED - A list of scene elements. Each element of the scene is
   defined by an object, which shall follow this template:
   - `file`, *string*, either this or `primitive` is REQUIRED - The
     path to the `.obj` file containing the mesh for this scene
     element.
   - `primitive`, *string*, either this or `file` is REQUIRED - Name
     of a built-in primitive mesh to use for this scene
     element. Available primitives are: `tri`, a right-angled
     triangle, `plane`, a flat quad, and `cube`, a cube out of 12
     triangles.
   - `import-materials`, *bool*, optional, default: false - This
     option is available only when using `file`. When enabled,
     materials are imported from the `.mtl` file referenced by
     the loaded `.obj`.
   - `override-materials`, *bool*, optional, default: false - This
     option is only available when `import-materials` is set to
     true. When enabled, materials imported from the `.mtl` file will
     override these manually specified in the config file, should they
     share the name.
   - `brdf`, *string*, optional, default: "ltc_ggx" - This option is
     only available when `import-materials` is set to true. It selects
     the BRDF to use for materials imported with the `.obj` scene
     file. The list of available brdfs is provided in a further
     section.
   - `axis`, *string, either X, Y or Z*, optional, default: "Y" - Only
     relavant for `plane` primitive. Sets the axis along which this
     plane is oriented, the nnormal vector of the quad will be
     alligned to the selected axis.
   - `material`, *string*, REQUIRED only if using `primitive` - Sets
     the material (by its name) for this scene element. If this
     element is a `primitive`, this option is required. If it is
     loaded from a `file`, selecting this option selects the material
     to use for all meshes from the file. Otherwise the meshes loaded
     from file will use material names as specified by the `.obj` file
     (`usemtl` directive), regardless of whether actual material
     properties are imported from `.mtl` or not.
   - `scale`, *Array of 3 floats*, default: [1,1,1] - Defines a
     scaling transformation to be applied for this entire scene
     element. Scaling is the first transformation performed.
   - `rotate`, *Array of 3 floats*, default: [0,0,0] - Defines a
     rotating transformation to be applied for this entire scene
     element. Rotation is applied after scaling, and before
     translation.  Rotation angles correspond to rotations along X, Y
     and Z axes, in this order, and must be specified in degrees. The
     rotation is first performed around Z axis, then Y, and then X.
   - `translate`, *Array of 3 floats*, default: [0,0,0] - Defines a
     translation transformation to be applied for this entire scene
     element. Translation is the last transformation applied.
   - `texture-scake`, *Array of 3 floats*, defailt: [1,1,1] - This
     option is only available when this element is a
     `primitive`. Specifies a scaling transformation to be applied to
     UV texture coordinates.

#### Global material config

 - `bumpscale`, *float*, optional, default: 1 - Global scaling factor
   for all bumpmaps.
 - `force-fresnell`, *bool*, optional, default: false - Enabling this
   option applies Fresnell reflection to all materials in the scene.
 - `thinglass`, *an Array of strings*, optional - Lists the material
   names that should be ignored when searching for intersection. This
   is a heuristic hack to improve convergence when rendering scenes
   lit by rays comming through windows with glass.
 - `brdf`, *string*, optional, default: "diffuse" - The file-global
   default reflection fuction that should be used when no other is
   specified (e.g. when importing materials from an .obj scene file).
   Available values are listed in the **BRDF** section below.

#### Material definitions

 - `materials`, *Array of Objects*, optional - specifies the list of
   materials used for this scene. This list may be empty, e.g. when
   importing materials from a scene file. The contents of each Object
   represent a single material, and must follow this description:
  - `name`, *string*, REQUIRED - Sets a name for this material, which
    can be later used to reference it (e.g. by a scene mesh).
  - `brdf`, *string*, optional, default: "ltc_ggx" - The BRDF to use
    for this material. The list of available BRDFs is in a further
    section. Most BRDFs use only some of material properties.
  - `diffuse`, *Array of 3 floats*, optional, default: [0,0,0] - Sets
    the RGB diffuse component for this material. The values may range
    from 0 to 1, use `diffuse255` to use 0-255 ranges.
  - `specular`, *Array of 3 floats*, optional, default: [0,0,0] - Sets
    the RGB specular component for this material. The values may range
    from 0 to 1, use `specular255` to use 0-255 ranges.
  - `emission`, *Array of 3 floats*, optional, default: [0,0,0] - Sets
    the RGB light emission for this material. The values are of
    arbitary range, as they represent radiance. However, you can use
    `emission255` to have these values scaled down by the factor of
    255.
  - `diffuse-texture`, *string*, optional - Path to the texture file
    (PNG, JPG, or HDR format) that will be used for the diffuse
    component of this material.
  - `specular-texture`, *string*, optional - Path to the texture file
    (PNG, JPG, or HDR format) that will be used for the diffuse
    component of this material.
  - `bump-map`, *string*, optional - Path to the texture file (PNG,
    JPG, or HDR format) that will be used as a bump-map for this
    material.
  - `ior`, *float*, optional, default: 1 - The index of refraction for
    this material.
  - `translucency`, *float*, optional: 0 - Fraction of light that is
    reflected or absorbed by this material. The rest passes
    through. Thus materials with translucency 1 are fully transparent,
    and materials with translucency 1 are fully opaque.
  - `exponent`, *float*, optional, default: 50 - The exponent
    coefficient for phong shading model. It is also used for
    cook-torrance specular reflection, and LTC roughness
    parameter. Generally, small values are good at imitating rough
    materials, and large values imitate smooth shiny surfaces. When
    mapped to roughness, exponent 0 translates to rougness 1, and as
    exponent goes to infinity roughness reaches zero.

#### Available BRDFs

  - `diffuse` Only cosine-weighted diffuse component
  - `cooktorr` Cook-Torrance reflection model
  - `phong` Phong shading according to my lecture notes
  - `phong2` Phong shading according to Total Lighting Compedium
  - `phongenergy` Energy-conserving Phong model
  - `ltc_beckmann` LTC-approximated Beckmann BRDF
  - `ltc_ggx` LTC-approximated GGX BRDF

#### Light desciption

To add lights to your scene, you can use a non-zero emission material
on a mesh, which creates an areal light. You can also specify
additional pointlights:

 - `lights`, *Array of Objects*, optional - A list of additional
   lights. Each objects represents a single light source, and must
   follow this specification:
   - `position`, *Array of 3 floats*, REQUIRED - The XYZ coordinates
     of this pointlight.
   - `color`, *Array of 3 floats*, optional, default: [1,1,1] - RGB
     color of this light source. You can use `color255` to have the
     values scaled down by the factor of 255.
   - `intensity`, *float*, REQUIRED - Irradiance of this light
     source. This is effectively a scaling factor for `color`.
   - `size`, *float*, optional, default: 0.0f - The radius of this
     spherical light source (in scene units). If 0, this becomes a
     point light. This setting is useful for creating soft shadows, at
     the cost of some extra output variance.

Another way of adding light to the scene is to have the sky emit some
light:

#### Sky configuration

 - `sky`, *Object* - Skybox configuration. If this key is not present,
   the sky will be black and will emit no light. Sky is configured as
   a subobject:
  - `color`, *Array of 3 floats*, either this or `envmap` is
    REQUIRED. - Specifies RGB sky color. The same color will be used
    for all directions, not just upper hemishpere. To have these
    values scaled down by 222, use `color255`.
  - `envmap`, *string*, either this or `color` is REQUIRED - Path to
    the texture file with evironmental map to use for sky. The top
    edge of the texture will be mapped to the Y=1 direction (world
    top). The bottom edge of the texture will be mapped to Y=-1
    direction (world bottom). The middle row of the image will be
    wrapped around the horizon. It is recommended to use HDR textures
    for envmaps, as they work best as an environmental light source.
  - `intensity`, *float*, optional, default: 1 - Lighting intensity
    for the skybox.
  - `rotate`, *float*, optional, default: 0 - This option is only
    available when using `envmap`. The entire sky will be rotated by
    the specified angle (in degrees) around Y axis, which simplifies
    getting the skybox oriented in the right direction.

### Config file example

The following example is here just to provide the general idea of how
the config file may look like. For more interesting examples, see `./scenes/`.

	{
	    "materials": [{
	        "name": "sp_00_pod",
	        "specular255": [209, 197, 181],
	        "exponent": 400.0,
	        "diffuse-texture": "sponza-fixed/KAMEN.JPG",
	        "bump-map": "sponza-fixed/KAMEN-bump.jpg"
	    }],
	    "scene": [{
	        "file": "sponza-fixed/sponza.obj",
	        "import-materials": true
	    }],
	    "output-file": "sponza4c.exr",
	    "output-width": 1280,
	    "output-height": 900,
	    "camera": {
	        "position": [-11.3, 7.2, -1.9],
	        "lookat": [3.0, 7.5, 6.2],
	        "fov": 85
	    },
	    "lights": [{ // Direct sun lighting
	        "position": [-5.0, 35.0, -15.0],
	        "color": [1.0, 0.85, 0.6],
	        "intensity": 2000.0,
	        "size": 0.7
	    }],
	    "multisample": 90,
	    "rounds": 40,
	    "sky": {
	        "envmap": "envmap/cloudy1.hdr",
	        "intensity": 0.9
	    },
	    "bumpscale": 10,
	    "clamp": 5.0,
	    "russian": 0.6,
	    "brdf": "ltc_ggx"
	}

## License

This software is published under the terms of the zlib license, see `./LICENSE` for details.
