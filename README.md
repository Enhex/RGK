# RGKrt

A path tracer.

External libraries used:
 - assimp
 - png++
 - glm
 - libjpeg
 - ctpl (included with sources)
 - openEXR

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

    ./RGKrt ../obj/box6.rtc

## Command-line options

 - `RGKrt` expects a config file as a command-line argument. The
   config file specifies all render data, including model file, camera
   config, and render strategy. The format of the config file is
   described in a following section.
 - `-p` renders a preview. It has 3 times smaller dimentions and uses
   2 times less rays per pixel, which results in 18 times faster
   render. The preview is saved to a separate output file
   (`*.preview.exr`).
 - `-v` and `-q` respectively increase and decrease verbosity
   levels. The default level is 2. At level 0, the program will have
   no output unless a critical error happens. Increased verbosity
   levels output various statistcs and diagnostic information.
 - Unless debug information was disabled compile-time, `-d X Y`
   displays detailed information about how the `X Y` pixel was
   rendered, which is useful for debugging.

## Config file format

The config file is a simple text file. Traditionally the files are
named `*.rtc`, though any name will do. The first 9 lines of the file
specify obligatory options, and must follow the format described
below, any file shorter than 9 lines is invalid. The following lines
specify optional options, and may appear in any order, or not appear
at all.

 - `Line 1:` The first line of the file is always ignored, and may
   contain arbitrary text. It may serve as a comment or file
   description.
 - `Line 3: (string)` The path and file name (relative to working
   directory in which `RGKrt` is run) to where the output shall be
   written. Since the output is using OpenEXR format, it is reasonable
   to name the output file with `.exr` extension.
 - `Line 4: (int)` Specifies maximum recursion depth. Unless russian
   roulette is enabled with a later option, this value limits the
   length of ray paths used for rendering. Refractions and total
   reflections are not counted towards this limit. If russian roulette
   is enabled, this option is ignored.
 - `Line 2: (string)` This line must contain a path (relative to this
   `.rtc` file) to the 3d model file (although various file formats
   should work, only `.obj` models are oficially supported). This
   model is what will be rendered. The model shall use right-hand
   coordinate system.
 - `Line 5 (int int)` Sets the output image resolution (x,y). This
   also configures camera width-height ratio (pixel size ratio is
   fixed to 1:1).
 - `Line 6: (float float float)` Camera position coordinates (x,y,z).
 - `Line 7: (float float float)` Camera look-at coordinates
   (x,y,z). The camera will be oriented so that it looks towards this
   point. If this point is the same as camera position coordinates,
   the behavior is undefined.
 - `Line 8: (float float float)` Camera up-vector coordinates
   (x,y,z). This vector specifies the direction which has to be
   rendered as pointing up in the rendered image space. It
   effectivelly determines the camera roll. This vector does not have
   to be perpendicular to camera direction, but it may not be
   parallel. Usually using `0.0 1.0 0.0` is a good idea.
 - `Line 9: (float)` The vertical camera field of view, defined as the
   ratio of image height to focal distance. Using `1.0` results in
   vertical FOV of 60 degrees, `2.0` means 90 degrees. The horizontal
   FOV is calculated from output image dimentions ratio.

The lines that folow are optional. Unless stated otherwise, if an option
appears more than once, the later occurence overrites the setting of
the earlier. Empty lines and lines starting with `#` character are
ignored and may be used as comments. Each option follows the format
`name value(s)`. The list below describes all possible options by
their names. If a line in the `.rtc` config file refereces an
undefined option, it is ignored.

 - `multisample (int)` Specifies the number of paths to be traced per
   each pixel, in a single rendering round. **Default value is 1**
   (which makes little sense), so you probably want to increse this
   value by a significant amount. Increasing this value is the best
   way to decrease output noise, but the render time is proportional
   to the multisample level.
 - `rounds (int)` Configures the number of render rounds, default
   is 1. A single render round consists of sampling `multisample`
   paths per each pixel, the resulting output is the average of all
   rounds. Therefore the total number of paths per pixel is equal to
   `rounds * multisample`, and thus increasing rounds number has the
   same result as multisample level. However, the important difference
   is that the output file is saved after each round. Therefore if you
   wish to peek at the output file while it is being rendered, you
   will want to render it in more than 1 round. Similarly, if you use
   a large number of rounds and the render process is (intentionally
   or accidentally) interrupted, the partial progress is present in
   the output file.
 - `sky (int:R int:G int:B float:intensity)` Sets the sky color
   (`RGB`) and radiant `intensity` (in W/sr). Any ray that does not
   hit any object in the scene will be considered to reach the sky,
   and will have this color. By default the sky has no color
   (intensity 0), and therefore appears black. This setting is useful
   for open scenes, and may work as a very simple atmoshpere
   simulaton.
 - `L (float:x float:y float:z int:R int:G int:B float:intenity [float:size])`
   Adds an additional point light to the scene. Useful for rendering
   scenes and models that do not have lighting information. The light
   will be placed at `xyz` and will have `RGB` color, it will use
   radiant `intensity` (in W/sr). The `size` argument is optional, if
   present, it will set this light source's radius. Unlike other
   options, repeating this confuguration value inserts more lights.
 - `russian (float)` Enables russian roulette and sets its probability
   parameter. By default, russian roulette is disabled, and global
   recursion depth limit is used instead (line 4). Enabling it and
   setting the probability to *a* causes all paths to terminate at
   each of their point with probability *(1-a)*. Paths never terminate
   at a total reflection or refraction point. Therefore, for example,
   using `russian 0.75`, the mean length of paths will be 4, for
   `russian 0.9` the mean increases to 10. This parameter makes no
   sense outside 0-1 range, and thus other values are clamped.
 - `clamp (float)` Enables clamping and sets the maximum radiance
   transferred along paths. At each point if the radiance exceeds this
   configured value, it will be reduced appropriatelly. This is useful
   for removing butterflies from rendered output, which may happen
   when a very unprobable light path transfers unreasonably large
   radiance. By default clamping is disabled. Reasonable values for
   this option depend on the light levels in the current scene.
 - `brdf (string)` Sets the globally used BRDF model. Available
   options are:
  - `phong` Phong shading according to my lecture notes
  - `phong2` Phong shading according to Total Lighting Compedium
  - `phongenergy` Energy-conserving Phong model
  - `diffuse` Only cosine-weighted diffuse component
  - `cooktorr` Cook-Torrance reflection model (default)
  - `ltc_beckmann` LTC-approximated beckmann BRDF
  - `ltc_ggx` LTC-approximated beckmann GGX
 - `bumpscale (float)` Configures the scale for bump maps. Normally
   OBJ files contain no information about bump map depth, and thus it
   is subject to configuration. There is no defined correspondence
   between this value and normal vector skew, but using `0` disables
   bump maps, and using negative values inverts bump
   direction. Default value is `10`.
 - `lenssize (float)` Sets the camera lens radius. By default the
   camera uses infinitelly small aperture. This can be changed by
   setting `lenssize` to a non-zero value. In such case `focus` must
   be set as well. This creates depth-of-field effect, blurring
   objects that are very close to the camera or very far. The actual
   lens size depends on the scale of the scene and desired
   depth-of-field. Changing this value does not affect the amount of
   light that reaches the camera, i.e. does not make the rendered
   image brighter.
 - `focus (float)` The distance to the focus plane. This option has no
   default value, and must be specified if `lenssize` is non-zero
   (otherwise it has no effect). This value specifies the distance at
   which objects are in focus.
 - `thinglass (string)` Enables thinglass heuristics. The specified
   string is a phrase that will be looked for in material
   names. Unlike other options, when this one appears multiple times,
   all phrases will be stored. For each material, if its name contains
   at least one of these `thinglass` phrases, the material will be
   considered to be a thin glass. Such glass is not subject to
   collision nor refraction, all rays pass through if as if it was
   empty space. However, all such rays that pass through thin glass
   are color filtered with the diffuse (because OBJ files have no
   cnsistent way of expressing transmission filter) color of the thin
   glass material. This creates stained glass effects with virtually
   no effect on render performance. See box3.rtc for example scene.
 - `force_fresnell (int)` 1 to enable, 0 to disable, disabled by
   default.  Forces calculation of fresnell reflections on all
   materials, regardless of their specular component.
 - `reverse (int)` Specifies the maximum length of reverse path (light
   path) for bi-directional tracing. Reflections and refractions are
   not counted towards this limit. Default is 0 (disabled).

Example config file:

    # An example file.
    box3/box3.obj
    box3.exr
    4
    1200 900
    7.0 -1.0 4.0
    0.0 0.0 0.0
    0.0 1.0 0.0
    1.2
    L 0.0 0.0 -2.0 255 255 255 5.0 0.4
    L 0.0 0.0 -3.5 255 200 190 4.0 0.2

    multisample 40
    rounds 20
    # Total paths per pixel: 800
    sky 195 230 245 1.0
    clamp 5.0
    russian 0.8
    thinglass glass
    lens_size 0.0095
    focus_plane 3.0
    brdf cooktorr
