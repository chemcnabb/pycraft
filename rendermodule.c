#include <Python.h>
#include <math.h>

#define bool int
#define true 1
#define false 0

#include "colours.c"


typedef struct
{
    wchar_t character;
    wchar_t character_left;
    wchar_t character_right;

    struct
    {
        Colour fg, bg;
        int style;
    } colours;
    bool solid;
} BlockData;

static long world_gen_height = 10;

char
get_block(long x, long y, PyObject *map)
{
    char result = 0;
    // try:
    //     return map[x][y]
    // except (KeyError, IndexError):
    //     pass

    return result;
}


BlockData *
get_block_data(char block_key)
{
    return (BlockData *)0;
}


bool
is_solid(char block_key)
{
    bool result = true;
    return result;
}


float
lightness(Colour *rgb)
{
    return 0.2126 * rgb->r + 0.7152 * rgb->g + 0.0722 * rgb->b;
}


float
circle_dist(long test_x, long test_y, long x, long y, long r)
{
    return ( pow(test_x - x, 2) / pow(r  , 2) +
             pow(test_y - y, 2) / pow(r/2, 2) );
}


int
light_mask(long x, long y, PyObject *map, PyObject *slice_heights)
{
    PyObject *px = PyLong_FromLong(x);
    long slice_height = PyLong_AsLong(PyDict_GetItem(slice_heights, px));
    return (get_block_data(get_block(x, y, map))->solid || (world_gen_height - y) < slice_height) ? 0 : -1;
}


float
lit(long x, long y, PyObject *pixel)
{
    PyObject *px = PyDict_GetItemString(pixel, "x"),
             *py = PyDict_GetItemString(pixel, "y"),
             *radius = PyDict_GetItemString(pixel, "radius");

    return fmin(circle_dist(x, y, PyLong_AsLong(px), PyLong_AsLong(py), PyLong_AsLong(radius)), 1);
}


void
get_block_lights(long x, long y, PyObject *lights, bool *bitmap)
{
    // Get all lights which affect this pixel
    long i = 0;
    PyObject *iter = PyObject_GetIter(lights);
    PyObject *pixel;

    while ((pixel = PyIter_Next(iter)))
    {
        bitmap[i] = lit(x, y, pixel) < 1;
        ++i;
    }
}


float
get_block_lightness(long x, long y, long world_x, PyObject *map, PyObject *slice_heights, PyObject *lights)
{
    bool bitmap[PyList_Size(lights)];
    get_block_lights(x, y, lights, bitmap);

    float min = 1;
    int i = 0;
    PyObject *iter = PyObject_GetIter(lights);
    PyObject *pixel;

    // If the light is not hidden by the mask
    while ((pixel = PyIter_Next(iter)))
    {
        long px = PyLong_AsLong(PyDict_GetItemString(pixel, "x"));
        long py = PyLong_AsLong(PyDict_GetItemString(pixel, "y"));
        long z = PyLong_AsLong(PyDict_GetItemString(pixel, "z"));
        PyObject *colour = PyDict_GetItemString(pixel, "colour");

        Colour rgb = {
            .r = PyFloat_AsDouble(PyTuple_GetItem(colour, 0)),
            .g = PyFloat_AsDouble(PyTuple_GetItem(colour, 1)),
            .b = PyFloat_AsDouble(PyTuple_GetItem(colour, 2))
        };

        bitmap[i] = bitmap[i] || z >= light_mask(world_x + px, py, map, slice_heights);

        float block_lightness = lit(x, y, pixel) * lightness(&rgb);
        if (bitmap[i] && block_lightness < min)
            min = block_lightness;

        ++i;
    }

    return 1 - min;
}


void
get_block_light(long x, long y, long world_x, PyObject *map, PyObject *slice_heights,
                PyObject *lights, float day, Colour *block_colour, bool fancy_lights)
{
    if (fancy_lights)
    {
        float block_lightness = get_block_lightness(x, y, world_x, map, slice_heights, lights);
        float d_ground_height = PyFloat_AsDouble(PyDict_GetItem(slice_heights, PyLong_FromLong(world_x+x))) - (world_gen_height - y);
        float v = lerp(day, fmin(1, fmax(0, d_ground_height / 3)), 0);

        Colour *hsv;
        rgb_to_hsv(block_colour, hsv);
        hsv->v = lerp(0, fmax(v, block_lightness), hsv->v);
        hsv_to_rgb(hsv, block_colour);
    }
}


void
sky(long x, long y, long world_x, PyObject *map, PyObject *slice_heights, PyObject *bk_objects, Colour *sky_colour, PyObject *lights, bool fancy_lights)
{
}


wchar_t
get_char(long x, long y, PyObject *map, BlockData *pixel)
{
    char left_block_key = get_block(x-1, y, map);
    char right_block_key = get_block(x+1, y, map);
    char below_block_key = get_block(x, y+1, map);

    wchar_t character = pixel->character;

    if (below_block_key == 0 || !is_solid(below_block_key))
    {
        if (left_block_key != 0 && is_solid(left_block_key) && pixel->character_left != 0)
        {
            character = pixel->character_left;
        }
        else if (right_block_key != 0 && is_solid(right_block_key) && pixel->character_right != 0)
        {
            character = pixel->character_right;
        }
    }

    return character;
}


typedef struct
{
    wchar_t character;
    Colour fg, bg;
    int style;
} PrintableChar;

bool
printable_char_eq(PrintableChar *a, PrintableChar *b)
{
    return (a->character == b->character &&
            colour_eq(&(a->fg), &(b->fg)) &&
            colour_eq(&(a->bg), &(b->bg)) &&
            a->style == b->style);
}


void
get_obj_pixel(long x, long y, PyObject *objects, wchar_t *obj_key_result, Colour *obj_colour_result)
{
}


void
calc_pixel(long x, long y, long world_x, long world_y, long world_screen_x,
           PyObject *map, PyObject *slice_heights, char pixel_f_key, PyObject *objects, PyObject *bk_objects,
           Colour *sky_colour, float day, PyObject *lights, bool fancy_lights, PrintableChar *result)
{
    result->bg.r = -1;
    result->fg.r = -1;

    // If the front block has a bg
    BlockData *pixel_f = get_block_data(pixel_f_key);
    if (pixel_f->colours.bg.r >= 0)
    {
        get_block_light(x, world_y, world_screen_x, map, slice_heights, lights, day, &(pixel_f->colours.bg), fancy_lights);
        result->bg = pixel_f->colours.bg;
    }
    else
    {
        sky(x, world_y, world_screen_x, map, slice_heights, bk_objects, sky_colour, lights, fancy_lights);
        result->bg = *sky_colour;
    }

    // Get any object
    wchar_t obj_key = 0;
    Colour obj_colour;
    get_obj_pixel(x, world_y, objects, &obj_key, &obj_colour);

    if (obj_key != 0)
    {
        result->character = obj_key;
        result->fg = obj_colour;
    }
    else
    {
        result->character = get_char(world_x, world_y, map, pixel_f);

        if (pixel_f->colours.fg.r >= 0)
        {
            get_block_light(x, world_y, world_screen_x, map, slice_heights, lights, day, &(pixel_f->colours.fg), fancy_lights);
            result->fg = pixel_f->colours.fg;
        }
    }

    result->style = pixel_f->colours.style;
}


typedef struct
{
    char *buffer;
    size_t size;
    size_t cur_pos;
} ScreenBuffer;


#define POS_STR_FORMAT "\033[%ld;%ldH%s"
#define POS_STR_FORMAT_MAX_LEN sizeof(POS_STR_FORMAT)
size_t
pos_str(long x, long y, char *s, char *result)
{
    char format[] = POS_STR_FORMAT;
    return sprintf(result, format, y+1, x+1, s);
}


static PrintableChar *last_frame;
static ScreenBuffer frame;
static long width;
static long height;

int
terminal_out(PrintableChar *c, long x, long y, long width)
{
    size_t frame_pos = y * width + x;
    if (!printable_char_eq(last_frame + frame_pos, c))
    {
        last_frame[frame_pos] = *c;

        char *pixel = colour_str(c->character, c->bg, c->fg, c->style);

        size_t added = pos_str(x, y, pixel, frame.buffer + frame.cur_pos);
        frame.cur_pos += added;
        if (frame.cur_pos >= frame.size)
        {
            printf("Error: Exceeded frame buffer size in terminal_out!\n");
            return -1;
        }
    }

    return 0;
}


int
neopixels_out(PrintableChar *printable_char)
{
    // neopixels.set_pixel(leds, width, height, x, y, fg or bg)
    return 0;
}


int
setup_frame(long new_width, long new_height)
{
    bool resize = false;
    if (new_width != width)
    {
        resize = true;
        width = new_width;
    }
    if (new_height != height)
    {
        resize = true;
        height = new_height;
    }

    if (resize)
    {
        frame.size = width * height * POS_STR_FORMAT_MAX_LEN;
        frame.buffer = (char *)malloc(frame.size);
        if (!frame.buffer)
        {
            printf("Error: Could not allocate frame buffer!\n");
            return -1;
        }
        last_frame = (PrintableChar *)malloc(width * height * sizeof(PrintableChar));
    }

    frame.cur_pos = 0;
    return 0;
}


typedef struct
{
    bool terminal_output;
    bool neopixels_output;
} Settings;

static Settings settings = {true, false};

static PyObject *
render_render(PyObject *self, PyObject *args)
{
    PyObject *map;
    long left_edge,
         right_edge,
         top_edge,
         bottom_edge;
    if (!PyArg_ParseTuple(args, "O(ll)(ll):render", &map,
            &left_edge, &right_edge, &top_edge, &bottom_edge))
        return NULL;

    long cur_width = right_edge - left_edge;
    long cur_height = bottom_edge - top_edge;
    setup_frame(cur_width, cur_height);

    if (!PyDict_Check(map))
        return NULL;

    Py_ssize_t i = 0;
    PyObject *world_x, *column;

    while (PyDict_Next(map, &i, &world_x, &column))
    {
        if (!PyList_Check(column))
        {
            printf("column is not a list\n");
            return NULL;
        }

        long world_x_l = PyLong_AsLong(world_x);
        if (!(world_x_l >= left_edge && world_x_l < right_edge))
            continue;

        long x = world_x_l - left_edge;

        PyObject *iter = PyObject_GetIter(column);
        PyObject *pixel;
        long world_y_l = 0;

        while ((pixel = PyIter_Next(iter)))
        {
            if (world_y_l >= top_edge && world_y_l < bottom_edge)
            {
                long y = world_y_l - top_edge;

                PrintableChar printable_char;
                // calc_pixel(x, y, world_x_l, world_y_l, left_edge,
                //     map, slice_heights, pixel, objects, bk_objects,
                //     sky_colour, day, lights, fancy_lights, &printable_char);

                if (settings.terminal_output)
                {
                    if (!terminal_out(&printable_char, x, y, width))
                        return NULL;
                }

                if (settings.neopixels_output)
                {
                    if (!neopixels_out(&printable_char))
                        return NULL;
                }
            }

            ++world_y_l;
            Py_XDECREF(pixel);
        }

        strcpy("\n", frame.buffer + frame.cur_pos);
        frame.cur_pos += 1;
        Py_XDECREF(iter);
    }

    fwrite(frame.buffer, frame.cur_pos, 1, stdout);

    Py_RETURN_NONE;
}


static PyMethodDef render_methods[] = {
    {"render", render_render, METH_VARARGS, PyDoc_STR("render(map) -> None")},
    {NULL, NULL}  /* sentinel */
};

PyDoc_STRVAR(module_doc, "The super-duper-fast renderer");

static struct PyModuleDef rendermodule = {
    PyModuleDef_HEAD_INIT,
    "render",
    module_doc,
    -1,
    render_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC
PyInit_render(void)
{
    PyObject *m = NULL;

    /* Create the module and add the functions */
    m = PyModule_Create(&rendermodule);
    if (m == NULL)
        goto fail;

    return m;
 fail:
    Py_XDECREF(m);
    return NULL;
}