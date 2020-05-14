#include <stdarg.h>
#include <stdio.h>

#include <stdbool.h>
#include <assert.h>

#include <SDL.h>

#include <windows.h>

static unsigned int frame = 0; //FIXME: Remove
static SDL_GameController* g = NULL;

#if 0
#include <hal/debug.h>

void debugPrintFloat(float f) {
#if 0
  //FIXME: pdclib can't do this
  debugPrint("%f", f);
#endif
#if 0
  debugPrint("0x%08X", *(uint32_t*)&f);
#endif
#if 1
  bool sign = f < 0.0f;
  if (sign) { f = -f; }
  unsigned int value = (unsigned int)(f * 10000);
  unsigned int mantissa = value % 10000;
  value /= 10000;
  debugPrint("%s%u.%04u", sign ? "-" : "", value, mantissa);
#endif
}


static void _unimplemented(const char* fmt, ...) {
  char buf[1024];
  va_list va;
  va_start(va, fmt);
  vsprintf(buf, fmt, va);
  va_end(va);
  debugPrint(buf);
}
#define unimplemented(fmt, ...) _unimplemented("%s (%s:%d) " fmt "\n", __FUNCTION__, __FILE__, __LINE__, __VA_ARGS__)

#else
void debugPrintFloat(float f) {}
#define debugPrintFloat(f) 0
#define debugPrint(...) 0
#define unimplemented(fmt, ...) 0
#endif

// stdlib.h
#include <stdlib.h>

int _putenv(const char *envstring) {
  assert(false); //FIXME: Implement
  return 0;
}

// direct.h
#include "direct.h"

int _mkdir(const char *dirname) {
  debugPrint("Making dir '%s'\n", dirname);
  CreateDirectoryA(dirname, NULL); //FIXME: Handle return value
  return 0;
}

// dirent.h
#include "dirent.h"

#if 0
DIR *opendir(const char *__name) {
  assert(false); //FIXME: Implement
  return NULL;
}

struct dirent *readdir(DIR *__dirp) {
  assert(false); //FIXME: Implement
  return NULL;
}

int closedir(DIR *__dirp) {
  assert(false); //FIXME: Implement
  return 0;
}
#endif


// sys/stat.h
#include "sys/stat.h"

#include <string.h>

int stat(const char *pathname, struct stat *statbuf) {
  debugPrint("stat file '%s'\n", pathname);
  memset(statbuf, 0x00, sizeof(*statbuf));

  int ret = -1;
  FILE* f = fopen(pathname, "rb");
  if (f != NULL) {
    ret = 0;
    fseek(f, 0, SEEK_END);
    statbuf->st_size = ftell(f);
    fclose(f);
  }

  return ret;
}

int access(const char *pathname, int mode) {
  assert(mode == F_OK);
  int ret = -1;
  FILE* f = fopen(pathname, "rb");
  if (f != NULL) {
    ret = 0;
    fclose(f);
  }
  debugPrint("access file '%s': %d\n", pathname, ret);
  return ret;
}

// sys/time.h
#include "sys/time.h"

int gettimeofday(struct timeval *tv, struct timezone *tz) {
  assert(false); //FIXME: Implement
  return 0;
}

// xgu.h
#include "xgu/xgu.h"
#include "xgu/xgux.h"


// GLES/gl.h
#include <stdint.h>
#include <string.h>
#include <pbkit/pbkit.h>

#include "GLES/gl.h"
//#include <xgu/xgu.h>


static void* AllocateResourceMemory(size_t size) {
#define MAXRAM 0x03FFAFFF
  return MmAllocateContiguousMemoryEx(size, 0, MAXRAM, 0, PAGE_READWRITE | PAGE_WRITECOMBINE);
}

static void FreeResourceMemory(void* ptr) {
  MmFreeContiguousMemory(ptr);
}

typedef struct {
  void* data;
} Object;

typedef struct {
  uint8_t* data;
  size_t size;
} Buffer;
#define DEFAULT_BUFFER() \
  { \
    .data = NULL, \
    .size = 0 \
  }

typedef struct {
  GLsizei width;
  GLsizei height;
  size_t pitch;
  void* data;
  GLint min_filter;
  GLint mag_filter;
  GLint wrap_s;
  GLint wrap_t;
  GLenum internal_base_format;
} Texture;

typedef struct {
  GLenum env_mode;
  GLfloat env_color[4];
  GLenum combine_rgb;
  GLenum combine_alpha;
  GLenum src_rgb[3];
  GLenum src_operand_rgb[3];
  GLenum src_alpha[3];
  GLenum src_operand_alpha[3];
} TexEnv;

//FIXME: Allow different pools
static Object* objects = NULL;
static GLuint object_count = 0;

static GLuint unused_object() {
  //FIXME: Find object slot where data is NULL
  object_count += 1;
  objects = realloc(objects, sizeof(Object) * object_count);
  return object_count - 1;
}

static void gen_objects(GLsizei n, GLuint* indices, const void* data, size_t size) {
  for(int i = 0; i < n; i++) {
    GLuint index = unused_object();

    Object* object = &objects[index];
    object->data = malloc(size);
    memcpy(object->data, data, size);

    indices[i] = 1 + index;
  }
}

static void del_objects(GLsizei n, const GLuint* indices) {
  for(int i = 0; i < n; i++) {

    //FIXME: Also ignore non-existing names
    if (indices[i] == 0) {
      continue;
    }

    GLuint index = indices[i] - 1;

    Object* object = &objects[index];

    //FIXME: Call a handler routine?

    assert(object->data != NULL);
    free(object->data);
    object->data = NULL;
  }
}

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define MASK(mask, val) (((val) << (__builtin_ffs(mask)-1)) & (mask))

typedef struct {
  struct {
    bool enabled;
    GLenum gl_type;
    unsigned int size;
    size_t stride;
    const void* data;
  } array;
  float value[4];
} Attrib;

#define DEFAULT_TEXENV() \
  { \
    .env_mode = GL_MODULATE, \
    .src_rgb = { GL_TEXTURE, GL_PREVIOUS, GL_CONSTANT }, \
    .src_alpha = { GL_TEXTURE, GL_PREVIOUS, GL_CONSTANT }, \
    .src_operand_rgb = { GL_SRC_COLOR, GL_SRC_COLOR, GL_SRC_ALPHA}, \
    .src_operand_alpha = { GL_SRC_ALPHA, GL_SRC_ALPHA, GL_SRC_ALPHA }, \
    .env_color = { 0.0f, 0.0f, 0.0f, 0.0f }, \
    /*.scale_rgb = 1.0f,*/ \
    /*.scale_alpha = 1.0f*/ \
  }

static unsigned int active_texture = 0;
static unsigned int client_active_texture = 0;

TexEnv texenvs[4] = {
  DEFAULT_TEXENV(),
  DEFAULT_TEXENV(),
  DEFAULT_TEXENV(),
  DEFAULT_TEXENV()
};

typedef struct {
  bool enabled;
  //FIXME: Store other attributes?
} Light;

typedef struct {
  Attrib texture_coord_array[4];
  Attrib vertex_array;
  Attrib color_array;
  Attrib normal_array;
  bool texture_2d[4]; //FIXME: Move into a texture unit array
  GLuint texture_binding_2d[4];
  Light light[3]; //FIXME: no more needed in neverball
} State;
static State state = {
  .texture_2d = { false, false, false, false },
  .texture_binding_2d = { 0, 0, 0, 0 }
};

#include "matrix.h"

static uint32_t* set_enabled(uint32_t* p, GLenum cap, bool enabled) {
  switch(cap) {
  case GL_ALPHA_TEST:
    p = xgu_set_alpha_test_enable(p, enabled);
    break;
  case GL_NORMALIZE:
    //FIXME: Needs more changes to matrices?
    p = xgu_set_normalization_enable(p, enabled);
    break;
  case GL_CULL_FACE:
    p = xgu_set_cull_face_enable(p, enabled);
    break;
  case GL_DEPTH_TEST:
    p = xgu_set_depth_test_enable(p, enabled);
    break;
  case GL_TEXTURE_2D:
    state.texture_2d[active_texture] = enabled;
    break;
  case GL_BLEND:
    p = xgu_set_blend_enable(p, enabled);
    break;
  case GL_LIGHT0:
  case GL_LIGHT1:
  case GL_LIGHT2:
    state.light[cap - GL_LIGHT0].enabled = enabled;
    break;
  case GL_LIGHTING:
    p = xgu_set_lighting_enable(p, enabled);
    break;
  case GL_COLOR_MATERIAL:
    unimplemented(); //FIXME: !!!
    break;
  case GL_POLYGON_OFFSET_FILL:
    unimplemented(); //FIXME: !!!
    break;   
  case GL_POINT_SPRITE: 
    unimplemented(); //FIXME: !!!
    break;   
  default:
    unimplemented("%d", cap);
    assert(false);
    break;
  }
  return p;
}

static void set_client_state_enabled(GLenum array, bool enabled) {
  switch(array) {
  case GL_TEXTURE_COORD_ARRAY:
    state.texture_coord_array[client_active_texture].array.enabled = enabled;
    break;
  case GL_VERTEX_ARRAY:
    state.vertex_array.array.enabled = enabled;
    break;
  case GL_COLOR_ARRAY:
    state.color_array.array.enabled = enabled;
    break;
  case GL_NORMAL_ARRAY:
    state.normal_array.array.enabled = enabled;
    break;
  default:
    unimplemented("%d", array);
    assert(false);
    break;
  }
}

static XguPrimitiveType gl_to_xgu_primitive_type(GLenum mode) {
  switch(mode) {
  case GL_POINTS:         return XGU_POINTS;
  case GL_TRIANGLES:      return XGU_TRIANGLES;
  case GL_TRIANGLE_STRIP: return XGU_TRIANGLE_STRIP;
  default:
    unimplemented("%d", mode);
    assert(false);
    return -1;
  }
}

typedef unsigned int XguTextureFilter;
#define XGU_TEXTURE_FILTER_LINEAR 4 //FIXME: Shitty workaround for XGU
#define XGU_TEXTURE_FILTER_NEAREST XGU_TEXTURE_FILTER_LINEAR //FIXME: Shitty workaround for XGU

static XguTextureFilter gl_to_xgu_texture_filter(GLenum filter) {
  switch(filter) {
  case GL_LINEAR:  return XGU_TEXTURE_FILTER_LINEAR;
  case GL_NEAREST: return XGU_TEXTURE_FILTER_NEAREST;
  default:
    unimplemented("%d", filter);
    assert(false);
    return -1;
  }
}


static XguStencilOp gl_to_xgu_stencil_op(GLenum op) {
  switch(op) {
  default:
    unimplemented("%d", op);
    assert(false);
    return -1;
  }
}

static XguFuncType gl_to_xgu_func_type(GLenum func) {
  switch(func) {
  case GL_LESS: return XGU_FUNC_LESS;
  default:
    unimplemented("%d", func);
    assert(false);
    return -1;
  }
}

static XguCullFace gl_to_xgu_cull_face(GLenum mode) {
  switch(mode) {
  case GL_FRONT:          return XGU_CULL_FRONT;
  case GL_BACK:           return XGU_CULL_BACK;
  case GL_FRONT_AND_BACK: return XGU_CULL_FRONT_AND_BACK;
  default:
    unimplemented("%d", mode);
    assert(false);
    return -1;
  }
}

static XguFrontFace gl_to_xgu_front_face(GLenum mode) {
  switch(mode) {
  //FIXME: Why do I have to invert this?!
  case GL_CW:  return XGU_FRONT_CCW;
  case GL_CCW: return XGU_FRONT_CW;
  default:
    unimplemented("%d", mode);
    assert(false);
    return -1;
  }
}

static XguVertexArrayType gl_to_xgu_vertex_array_type(GLenum mode) {
  switch(mode) {
  case GL_FLOAT:         return XGU_FLOAT;
  case GL_SHORT:         return NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S32K; //FIXME: NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S1; for normals?
  case GL_UNSIGNED_BYTE: return NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_OGL; //FIXME XGU_U8_XYZW ?
  default:
    unimplemented("%d", mode);
    assert(false);
    return -1;
  }
}

static XguBlendFactor gl_to_xgu_blend_factor(GLenum factor) {
  switch(factor) {
  case GL_ONE:                 return XGU_FACTOR_ONE;
  case GL_SRC_ALPHA:           return XGU_FACTOR_SRC_ALPHA;
  case GL_ONE_MINUS_SRC_ALPHA: return XGU_FACTOR_ONE_MINUS_SRC_ALPHA;
  default:
    unimplemented("%d", factor);
    assert(false);
    return -1;
  }
}

static uint8_t f_to_u8(float f) {
  assert(f >= 0.0f && f <= 1.0f);
  return f * 0xFF;
}

// 3.7.10 ES 1.1 FULL
#define DEFAULT_TEXTURE() \
  { \
    /* FIXME: Set up other fields */ \
    .width = 0, \
    .height = 0, \
    .pitch = 0, \
    .data = NULL, \
    .min_filter = GL_NEAREST_MIPMAP_LINEAR, \
    .mag_filter = GL_LINEAR, \
    .wrap_s = GL_REPEAT, \
    .wrap_t = GL_REPEAT, \
    .internal_base_format = 1, \
    /* .generate_mipmaps = false */ \
  }

static Texture zero_texture = DEFAULT_TEXTURE();

static Texture* get_bound_texture(GLuint stage) {
  GLuint texture = state.texture_binding_2d[stage];

  if (texture == 0) {
    return &zero_texture;
  }

  Texture* object = objects[texture - 1].data;
  assert(object != NULL);

  return object;
}

#define _RC_ZERO 0x0
#define _RC_DISCARD 0x0
#define _RC_PRIMARY_COLOR 0x4
#define _RC_TEXTURE 0x8
#define _RC_SPARE0 0xC

#define _RC_UNSIGNED 0x0
#define _RC_UNSIGNED_INVERT 0x1


static uint32_t* setup_texenv_output(uint32_t* p,
                                     TexEnv* t, unsigned int texture, unsigned int stage, bool rgb,
                                     bool ab, bool cd, bool sum) {
  p = pb_push1(p, NV097_SET_COMBINER_COLOR_OCW + stage * 4,
    MASK(NV097_SET_COMBINER_COLOR_OCW_AB_DST, ab ? _RC_SPARE0 : _RC_DISCARD)
    | MASK(NV097_SET_COMBINER_COLOR_OCW_CD_DST, cd ? _RC_SPARE0 : _RC_DISCARD)
    | MASK(NV097_SET_COMBINER_COLOR_OCW_SUM_DST, sum ? _RC_SPARE0 : _RC_DISCARD)
    | MASK(NV097_SET_COMBINER_COLOR_OCW_MUX_ENABLE, 0)
    | MASK(NV097_SET_COMBINER_COLOR_OCW_AB_DOT_ENABLE, 0)
    | MASK(NV097_SET_COMBINER_COLOR_OCW_CD_DOT_ENABLE, 0)
    | MASK(NV097_SET_COMBINER_COLOR_OCW_OP, NV097_SET_COMBINER_COLOR_OCW_OP_NOSHIFT));
  return p;
}

static unsigned int gl_to_texenv_src(TexEnv* t, unsigned int texture, bool rgb, int arg) {
  GLenum src = rgb ? t->src_rgb[arg] : t->src_alpha[arg];

  //FIXME: Move to function?
  switch(src) {
  case GL_PREVIOUS:
    if (texture == 0) {
      return _RC_PRIMARY_COLOR;
    }
    return _RC_SPARE0;
  case GL_PRIMARY_COLOR: return _RC_PRIMARY_COLOR; //FIXME: Name?
  case GL_TEXTURE:       return _RC_TEXTURE+texture; //FIXME: Get stage
  default:
    unimplemented("%d", src);
    assert(false);
    return gl_to_texenv_src(t, texture, rgb, -1);
  }
}

static unsigned int is_texenv_src_alpha(TexEnv* t, unsigned int texture, bool rgb, int arg) {
  GLenum operand = rgb ? t->src_operand_rgb[arg] : t->src_operand_alpha[arg];

  // Operand also checked for validity in `gl_to_texenv_src_map`
  return operand == GL_SOURCE_ALPHA || operand == GL_ONE_MINUS_SRC_ALPHA;
}

static bool is_texenv_src_inverted(TexEnv* t,  int texture, bool rgb, int arg) {
  bool invert;
  GLenum operand = rgb ? t->src_operand_rgb[arg] : t->src_operand_alpha[arg];

  if (rgb) {
    assert(operand == GL_SRC_COLOR ||
           operand == GL_SRC_ALPHA ||
           operand == GL_ONE_MINUS_SRC_COLOR ||
           operand == GL_ONE_MINUS_SRC_ALPHA);
    invert = (operand == GL_ONE_MINUS_SRC_COLOR || operand == GL_ONE_MINUS_SRC_ALPHA);
  } else {
    assert(operand == GL_SRC_ALPHA ||
           operand == GL_ONE_MINUS_SRC_ALPHA);
    invert = (operand == GL_ONE_MINUS_SRC_ALPHA);
  }

  return invert;
}

static uint32_t* setup_texenv_src(uint32_t* p,
                                  TexEnv* t, unsigned int texture, unsigned int stage, bool rgb,
                                  unsigned int x_a, bool a_rgb, bool a_invert,
                                  unsigned int x_b, bool b_rgb, bool b_invert,
                                  unsigned int x_c, bool c_rgb, bool c_invert,
                                  unsigned int x_d, bool d_rgb, bool d_invert) {
  if (rgb) {
    p = pb_push1(p, NV097_SET_COMBINER_COLOR_ICW + stage * 4,
          MASK(NV097_SET_COMBINER_COLOR_ICW_A_SOURCE, x_a) | MASK(NV097_SET_COMBINER_COLOR_ICW_A_ALPHA, a_rgb ? 0 : 1) | MASK(NV097_SET_COMBINER_COLOR_ICW_A_MAP, a_invert ? _RC_UNSIGNED_INVERT : _RC_UNSIGNED)
        | MASK(NV097_SET_COMBINER_COLOR_ICW_B_SOURCE, x_b) | MASK(NV097_SET_COMBINER_COLOR_ICW_B_ALPHA, b_rgb ? 0 : 1) | MASK(NV097_SET_COMBINER_COLOR_ICW_B_MAP, b_invert ? _RC_UNSIGNED_INVERT : _RC_UNSIGNED)
        | MASK(NV097_SET_COMBINER_COLOR_ICW_C_SOURCE, x_c) | MASK(NV097_SET_COMBINER_COLOR_ICW_C_ALPHA, c_rgb ? 0 : 1) | MASK(NV097_SET_COMBINER_COLOR_ICW_C_MAP, c_invert ? _RC_UNSIGNED_INVERT : _RC_UNSIGNED)
        | MASK(NV097_SET_COMBINER_COLOR_ICW_D_SOURCE, x_d) | MASK(NV097_SET_COMBINER_COLOR_ICW_D_ALPHA, d_rgb ? 0 : 1) | MASK(NV097_SET_COMBINER_COLOR_ICW_D_MAP, d_invert ? _RC_UNSIGNED_INVERT : _RC_UNSIGNED));
  } else {
    p = pb_push1(p, NV097_SET_COMBINER_ALPHA_ICW + stage * 4,
          MASK(NV097_SET_COMBINER_ALPHA_ICW_A_SOURCE, x_a) | MASK(NV097_SET_COMBINER_ALPHA_ICW_A_ALPHA, a_rgb ? 0 : 1) | MASK(NV097_SET_COMBINER_ALPHA_ICW_A_MAP, a_invert ? _RC_UNSIGNED_INVERT : _RC_UNSIGNED)
        | MASK(NV097_SET_COMBINER_ALPHA_ICW_B_SOURCE, x_b) | MASK(NV097_SET_COMBINER_ALPHA_ICW_B_ALPHA, b_rgb ? 0 : 1) | MASK(NV097_SET_COMBINER_ALPHA_ICW_B_MAP, b_invert ? _RC_UNSIGNED_INVERT : _RC_UNSIGNED)
        | MASK(NV097_SET_COMBINER_ALPHA_ICW_C_SOURCE, x_c) | MASK(NV097_SET_COMBINER_ALPHA_ICW_C_ALPHA, c_rgb ? 0 : 1) | MASK(NV097_SET_COMBINER_ALPHA_ICW_C_MAP, c_invert ? _RC_UNSIGNED_INVERT : _RC_UNSIGNED)
        | MASK(NV097_SET_COMBINER_ALPHA_ICW_D_SOURCE, x_d) | MASK(NV097_SET_COMBINER_ALPHA_ICW_D_ALPHA, d_rgb ? 0 : 1) | MASK(NV097_SET_COMBINER_ALPHA_ICW_D_MAP, d_invert ? _RC_UNSIGNED_INVERT : _RC_UNSIGNED));
  }
  return p;
}

static void setup_texenv() {

  uint32_t* p = pb_begin();

  //FIXME: This is an assumption on how this should be handled
  //       The GL ES 1.1 Full spec claism that only texture 0 uses Cp=Cf.
  //       However, what if unit 0 is inactive? Cp=Cf? Cp=undefined?
  //       Couldn't find an answer when skimming over spec.
  GLuint rc_previous = _RC_PRIMARY_COLOR;

#define _Cf _RC_PRIMARY_COLOR, true, false
#define _Af _RC_PRIMARY_COLOR, false, false
#define _Cs _RC_TEXTURE+texture, true, false
#define _As _RC_TEXTURE+texture, false, false
#define _Cp rc_previous, true, false
#define _Ap rc_previous, false, false
#define _Zero _RC_ZERO, true, false //FIXME: make dependent on portion (RGB/ALPHA)?
#define _One _RC_ZERO, true, true //FIXME: make dependent on portion (RGB/ALPHA)?

  // This function is meant to create a register combiner from texenvs
  unsigned int stage = 0;
  for(int texture = 0; texture < 4; texture++) {

    // Skip stage if texture is disabled
    if (!state.texture_2d[texture]) {
      continue;
    }

    Texture* tx = get_bound_texture(texture);
    TexEnv* t = &texenvs[texture];

    switch(t->env_mode) {
    case GL_REPLACE:
      switch(tx->internal_base_format) {
      case GL_ALPHA:
        unimplemented();
        assert(false);
        break;   
      case GL_LUMINANCE: case 1:
        unimplemented();
        assert(false);
        break;        
      case GL_LUMINANCE_ALPHA: case 2:
        unimplemented();
        assert(false);
        break;        
      case GL_RGB: case 3:
        unimplemented();
        assert(false);
        break;
      case GL_RGBA: case 4:
        unimplemented();
        assert(false);
        break;
      default:
        unimplemented("%d\n", tx->internal_base_format);
        assert(false);
        break;
      }
      break;
    case GL_MODULATE:
      switch(tx->internal_base_format) {

      case GL_ALPHA:
        unimplemented();
        assert(false);
        break;   

      case GL_LUMINANCE: case 1:     
      case GL_RGB: case 3:
      
        // Cv = Cp * Cs
        // Av = Ap
        p = setup_texenv_output(p, t, texture, stage, true, true, false, false);
        p = setup_texenv_src(p, t, texture, stage, true,
                             _Cp, _Cs,
                             _Zero, _Zero);
        p = setup_texenv_output(p, t, texture, stage, false, true, false, false);
        p = setup_texenv_src(p, t, texture, stage, false,
                             _Ap, _One,
                             _Zero, _Zero);
        break;

      case GL_LUMINANCE_ALPHA: case 2:
      case GL_RGBA: case 4:

        // Cv = Cp * Cs
        // Av = Ap * As
        p = setup_texenv_output(p, t, texture, stage, true, true, false, false);
        p = setup_texenv_src(p, t, texture, stage, true,
                             _Cp, _Cs,
                             _Zero, _Zero);
        p = setup_texenv_output(p, t, texture, stage, false, true, false, false);
        p = setup_texenv_src(p, t, texture, stage, false,
                             _Ap, _As,
                             _Zero, _Zero);
        break;

      default:
        unimplemented("%d\n", tx->internal_base_format);
        assert(false);
        break;
      }
      break;
    case GL_COMBINE: {

      bool rgb;

#define _Arg(i) \
  gl_to_texenv_src(t, texture, rgb, (i)), \
  is_texenv_src_alpha(t, texture, rgb, (i)), \
  is_texenv_src_inverted(t, texture, rgb, (i))

#define _OneMinusArg(i) \
  gl_to_texenv_src(t, texture, rgb, (i)), \
  is_texenv_src_alpha(t, texture, rgb, (i)), \
  !is_texenv_src_inverted(t, texture, rgb, (i))

      rgb = true;
      switch(t->combine_rgb) {
      case GL_MODULATE:
        //FIXME: Can potentially merge into previous stage
        // Setup `spare0 = a * b`
        p = setup_texenv_output(p, t, texture, stage, rgb, true, false, false);
        p = setup_texenv_src(p, t, texture, stage, rgb,
                             _Arg(0), _Arg(1),
                             _Zero, _Zero);
        break;
      case GL_INTERPOLATE:
        // Setup `spare0 = a * b + c * d`
        p = setup_texenv_output(p, t, texture, stage, rgb, false, false, true);
        p = setup_texenv_src(p, t, texture, stage, rgb,
                             _Arg(0), _Arg(2),
                             _Arg(1), _OneMinusArg(2));
        break;     
      default:
        assert(false);
        break;     
      }

      rgb = false;
      switch(t->combine_alpha) {
      case GL_REPLACE:
        //FIXME: Can potentially merge into previous stage
        // Setup `spare0 = a`
        p = setup_texenv_output(p, t, texture, stage, rgb, true, false, false);
        p = setup_texenv_src(p, t, texture, stage, rgb,
                             _Arg(0), _One,
                             _Zero, _Zero);
        break;      
      default:
        assert(false);
        break;      
      }

      break;
    }
    default:
      assert(false);
      break;
    }

    rc_previous = _RC_SPARE0;
    stage++;
  }

  // Add at least 1 dummy stage (spare0 = fragment color)
  if (stage == 0) {
    p = setup_texenv_output(p, NULL, 0, stage, true, true, false, false);
    p = setup_texenv_src(p, NULL, 0, stage, true,
                         _Cf, _One,
                         _Zero, _Zero);
    p = setup_texenv_output(p, NULL, 0, stage, false, true, false, false);
    p = setup_texenv_src(p, NULL, 0, stage, false,
                         _Af, _One,
                         _Zero, _Zero);
    stage++;
  }

  // Set up final combiner
  //FIXME: This could merge an earlier stage into final_product
  //FIXME: Ensure this is `out.rgb = spare0.rgb; out.a = spare0.a;`
  p = pb_push1(p, NV097_SET_COMBINER_CONTROL,
        MASK(NV097_SET_COMBINER_CONTROL_FACTOR0, NV097_SET_COMBINER_CONTROL_FACTOR0_SAME_FACTOR_ALL)
      | MASK(NV097_SET_COMBINER_CONTROL_FACTOR1, NV097_SET_COMBINER_CONTROL_FACTOR1_SAME_FACTOR_ALL)
      | MASK(NV097_SET_COMBINER_CONTROL_ITERATION_COUNT, stage));
  p = pb_push1(p, NV097_SET_COMBINER_SPECULAR_FOG_CW0,
        MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_SOURCE, _RC_ZERO)   | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_ALPHA, 0) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_INVERSE, 0)
      | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_SOURCE, _RC_ZERO)   | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_ALPHA, 0) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_INVERSE, 0)
      | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_SOURCE, _RC_ZERO)   | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_ALPHA, 0) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_INVERSE, 0)
      | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_SOURCE, _RC_SPARE0) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_ALPHA, 0) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_INVERSE, 0));
  p = pb_push1(p, NV097_SET_COMBINER_SPECULAR_FOG_CW1,
        MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_SOURCE, _RC_ZERO)   | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_ALPHA, 0) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_INVERSE, 0)
      | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_SOURCE, _RC_ZERO)   | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_ALPHA, 0) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_INVERSE, 0)
      | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_SOURCE, _RC_SPARE0) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_ALPHA, 1) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_INVERSE, 0)
      | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_SPECULAR_CLAMP, 0));

  pb_end(p);
}


static GLuint gl_array_buffer = 0;
static GLuint gl_element_array_buffer = 0;

static GLuint* get_bound_buffer_store(GLenum target) {
  switch(target) {
  case GL_ARRAY_BUFFER:
    return &gl_array_buffer;
  case GL_ELEMENT_ARRAY_BUFFER:
    return &gl_element_array_buffer;
  default:
    unimplemented("%d", target);
    assert(false);
    break;
  }
  return NULL;  
}

#define DEFAULT_MATRIX() \
  { \
        1.0f, 0.0f, 0.0f, 0.0f, \
        0.0f, 1.0f, 0.0f, 0.0f, \
        0.0f, 0.0f, 1.0f, 0.0f, \
        0.0f, 0.0f, 0.0f, 1.0f \
  }

static float matrix_mv[16 * 4*4] = DEFAULT_MATRIX();
static unsigned int matrix_mv_slot = 0;
static float matrix_p[16 * 4*4] = DEFAULT_MATRIX();
static unsigned int matrix_p_slot = 0;
static float matrix_t[4][16 * 4*4] = {
  DEFAULT_MATRIX(),
  DEFAULT_MATRIX(),
  DEFAULT_MATRIX(),
  DEFAULT_MATRIX()
};
static unsigned int matrix_t_slot[4] = { 0, 0, 0, 0 };

//FIXME: This code assumes that matrix_mv_slot is 0
static unsigned int* matrix_slot = &matrix_mv_slot;
static float* matrix = &matrix_mv[0];
static GLenum matrix_mode = GL_MODELVIEW;

typedef struct {
  bool enabled;
  float x;
  float y;
  float z;
  float w;
} ClipPlane;
static ClipPlane clip_planes[3]; //FIXME: No more needed for neverball


static void print_attrib(XguVertexArray array, Attrib* attrib, unsigned int start, unsigned int count, bool submit) {
  unsigned int end = start + count;
  if (!attrib->array.enabled) {
    debugPrint("\narray %d disabled\n", array, attrib->array.stride, attrib->array.data);
    return;
  }
  if (attrib->array.gl_type == GL_SHORT) {
    debugPrint("\narray %d as GL_SHORT, stride %d at %p\n", array, attrib->array.stride, attrib->array.data);
    for(int i = start; i < end; i++) {
      int16_t* v = i * attrib->array.stride + (uintptr_t)attrib->array.data;
      debugPrint("[%d]:", i);
      for(int j = 0; j < attrib->array.size; j++) {
        debugPrint(" %d", (int)v[j]);
      }
      debugPrint("\n");
      if (submit) {
        assert(array == XGU_VERTEX_ARRAY);
        assert(attrib->array.size == 2);
        uint32_t* p = pb_begin();
        p = xgu_vertex4f(p,  v[0], v[1], 0.0f, 1.0f);
        pb_end(p);
      }
    }
  } else if (attrib->array.gl_type == GL_FLOAT) {
    debugPrint("\narray %d as GL_FLOAT, stride %d at %p\n", array, attrib->array.stride, attrib->array.data);
    for(int i = start; i < end; i++) {
      float* v = i * attrib->array.stride + (uintptr_t)attrib->array.data;
      debugPrint("[%d]:", i);
      for(int j = 0; j < attrib->array.size; j++) {
        debugPrint(" ");
        debugPrintFloat(v[j]);
      }
      debugPrint("\n");
      if (submit) {
        assert(array == XGU_VERTEX_ARRAY);
        uint32_t* p = pb_begin();
        if (attrib->array.size == 2){
          p = xgu_vertex4f(p,  v[0], v[1], 0.0f, 1.0f);
        } else if (attrib->array.size == 3){
          p = xgu_vertex4f(p,  v[0], v[1], v[2], 1.0f);
        } else {
          assert(false);
        }
        pb_end(p);
      }
    }
  }
}

static void setup_attrib(XguVertexArray array, Attrib* attrib) {
  if (!attrib->array.enabled) {
    uint32_t* p = pb_begin();
    //FIXME: Somehow XGU should check for XGU_FLOAT if size is 0? type=FLOAT + size=0 means disabled
    p = xgu_set_vertex_data_array_format(p, array, XGU_FLOAT, 0, 0);
    p = xgu_set_vertex_data4f(p, array, attrib->value[0], attrib->value[1], attrib->value[2], attrib->value[3]);
    pb_end(p);
    return;
  }
  assert(attrib->array.size > 0);
  assert(attrib->array.stride > 0);
  xgux_set_attrib_pointer(array, gl_to_xgu_vertex_array_type(attrib->array.gl_type), attrib->array.size, attrib->array.stride, attrib->array.data);
}

static bool is_texture_complete(Texture* tx) {
  if (tx->width == 0) { return false; }
  if (tx->height == 0) { return false; }
  unimplemented(); //FIXME: Check mipmaps, filters, ..
  return true;
}

static unsigned int gl_to_xgu_texture_format(GLenum internalformat) {
  switch(internalformat) {
  case GL_LUMINANCE:       return XGU_TEXTURE_FORMAT_Y8;
  case GL_LUMINANCE_ALPHA: return XGU_TEXTURE_FORMAT_A8Y8;
  case GL_RGB:             return XGU_TEXTURE_FORMAT_X8R8G8B8;
  case GL_RGBA:            return XGU_TEXTURE_FORMAT_A8B8G8R8;
  default:
    unimplemented("%d", internalformat);
    assert(false);
    break;
  }
  return -1;
}

static void setup_textures() {

  uint32_t* p;

  // Disable texture dependencies (unused)
  //FIXME: init once?
  p = pb_begin();
  p = pb_push1(p, NV097_SET_SHADER_OTHER_STAGE_INPUT,
        MASK(NV097_SET_SHADER_OTHER_STAGE_INPUT_STAGE1, 0)
      | MASK(NV097_SET_SHADER_OTHER_STAGE_INPUT_STAGE2, 0)
      | MASK(NV097_SET_SHADER_OTHER_STAGE_INPUT_STAGE3, 0));
  pb_end(p);

  unsigned int clip_plane_index = 0;
  unsigned int shaders[4];

  for(int i = 0; i < 4; i++) {

    // Find texture object
    Texture* tx = get_bound_texture(i);
    
    // Check if this unit is disabled
    if (!state.texture_2d[i] || !is_texture_complete(tx)) {

      // Disable texture
      p = pb_begin();
      p = xgu_set_texture_control0(p, i, false, 0, 0);
      //FIXME: pbkit also sets wrap/addressing and filter stuff for disabled textures?!
      pb_end(p);

      // Find the next used clip plane
      while(clip_plane_index < ARRAY_SIZE(clip_planes)) {
        if (clip_planes[clip_plane_index].enabled) {
          break;
        }
        clip_plane_index++;
      }

      // Set clip plane if one was found
      if (clip_plane_index < ARRAY_SIZE(clip_planes)) {
        const ClipPlane* clip_plane = &clip_planes[clip_plane_index];

        // Set vertex attributes for the texture coordinates
        //FIXME: This is wrong? It has to inherit the vertex position
        //       Should be using texgen probably
        Attrib attrib = {
          .array = { .enabled = false },
          .value = { clip_plane->x, clip_plane->y, clip_plane->z, clip_plane->w }
        };
        setup_attrib(9 /*FIXME: XGU_TEXCOORD0_ARRAY*/+i, &attrib);

        // Setup shader
        //FIXME: Can use 4 clip planes per texture unit by using texture-matrix?
        //FIXME: Also disable texture matrix?!
        unimplemented(); //FIXME: This should be NV097_SET_SHADER_STAGE_PROGRAM_STAGE0_CLIP_PLANE
        shaders[i] = NV097_SET_SHADER_STAGE_PROGRAM_STAGE0_PROGRAM_NONE;

        clip_plane_index++;
        continue;
      }

      // Disable texture shader if nothing depends on it
      shaders[i] = NV097_SET_SHADER_STAGE_PROGRAM_STAGE0_PROGRAM_NONE;

      continue;
    }

    // Sanity check texture
    assert(tx->width != 0);
    assert(tx->height != 0);
    assert(tx->pitch != 0);
    debugPrint("%d x %d [%d]\n", tx->width, tx->height, tx->pitch);
    assert(tx->data != NULL);

    // Setup texture
    shaders[i] = NV097_SET_SHADER_STAGE_PROGRAM_STAGE0_2D_PROJECTIVE;
    p = pb_begin();

    //FIXME: I'd rather have XGU_TEXTURE_2D and XGU_TEXTURE_CUBEMAP for these
    bool cubemap_enable = false;
    unsigned int dimensionality = 2;

    unsigned int context_dma = 2; //FIXME: Which one did pbkit use?
    bool border = false;
    p = xgu_set_texture_offset(p, i, (uintptr_t)tx->data & 0x03ffffff);
    p = xgu_set_texture_format(p, i, context_dma, cubemap_enable, border, dimensionality,
                                     gl_to_xgu_texture_format(tx->internal_base_format), 1,
                                     0,0,0);
    unimplemented("Setup wrap"); //FIXME: !!!
    p = xgu_set_texture_address(p, i, 0x00030303); //FIXME: Shitty workaround for XGU
    p = xgu_set_texture_control0(p, i, true, 0, 0);
    p = xgu_set_texture_control1(p, i, tx->pitch);
    p = xgu_set_texture_filter(p, i, 0, XGU_TEXTURE_CONVOLUTION_QUINCUNX,
                                        gl_to_xgu_texture_filter(tx->min_filter),
                                        gl_to_xgu_texture_filter(tx->mag_filter),
                                        false, false, false, false);
    p = xgu_set_texture_image_rect(p, i, tx->width, tx->height);

#if 0
    //FIXME: Use NV097_SET_TEXTURE_FORMAT and friends
    p = pb_push2(p,NV20_TCL_PRIMITIVE_3D_TX_OFFSET(i), (uintptr_t)tx->data & 0x03ffffff, 0x0001002A | (gl_to_xgu_texture_format(tx->internal_base_format) << 8)); //set stage 0 texture address & format
    p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_NPOT_PITCH(i), tx->pitch<<16); //set stage 0 texture pitch (pitch<<16)
    p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_NPOT_SIZE(i), (tx->width<<16) | tx->height); //set stage 0 texture width & height ((witdh<<16)|height)

    p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_WRAP(i),0x00030303);//set stage 0 texture modes (0x0W0V0U wrapping: 1=wrap 2=mirror 3=clamp 4=border 5=clamp to edge)
    p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_ENABLE(i),0x4003ffc0); //set stage 0 texture enable flags

    p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_FILTER(i),0x04074000); //set stage 0 texture filters (AA!)
#endif

    p = xgu_set_texgen_s(p, i, XGU_TEXGEN_DISABLE);
    p = xgu_set_texgen_t(p, i, XGU_TEXGEN_DISABLE);
    p = xgu_set_texgen_r(p, i, XGU_TEXGEN_DISABLE);
    p = xgu_set_texgen_q(p, i, XGU_TEXGEN_DISABLE);
    p = xgu_set_texture_matrix_enable(p, i, true);
    unimplemented(); //FIXME: Not hitting pixel centers yet!
    const float m[4*4] = {
      tx->width, 0.0f,       0.0f, 0.0f,
      0.0f,      tx->height, 0.0f, 0.0f,
      0.0f,      0.0f,       1.0f, 0.0f,
      0.0f,      0.0f,       0.0f, 1.0f
    };
    unimplemented(); //FIXME: Also respect matrix_t
    p = xgu_set_texture_matrix(p, i, m);

    pb_end(p);

    // Setup texture shader
    p = pb_begin();
    p = pb_push1(p, NV097_SET_SHADER_STAGE_PROGRAM,
          MASK(NV097_SET_SHADER_STAGE_PROGRAM_STAGE0, shaders[0])
        | MASK(NV097_SET_SHADER_STAGE_PROGRAM_STAGE1, shaders[1])
        | MASK(NV097_SET_SHADER_STAGE_PROGRAM_STAGE2, shaders[2])
        | MASK(NV097_SET_SHADER_STAGE_PROGRAM_STAGE3, shaders[3]));
    pb_end(p);
  }


  // Assert that we handled all clip planes
  while(clip_plane_index < ARRAY_SIZE(clip_planes)) {
    assert(!clip_planes[clip_plane_index].enabled);
    clip_plane_index++;
  }
}

static void setup_matrices() {


    //FIXME: one time init?
    uint32_t* p = pb_begin();

    /* A generic identity matrix */
    const float m_identity[4*4] = DEFAULT_MATRIX();

    //FIXME: p = xgu_set_skinning(p, XGU_SKINNING_OFF);
    //FIXME: p = xgu_set_normalization(p, false);
    //FIXME: p = xgu_set_lighting_enable(p, false);



    pb_end(p);

    int width = pb_back_buffer_width();
    int height = pb_back_buffer_height();





    // Update matrices

    //FIXME: Keep dirty bits
unimplemented(); //FIXME:    const
float* matrix_p_now = &matrix_p[matrix_p_slot * 4*4];
    const float* matrix_mv_now = &matrix_mv[matrix_mv_slot * 4*4];


#if 0
debugPrint("\ndraw:\n");
  PRINT_MATRIX(matrix_p_now);
#endif

#if 1
  {
    static float m[4*4];
    memcpy(m, matrix_mv_now, sizeof(m));
    matrix_mv_now = m;
  } {
    static float m[4*4];
    memcpy(m, matrix_p_now, sizeof(m));
    matrix_p_now = m;
  }






float f = 6000.0f;

  if (true) {
    float m[4*4];
    matrix_identity(m);
static float dx = 320.0f;
static float dy = -240.0f;
#if 0
if (g != NULL) {
  float xx = SDL_GameControllerGetAxis(g, SDL_CONTROLLER_AXIS_RIGHTX);
  if (fabsf(xx) > 1000) { dx += xx/f; }
  float xy = SDL_GameControllerGetAxis(g, SDL_CONTROLLER_AXIS_RIGHTY);
  if (fabsf(xy) > 1000) { dy += xy/f; }
}
pb_print("%d %d scale\n", (int)(1000*dx), (int)(1000*dy));
#endif
    _math_matrix_scale(m, dx, dy, (float)0xFFFF);
    float t[4*4];
    memcpy(t, matrix_p_now, sizeof(t));
    matmul4(matrix_p_now, t, m);
  }


  if (true) {
    float m[4*4];
    matrix_identity(m);
static float dx = 0.0f;
static float dy = -480.0f;
#if 0
if (g != NULL) {
  float xx = SDL_GameControllerGetAxis(g, SDL_CONTROLLER_AXIS_LEFTX);
  if (fabsf(xx) > 1000) { dx += xx/f; }
  float xy = SDL_GameControllerGetAxis(g, SDL_CONTROLLER_AXIS_LEFTY);
  if (fabsf(xy) > 1000) { dy += xy/f; }
}
pb_print("%d %d trans\n", (int)(1000*dx), (int)(1000*dy));
#endif
    _math_matrix_translate(m, dx, dy, 0.0f);
    float t[4*4];
    memcpy(t, matrix_p_now, sizeof(t));
    matmul4(matrix_p_now, t, m);
  }

#if 0
matrix_p_now[4*3+0] = 0.0f;
matrix_p_now[4*3+1] = 0.14f;
matrix_p_now[4*3+2] = -0.99f;
matrix_p_now[4*3+3] = 501.0f;
#endif

#if 0
matrix_p_now[4*0+3] = 320.0f;
matrix_p_now[4*1+3] = 240.0f;
#endif
#if 0
matrix_p_now[4*2+0] = 10.0f;
matrix_p_now[4*2+1] = 10.0f;
matrix_p_now[4*2+3] = 0.0f;
#endif
#if 0
matrix_p_now[4*3+0] = 10.0f;
matrix_p_now[4*3+1] = 10.0f;
matrix_p_now[4*3+2] = 0.0f;
#endif
#if 0
matrix_p_now[4*3+3] = 10.0f;
#endif

//    matrix_identity(matrix_mv_now);
    //matrix_identity(matrix_p_now);

#if 1
debugPrint("\ndraw (xbox):\n");
  PRINT_MATRIX(matrix_p_now);
#endif

//Sleep(100);
#endif

#if 0
    float m_viewport[4*4] = {
        320.0f,   0.0f,   0.0f,   0.0f,
        0.0f,   -240.0f,   0.0f,   0.0f,
        0.0f,     0.0f, 0xFFFF,   0,
        0.0f,    480.0f,   0,   1
    };
//memcpy(matrix_mv_now, m_viewport, sizeof(m_viewport));
//memcpy(matrix_p_now, m_identity, sizeof(m_identity));

matmul4(matrix_p_now,matrix_p_now,m_viewport);
#endif

#if 0
    float m_viewport[4*4] = {
        0.5 * width/2.0f, 0.0f,         0.0f,        width/2.0f,
        0.0f,       0.5*-height/2.0f, 0.0f,          height/2.0f,
        0.0f,       0.0f,         (float)0x7FFF, 0x7FFF,
        0.0f,       0.0f,         0.0f,          1.0f
    };
//memcpy(matrix_mv_now, m_viewport, sizeof(m_viewport));
//memcpy(matrix_p_now, m_identity, sizeof(m_identity));

matmul4(matrix_p_now,m_viewport,matrix_p_now);
#endif


// Transpose
#define TRANSPOSE(__m) \
  { \
    float* m_old = __m; \
    float m_new[4*4]; \
    for(int i = 0; i < 4; i++) { \
      for(int j = 0; j < 4; j++) { \
        m_new[i+4*j] = m_old[i*4+j]; \
      } \
    } \
    memcpy(m_old, m_new, sizeof(m_new)); \
  }
TRANSPOSE(matrix_p_now)
TRANSPOSE(matrix_mv_now)



  //FIXME: Could be wrong
  float matrix_c_now[4*4];
  matmul4(matrix_c_now, matrix_mv_now, matrix_p_now);

  CHECK_MATRIX(matrix_p_now);
  CHECK_MATRIX(matrix_mv_now);
  CHECK_MATRIX(matrix_c_now);


#if 0
  for(int i = 0; i < XGU_WEIGHT_COUNT; i++) {
    p = xgu_set_model_view_matrix(p, i, m_identity); //FIXME: Not sure when used?
    p = xgu_set_inverse_model_view_matrix(p, i, m_identity); //FIXME: Not sure when used?
  }
#endif
  
  
  /* Set up all states for hardware vertex pipeline */
  //FIXME: Might need inverse or some additional transform
  p = pb_begin();
  p = xgu_set_transform_execution_mode(p, XGU_FIXED, XGU_RANGE_MODE_PRIVATE);
  //FIXME: p = xgu_set_fog_enable(p, false);
  p = xgu_set_projection_matrix(p, matrix_p_now); //FIXME: Unused in XQEMU
  p = xgu_set_composite_matrix(p, matrix_c_now); //FIXME: Always used in XQEMU?
  p = xgu_set_viewport_offset(p, 0.0f, 0.0f, 0.0f, 0.0f);
  p = xgu_set_viewport_scale(p, 1.0f / width, 1.0f / height, 1.0f / (float)0xFFFF, 1.0f); //FIXME: Ignored?!
  pb_end(p);

}

static void prepare_drawing() {
  debugPrint("Preparing to draw\n");

  //FIXME: Track dirty bits

  // Setup lights from state.light[].enabled
  //FIXME: This uses a mask: p = xgu_set_light_enable(p, );

  // Setup attributes
#if 1
  uint32_t* p = pb_begin();
  for(int i = 0; i < 16; i++) {
    p = xgu_set_vertex_data_array_format(p, i, XGU_FLOAT, 0, 0);
  }
  pb_end(p);
#endif
  setup_attrib(XGU_VERTEX_ARRAY, &state.vertex_array);
  setup_attrib(XGU_COLOR_ARRAY, &state.color_array);
  setup_attrib(XGU_NORMAL_ARRAY, &state.normal_array);
  setup_attrib( 9 /*FIXME:XGU_TEXCOORD0_ARRAY*/, &state.texture_coord_array[0]);
  setup_attrib(10 /*FIXME:XGU_TEXCOORD1_ARRAY*/, &state.texture_coord_array[1]);
  setup_attrib(11 /*FIXME:XGU_TEXCOORD2_ARRAY*/, &state.texture_coord_array[2]);
  setup_attrib(12 /*FIXME:XGU_TEXCOORD3_ARRAY*/, &state.texture_coord_array[3]);

  // Set up all matrices etc.
  setup_matrices();

  // Setup textures
  setup_textures();

  // Set the register combiner
  setup_texenv();

#if 1
  {
    // Set some safe state
    uint32_t* p = pb_begin();
    p = xgu_set_cull_face_enable(p, false);
    p = xgu_set_depth_test_enable(p, false);
    p = xgu_set_lighting_enable(p, false);
    pb_end(p);
  }
#endif

#if 0
  {
    // Set some safe alpha state
    uint32_t* p = pb_begin();
    p = xgu_set_alpha_test_enable(p, false);
    p = xgu_set_blend_enable(p, false);
    pb_end(p);
  }
#endif

#if 0
  {
    // Disco lighting
    uint32_t* p = pb_begin();
    p = xgu_set_vertex_data_array_format(p, XGU_COLOR_ARRAY, XGU_FLOAT, 0, 0);
    p = xgu_set_vertex_data4ub(p, XGU_COLOR_ARRAY, rand() & 0xFF, rand() & 0xFF, rand() & 0xFF, rand() & 0xFF);
    pb_end(p);
  }
#endif

#if 0
  {
    // Output selector
    unsigned int source = _RC_TEXTURE+0;
    uint32_t* p = pb_begin();
    p = pb_push1(p, NV097_SET_COMBINER_SPECULAR_FOG_CW0,
          MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_SOURCE, _RC_ZERO)   | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_ALPHA, 0) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_A_INVERSE, 0)
        | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_SOURCE, _RC_ZERO)   | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_ALPHA, 0) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_B_INVERSE, 0)
        | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_SOURCE, _RC_ZERO)   | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_ALPHA, 0) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_C_INVERSE, 0)
        | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_SOURCE, source) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_ALPHA, 0) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW0_D_INVERSE, 0));
    p = pb_push1(p, NV097_SET_COMBINER_SPECULAR_FOG_CW1,
          MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_SOURCE, _RC_ZERO)   | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_ALPHA, 0) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_E_INVERSE, 0)
        | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_SOURCE, _RC_ZERO)   | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_ALPHA, 0) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_F_INVERSE, 0)
        | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_SOURCE, source) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_ALPHA, 1) | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_G_INVERSE, 0)
        | MASK(NV097_SET_COMBINER_SPECULAR_FOG_CW1_SPECULAR_CLAMP, 0));
    pb_end(p);
  }
#endif
}





GL_API void GL_APIENTRY glGetIntegerv (GLenum pname, GLint *data) {
  switch(pname) {
  case GL_MAX_TEXTURE_SIZE:
    data[0] = 1024; //FIXME: We can do more probably
    break;
  case GL_MAX_TEXTURE_UNITS:
    data[0] = 4;
    break;
  case GL_MATRIX_MODE:
    data[0] = matrix_mode;
    break;
  default:
    unimplemented("%d", pname);
    assert(false);
    break;
  }
}

GL_API void GL_APIENTRY glGetFloatv (GLenum pname, GLfloat *data) {
  switch(pname) {
  case GL_MODELVIEW_MATRIX:
    memcpy(data, &matrix_mv[matrix_mv_slot * 16], 16 * sizeof(GLfloat));
    break;
  case GL_PROJECTION_MATRIX:
    memcpy(data, &matrix_p[matrix_p_slot * 16], 16 * sizeof(GLfloat));
    break;
  default:
    unimplemented("%d", pname);
    assert(false);
    break;
  }
}


GL_API const GLubyte *GL_APIENTRY glGetString (GLenum name) {
  const char* result = "";
  switch(name) {
  case GL_EXTENSIONS:
    result = "";
    break;
  case GL_VERSION:
    result = "OpenGL ES-CL 1.1";
    break;
  case GL_RENDERER:
    result = "XGU";
    break;
  case GL_VENDOR:
    result = "Jannik Vogel";
    break;
  default:
    unimplemented("%d", name);
    assert(false);
    break;
  }
  return (GLubyte*)result;
}


// Clearing
GL_API void GL_APIENTRY glClear (GLbitfield mask) {

  XguClearSurface flags = 0;
  if (mask & GL_COLOR_BUFFER_BIT)   { flags |= XGU_CLEAR_COLOR;   }
  if (mask & GL_DEPTH_BUFFER_BIT)   { flags |= XGU_CLEAR_Z;       }
  if (mask & GL_STENCIL_BUFFER_BIT) { flags |= XGU_CLEAR_STENCIL; }

  pb_print("Clearing\n");

  uint32_t* p = pb_begin();
  //FIXME: Set clear region
  p = xgu_clear_surface(p, flags);
  pb_end(p);

  //FIXME: Store size we used to init
  int width = pb_back_buffer_width();
  int height = pb_back_buffer_height();

  //FIXME: Remove this hack!
  pb_erase_depth_stencil_buffer(0, 0, width, height); //FIXME: Do in XGU
  pb_fill(0, 0, width, height, 0x80808080); //FIXME: Do in XGU

}

GL_API void GL_APIENTRY glClearColor (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {

  uint32_t color = 0;
  //FIXME: Verify order
  color |= f_to_u8(red) << 0;
  color |= f_to_u8(green) << 8;
  color |= f_to_u8(blue) << 16;
  color |= f_to_u8(alpha) << 24;

  uint32_t* p = pb_begin();
  p = xgu_set_color_clear_value(p, color);
  pb_end(p);
}


// Buffers
GL_API void GL_APIENTRY glGenBuffers (GLsizei n, GLuint *buffers) {
  Buffer buffer = DEFAULT_BUFFER();
  gen_objects(n, buffers, &buffer, sizeof(Buffer));
}

GL_API void GL_APIENTRY glBindBuffer (GLenum target, GLuint buffer) {
  GLuint* bound_buffer_store = get_bound_buffer_store(target);
  *bound_buffer_store = buffer;
}

GL_API void GL_APIENTRY glBufferData (GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
  Buffer* buffer = objects[*get_bound_buffer_store(target)-1].data;
  if (buffer->data != NULL) {
    //FIXME: Re-use existing buffer if it's a good fit?
    assert(false); //FIXME: Assert that this memory is no longer used
    FreeResourceMemory(buffer->data);
  }
  buffer->data = AllocateResourceMemory(size);
  buffer->size = size;
  assert(buffer->data != NULL);
  if (data != NULL) {
    memcpy(buffer->data, data, size);
  } else {
    memset(buffer->data, 0x00, size);
  }
}

GL_API void GL_APIENTRY glBufferSubData (GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
  Buffer* buffer = objects[*get_bound_buffer_store(target)-1].data;
  assert(buffer->data != NULL);
  assert(buffer->size >= (offset + size));
  assert(data != NULL);
  memcpy(&buffer->data[offset], data, size);
debugPrint("Set %d bytes at %d in %p\n", size, offset, &buffer->data[offset]);
}

GL_API void GL_APIENTRY glDeleteBuffers (GLsizei n, const GLuint *buffers) {
  for(int i = 0; i < n; i++) {

    //FIXME: Also ignore non-existing names
    if (buffers[i] == 0) {
      continue;
    }

    Buffer* buffer = objects[buffers[i]-1].data;
    if (buffer->data != NULL) {
      unimplemented(); //FIXME: Assert that the data is no longer used
      FreeResourceMemory(buffer->data);
    }
  }
  del_objects(n, buffers);
}

static void store_attrib_pointer(Attrib* attrib, GLenum gl_type, unsigned int size, size_t stride, const void* pointer) {
  uintptr_t base;
  if (gl_array_buffer == 0) {
    base = 0;
  } else {
    Buffer* buffer = objects[gl_array_buffer-1].data;
    base = (uintptr_t)buffer->data;
    assert(base != 0);
  }
  attrib->array.gl_type = gl_type;
  attrib->array.size = size;
  attrib->array.stride = stride;
  attrib->array.data = (const void*)(base + (uintptr_t)pointer);
}

// Vertex buffers
GL_API void GL_APIENTRY glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const void *pointer) {
  store_attrib_pointer(&state.texture_coord_array[client_active_texture], type, size, stride, pointer);
}

GL_API void GL_APIENTRY glColorPointer (GLint size, GLenum type, GLsizei stride, const void *pointer) {
  store_attrib_pointer(&state.color_array, type, size, stride, pointer);
}

GL_API void GL_APIENTRY glNormalPointer (GLenum type, GLsizei stride, const void *pointer) {
  store_attrib_pointer(&state.normal_array, type, 3, stride, pointer);
}

GL_API void GL_APIENTRY glVertexPointer (GLint size, GLenum type, GLsizei stride, const void *pointer) {
  store_attrib_pointer(&state.vertex_array, type, size, stride, pointer);
}

static uint32_t* borders(uint32_t* p) {

/*
  p = xgu_begin(p, XGU_LINE_STRIP);
  p = xgux_set_color3f(p, 1.0f, 0.0f, 0.0f);
  p = xgu_vertex4f(p,  10,  10, 1.0f, 1.0f);
  p = xgu_vertex4f(p,  10, 470, 1.0f, 1.0f);
  p = xgu_vertex4f(p, 630, 470, 1.0f, 1.0f);
  p = xgu_vertex4f(p, 630,  10, 1.0f, 1.0f);
  p = xgu_vertex4f(p,  10,  10, 1.0f, 1.0f);
  p = xgu_end(p);

  p = xgu_begin(p, XGU_LINE_STRIP);
  p = xgux_set_color3f(p, 0.0f, 0.0f, 1.0f);
  p = xgu_vertex4f(p, -310, -230, 1.0f, 1.0f);
  p = xgu_vertex4f(p, -310,  230, 1.0f, 1.0f);
  p = xgu_vertex4f(p,  310,  230, 1.0f, 1.0f);
  p = xgu_vertex4f(p,  310, -230, 1.0f, 1.0f);
  p = xgu_vertex4f(p, -310, -230, 1.0f, 1.0f);
  p = xgu_end(p);
*/

/*
  p = xgu_begin(p, XGU_TRIANGLE_STRIP);
  p = xgux_set_color3f(p, 1.0f, 1.0f, 1.0f);
  for(int i = -1; i < 4; i++)
  p = xgux_set_texcoord3f(p, i, 0.0f, 0.0f, 1.0f); p = xgu_vertex4f(p, -310, -230, 1.0f, 1.0f);
  for(int i = -1; i < 4; i++)
  p = xgux_set_texcoord3f(p, i, 32.0f, 0.0f, 1.0f); p = xgu_vertex4f(p,  310, -230, 1.0f, 1.0f);
  for(int i = -1; i < 4; i++)
  p = xgux_set_texcoord3f(p, i, 0.0f, 32.0f, 1.0f); p = xgu_vertex4f(p, -310,  230, 1.0f, 1.0f);
  for(int i = -1; i < 4; i++)
  p = xgux_set_texcoord3f(p, i, 32.0f, 32.0f, 1.0f); p = xgu_vertex4f(p,  310,  230, 1.0f, 1.0f);
  p = xgu_end(p);
*/

  return p;
}


// Draw calls
GL_API void GL_APIENTRY glDrawArrays (GLenum mode, GLint first, GLsizei count) {

unimplemented("disabled due to errors on physical hardware");
return; //FIXME: !!!

  static unsigned int f = -1;
  if (f == frame) {
    //return;
  }
  f = frame;

  prepare_drawing();

  xgux_draw_arrays(gl_to_xgu_primitive_type(mode), first, count);

#if 1
  uint32_t* p = pb_begin();
#if 0
  p = xgu_begin(p, XGU_LINE_STRIP); //gl_to_xgu_primitive_type(mode));
  pb_end(p);
  print_attrib(XGU_VERTEX_ARRAY, &state.vertex_array, first, count, true);
  p = pb_begin();
  p = xgu_end(p);
#endif
p = borders(p);
  pb_end(p);
#endif
}

GL_API void GL_APIENTRY glDrawElements (GLenum mode, GLsizei count, GLenum type, const void *indices) {

//return;

  static unsigned int f = -1;
  if (f == frame) {
    //return;
  }
  f = frame;

  prepare_drawing();

  uintptr_t base;
  if (gl_element_array_buffer == 0) {
    base = 0;
  } else {
    Buffer* buffer = objects[gl_element_array_buffer-1].data;
    base = (uintptr_t)buffer->data;
    assert(base != 0);
  }

  // This function only handles the 16 bit variant for now
  switch(type) {
  case GL_UNSIGNED_SHORT: {
    const uint16_t* indices_ptr = (const uint16_t*)(base + (uintptr_t)indices);
    xgux_draw_elements16(gl_to_xgu_primitive_type(mode), indices_ptr, count);
#if 1
    uint32_t* p = pb_begin();
#if 0
    p = xgu_begin(p, XGU_LINE_STRIP); //gl_to_xgu_primitive_type(mode));
    p = xgu_vertex3f(p, 0, 0, 1);
    pb_end(p);
    for(unsigned int i = 0; i < count; i++) {
      print_attrib(XGU_VERTEX_ARRAY, &state.vertex_array, indices_ptr[i], 1, true); 
    }
    p = pb_begin();
    p = xgu_end(p);
#endif
p = borders(p);
    pb_end(p);
#endif
    break;
  }
#if 0
  //FIXME: Not declared in gl.h + untested
  case GL_UNSIGNED_INT: {
    const uint32_t* indices_ptr = (const uint32_t*)(base + (uintptr_t)indices);
    xgux_draw_elements32(gl_to_xgu_primitive_type(mode), indices_ptr, count);
    break;
  }
#endif
  default:
    unimplemented("%d", type);
    assert(false);
    break;
  }
}


// Matrix functions
GL_API void GL_APIENTRY glMatrixMode (GLenum mode) {
  CHECK_MATRIX(matrix);

  // Remember mode
  matrix_mode = mode;

  switch(mode) {
  case GL_PROJECTION:
    matrix = &matrix_p[0];
    matrix_slot = &matrix_p_slot;
    break;
  case GL_MODELVIEW:
    matrix = &matrix_mv[0];
    matrix_slot = &matrix_mv_slot;
    break;
  case GL_TEXTURE:
    matrix = &matrix_t[client_active_texture][0];
    matrix_slot = &matrix_t_slot[client_active_texture];
    break;
  default:
    unimplemented("%d", mode);
    assert(false);
    break;
  }

  // Go to the current slot
  matrix = &matrix[*matrix_slot * 4*4];

  CHECK_MATRIX(matrix);
}

GL_API void GL_APIENTRY glLoadIdentity (void) {
  matrix_identity(matrix);

  CHECK_MATRIX(matrix);
}

#undef glMultMatrixf

static void _glMultMatrixf (const GLfloat *m) {

debugPrint("_glMultMatrixf:\n");
  CHECK_MATRIX(matrix);
//PRINT_MATRIX(matrix);
  PRINT_MATRIX(m);

  float t[4 * 4];
  matmul4(t, matrix, m);
  memcpy(matrix, t, sizeof(t));

  CHECK_MATRIX(matrix);
}

GL_API void GL_APIENTRY glMultMatrixf (const GLfloat *m) {

  float t[4 * 4];
  memcpy(t, m, sizeof(t));

//TRANSPOSE(t);

  _glMultMatrixf(t);
}

GL_API void GL_APIENTRY glOrthof (GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f) {
  float m[4*4];
  ortho(m, l, r, b, t, n, f);
  CHECK_MATRIX(m);
  _glMultMatrixf(m);
  CHECK_MATRIX(matrix);
}

GL_API void GL_APIENTRY glTranslatef (GLfloat x, GLfloat y, GLfloat z) {
  float m[4*4];
  matrix_identity(m);
  _math_matrix_translate(m, x, y, z);

#if 0
debugPrint("\ntrans: ");
debugPrintFloat(x); debugPrint(" ");
debugPrintFloat(y); debugPrint(" ");
debugPrintFloat(z); debugPrint("\n");
PRINT_MATRIX(m);
#endif

  CHECK_MATRIX(m);
  _glMultMatrixf(m);

  CHECK_MATRIX(matrix);
}

GL_API void GL_APIENTRY glRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
  float m[4*4];
  matrix_identity(m);
  _math_matrix_rotate(m, angle, x, y, z);
  CHECK_MATRIX(m);
  _glMultMatrixf(m);

  CHECK_MATRIX(matrix);
}

GL_API void GL_APIENTRY glScalef (GLfloat x, GLfloat y, GLfloat z) {
  float m[4*4];
  matrix_identity(m);
  _math_matrix_scale(m, x, y, z);
  CHECK_MATRIX(m);

#if 0
debugPrint("\nscale: ");
debugPrintFloat(x); debugPrint(" ");
debugPrintFloat(y); debugPrint(" ");
debugPrintFloat(z); debugPrint("\n");
PRINT_MATRIX(m);
#endif

  _glMultMatrixf(m);
  CHECK_MATRIX(matrix);
}

GL_API void GL_APIENTRY glPopMatrix (void) {
  assert(*matrix_slot > 0);
  matrix -= 4*4;
  *matrix_slot -= 1;

  CHECK_MATRIX(matrix);
}

GL_API void GL_APIENTRY glPushMatrix (void) {
  CHECK_MATRIX(matrix);

  float* new_matrix = matrix;
  new_matrix += 4*4;
  memcpy(new_matrix, matrix, 4*4*sizeof(float));
  matrix = new_matrix;
  *matrix_slot += 1;
  assert(*matrix_slot < 16);
}


// Framebuffer setup
GL_API void GL_APIENTRY glViewport (GLint x, GLint y, GLsizei width, GLsizei height) {
  //FIXME: Switch to xgu variant to avoid side-effects
  debugPrint("%d %d %d %d\n", x, y, width, height);
  assert(x == 0);
  assert(y == 0);
  assert(width == 640); //FIXME: Set up game correctly
  assert(height == 480); //FIXME: Set up game correctly
  if (width > 640) { width = 640; } //FIXME: Remove!
  if (height > 480) { height = 480; } //FIXME: Remove!
  pb_set_viewport(x, y, width, height, 0.0f, 1.0f);
}


// Textures
GL_API void GL_APIENTRY glGenTextures (GLsizei n, GLuint *textures) {
  Texture texture = DEFAULT_TEXTURE();
  gen_objects(n, textures, &texture, sizeof(Texture));
}

GL_API void GL_APIENTRY glBindTexture (GLenum target, GLuint texture) {
  assert(target == GL_TEXTURE_2D);
  state.texture_binding_2d[active_texture] = texture;
}

GL_API void GL_APIENTRY glActiveTexture (GLenum texture) {
  active_texture = texture - GL_TEXTURE0;
}

GL_API void GL_APIENTRY glClientActiveTexture (GLenum texture) {
  client_active_texture = texture - GL_TEXTURE0;
}

GL_API void GL_APIENTRY glDeleteTextures (GLsizei n, const GLuint *textures) {
  del_objects(n, textures);
}


GL_API void GL_APIENTRY glTexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) {
  assert(target == GL_TEXTURE_2D);
  assert(border == 0);

  Texture* tx = get_bound_texture(active_texture);

  if (level > 0) {
    unimplemented();
    return;
  }

  unsigned int bpp;

  switch(internalformat) {
  case GL_LUMINANCE:
    assert(format == GL_LUMINANCE);
    assert(type == GL_UNSIGNED_BYTE);
    tx->internal_base_format = GL_LUMINANCE;
    bpp = 1*8;
    break;
  case GL_LUMINANCE_ALPHA:
    assert(format == GL_LUMINANCE_ALPHA);
    assert(type == GL_UNSIGNED_BYTE);
    tx->internal_base_format = GL_LUMINANCE_ALPHA;
    bpp = 2*8;
    break;
  case GL_RGB:
    assert(format == GL_RGB);
    assert(type == GL_UNSIGNED_BYTE);
    tx->internal_base_format = GL_RGB;
    bpp = 4*8; // 3*8 in GL, but we have to pad this
    break;
  case GL_RGBA:
    assert(format == GL_RGBA);
    assert(type == GL_UNSIGNED_BYTE);
    tx->internal_base_format = GL_RGBA;
    bpp = 4*8;
    break;
  default:
    tx->internal_base_format = -1;
    bpp = 0;
    unimplemented("%d", internalformat);
    assert(false);
    return;
  }

  tx->width = width;
  tx->height = height;
  tx->pitch = width * bpp / 8;
  size_t size = tx->pitch * tx->height;
  if (tx->data != NULL) {
    //FIXME: Re-use existing buffer if it's a good fit?
    //FIXME: Assert that this memory is no longer used
    FreeResourceMemory(tx->data);
  }
  tx->data = AllocateResourceMemory(size);

  // Copy pixels
  //FIXME: Respect GL pixel packing stuff
  //FIXME: Swizzle
  assert(pixels != NULL);
  if (tx->internal_base_format == GL_RGB) {
    assert(format == GL_RGB);
    assert(type == GL_UNSIGNED_BYTE);
    const uint8_t* src = pixels;
    uint8_t* dst = tx->data;
    assert(tx->pitch == tx->width * 4);
    for(int y = 0; y < tx->height; y++) {
      for(int x = 0; x < tx->height; x++) {
        //FIXME: Can also use different texture format instead probably
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
        dst[3] = 0xFF;
        dst += 4;
        src += 3;
      }
    }
  } else {
    memcpy(tx->data, pixels, size);
  }
}

GL_API void GL_APIENTRY glTexParameteri (GLenum target, GLenum pname, GLint param) {
  assert(target == GL_TEXTURE_2D);

  Texture* tx = get_bound_texture(active_texture);

  switch(pname) {
  case GL_TEXTURE_MIN_FILTER:
    tx->min_filter = param;
    break;
  case GL_TEXTURE_MAG_FILTER:
    tx->mag_filter = param;
    break;
  case GL_TEXTURE_WRAP_S:
    tx->wrap_s = param;
    break;
  case GL_TEXTURE_WRAP_T:
    tx->wrap_t = param;
    break;
  default:
    unimplemented("%d", pname);
    assert(false);
    return;
  }
}


// Renderstates
GL_API void GL_APIENTRY glAlphaFunc (GLenum func, GLfloat ref) {
#if 0
  //FIXME: https://github.com/dracc/nxdk/pull/4/files#r357990989
  //       https://github.com/dracc/nxdk/pull/4/files#r357990961
  uint32_t* p = pb_begin();
  p = xgu_set_alpha_func(p, gl_to_xgu_alpha_func(func));
  p = xgu_set_alpha_ref(p, f_to_u8(ref));
  pb_end(p);
#endif
}

GL_API void GL_APIENTRY glBlendFunc (GLenum sfactor, GLenum dfactor) {
  uint32_t* p = pb_begin();
  p = xgu_set_blend_func_sfactor(p, gl_to_xgu_blend_factor(sfactor));
  p = xgu_set_blend_func_dfactor(p, gl_to_xgu_blend_factor(dfactor));
  pb_end(p);
}

GL_API void GL_APIENTRY glClipPlanef (GLenum p, const GLfloat *eqn) {
  //FIXME: Use a free texture stage and set it to cull mode
  GLuint index = p - GL_CLIP_PLANE0;
  assert(index < ARRAY_SIZE(clip_planes));

  ClipPlane* clip_plane = &clip_planes[index];

  float v[4];
  float m[4*4];
  bool ret = invert(m, &matrix_mv[matrix_mv_slot * 4*4]);
  unimplemented(); //FIXME: Multiply eqn by m to get the v

  clip_plane->x = v[0];
  clip_plane->y = v[1];
  clip_plane->z = v[2];
  clip_plane->w = v[3];
}

GL_API void GL_APIENTRY glColor4ub (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) {
  //FIXME: Could save bandwidth using other commands; however, this is currently easier to maintain
  //FIXME: Won't work between begin/end
  state.color_array.value[0] = red / 255.0f;
  state.color_array.value[1] = green / 255.0f;
  state.color_array.value[2] = blue / 255.0f;
  state.color_array.value[3] = alpha / 255.0f;
}

GL_API void GL_APIENTRY glColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
  //FIXME: Won't work between begin/end
  state.color_array.value[0] = red;
  state.color_array.value[1] = green;
  state.color_array.value[2] = blue;
  state.color_array.value[3] = alpha;
}

GL_API void GL_APIENTRY glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {

  XguColorMask mask = 0;
  if (red   != GL_FALSE) { mask |= XGU_RED;   }
  if (green != GL_FALSE) { mask |= XGU_GREEN; }
  if (blue  != GL_FALSE) { mask |= XGU_BLUE;  }
  if (alpha != GL_FALSE) { mask |= XGU_ALPHA; }

  uint32_t* p = pb_begin();
  p = xgu_set_color_mask(p, mask);
  pb_end(p);
}

GL_API void GL_APIENTRY glDepthFunc (GLenum func) {
  uint32_t* p = pb_begin();
//  p = xgu_set_depth_func(p, gl_to_xgu_func_type(func));
  pb_end(p);
}

GL_API void GL_APIENTRY glDepthMask (GLboolean flag) {
  unimplemented(); //FIXME: Missing from XGU
}

GL_API void GL_APIENTRY glEnable (GLenum cap) {
  uint32_t* p = pb_begin();
  p = set_enabled(p, cap, true);
  pb_end(p);
}

GL_API void GL_APIENTRY glDisable (GLenum cap) {
  uint32_t* p = pb_begin();
  p = set_enabled(p, cap, false);
  pb_end(p);
}

GL_API void GL_APIENTRY glEnableClientState (GLenum array) {
  set_client_state_enabled(array, true);
}

GL_API void GL_APIENTRY glDisableClientState (GLenum array) {
  set_client_state_enabled(array, false);
}

GL_API void GL_APIENTRY glCullFace (GLenum mode) {
  uint32_t* p = pb_begin();
  p = xgu_set_cull_face(p, gl_to_xgu_cull_face(mode));
  pb_end(p);
}

GL_API void GL_APIENTRY glFrontFace (GLenum mode) {
  uint32_t* p = pb_begin();
  p = xgu_set_front_face(p, gl_to_xgu_front_face(mode));
  pb_end(p);
}


// Stencil actions
GL_API void GL_APIENTRY glStencilFunc (GLenum func, GLint ref, GLuint mask) {
  unimplemented(); //FIXME: Missing from XGU
}

GL_API void GL_APIENTRY glStencilOp (GLenum fail, GLenum zfail, GLenum zpass) {
  uint32_t* p = pb_begin();
  p = xgu_set_stencil_op_fail(p, gl_to_xgu_stencil_op(fail));
  p = xgu_set_stencil_op_zfail(p, gl_to_xgu_stencil_op(zfail));
  p = xgu_set_stencil_op_zpass(p, gl_to_xgu_stencil_op(zpass));
  pb_end(p);
}


// Misc.
GL_API void GL_APIENTRY glPointParameterf (GLenum pname, GLfloat param) {
  unimplemented(); //FIXME: Bad in XGU
}

GL_API void GL_APIENTRY glPointParameterfv (GLenum pname, const GLfloat *params) {
  unimplemented(); //FIXME: Bad in XGU
}

GL_API void GL_APIENTRY glPointSize (GLfloat size) {
  unimplemented(); //FIXME: Bad in XGU
}

GL_API void GL_APIENTRY glPolygonOffset (GLfloat factor, GLfloat units) {
  unimplemented(); //FIXME: Missing from XGU
}


// TexEnv
GL_API void GL_APIENTRY glTexEnvi (GLenum target, GLenum pname, GLint param) {

  // Deal with the weird pointsprite target first
  if (target == GL_POINT_SPRITE) {
    assert(pname == GL_COORD_REPLACE);
    unimplemented(); //FIXME: !!! Supported on Xbox?!
    return;
  }

  // Deal with actual texenv stuff now

  assert(target == GL_TEXTURE_ENV);

  TexEnv* t = &texenvs[active_texture];

  switch(pname) {

  case GL_TEXTURE_ENV_MODE:
    t->env_mode = param;
    break;

  case GL_COMBINE_RGB:
    t->combine_rgb = param;
    break;
  case GL_COMBINE_ALPHA:
    t->combine_alpha = param;
    break;

  case GL_SRC0_RGB:
    t->src_rgb[0] = param;
    break;
  case GL_SRC1_RGB:
    t->src_rgb[1] = param;
    break;
  case GL_SRC2_RGB:
    t->src_rgb[2] = param;
    break;

  case GL_SRC0_ALPHA:
    t->src_alpha[0] = param;
    break;

  case GL_OPERAND0_RGB:
    t->src_operand_rgb[0] = param;
    break;

  case GL_OPERAND1_RGB:
    t->src_operand_rgb[1] = param;
    break;

  case GL_OPERAND2_RGB:
    t->src_operand_rgb[2] = param;
    break;

  case GL_OPERAND0_ALPHA:
    t->src_operand_alpha[0] = param;
    break;

  default:
    unimplemented("%d", pname);
    assert(false);
    return;
  }
}


// Pixel readback
GL_API void GL_APIENTRY glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels) {
  // Not implemented, only used in screenshots
  unimplemented();
}


// Lighting
GL_API void GL_APIENTRY glLightModelf (GLenum pname, GLfloat param) {
  switch(pname) {
  default:
    unimplemented("%d", pname);
    assert(false);
    return;
  }
}

GL_API void GL_APIENTRY glLightModelfv (GLenum pname, const GLfloat *params) {
  switch(pname) {
  case GL_LIGHT_MODEL_AMBIENT:
    unimplemented("%d", pname); //FIXME: !!!
    break;
//#define GL_LIGHT_MODEL_TWO_SIDE 10032:
  default:
    unimplemented("%d", pname);
    assert(false);
    return;
  }
}

GL_API void GL_APIENTRY glLightfv (GLenum light, GLenum pname, const GLfloat *params) {
  assert(light >= GL_LIGHT0);
  unsigned int light_index = light - GL_LIGHT0;
  assert(light_index < 4); //FIXME: Not sure how many lights Xbox has; there's probably some constant we can use
  switch(pname) {
  case GL_POSITION:
    unimplemented("%d", pname); //FIXME: !!!
    break;
  case GL_AMBIENT:
    unimplemented("%d", pname); //FIXME: !!!
    break;
  case GL_DIFFUSE:
    unimplemented("%d", pname); //FIXME: !!!
    break;
  case GL_SPECULAR:
    unimplemented("%d", pname); //FIXME: !!!
    break;
  default:
    unimplemented("%d", pname);
    assert(false);
    return;
  }
}


// Materials
GL_API void GL_APIENTRY glMaterialfv (GLenum face, GLenum pname, const GLfloat *params) {

  if (face == GL_FRONT_AND_BACK) {
    glMaterialfv(GL_FRONT, pname, params);
    glMaterialfv(GL_BACK, pname, params);
    return;
  }

  assert(face == GL_FRONT || face == GL_BACK);
  switch(pname) {
  case GL_SHININESS:
    unimplemented("%d", pname); //FIXME: !!!
    break;
  case GL_EMISSION:
    unimplemented("%d", pname); //FIXME: !!!
    break;
  case GL_AMBIENT:
    unimplemented("%d", pname); //FIXME: !!!
    break;
  case GL_DIFFUSE:
    unimplemented("%d", pname); //FIXME: !!!
    break;
  case GL_SPECULAR:
    unimplemented("%d", pname); //FIXME: !!!
    break;
  default:
    unimplemented("%d", pname);
    assert(false);
    return;
  }
}


// Pixel pushing
GL_API void GL_APIENTRY glPixelStorei (GLenum pname, GLint param) {
  switch(pname) {
  case GL_PACK_ALIGNMENT:
    assert(param == 1);
    break;
  case GL_UNPACK_ALIGNMENT:
    assert(param == 1);
    break;
  default:
    unimplemented("%d", pname);
    assert(false);
    break;
  }
}





// SDL GL hooks via EGL

#include <EGL/egl.h>

#if 1
EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffers (EGLDisplay dpy, EGLSurface surface) {


  frame++;

#if 1
  //FIXME: This doesn't work for some reason
  static bool draw_debug = false;
  SDL_GameControllerUpdate();
  if (g == NULL) {
    g = SDL_GameControllerOpen(0);
    if (g == NULL) {
      debugPrint("failed to open gamepad\n");
      Sleep(100);
    }
  }
  if (g != NULL) {
    bool button = SDL_GameControllerGetButton(g, SDL_CONTROLLER_BUTTON_START);
    if (button) {
      draw_debug = !draw_debug;

      if (draw_debug) {
        pb_show_front_screen();
      } else {
        pb_show_debug_screen();
      }

      Sleep(200);
    }
  }
#endif



  debugPrint("Going to swap buffers\n");

  /* Draw some text on the screen */
  static int frame = 0;
  frame++;
  pb_print("Neverball (frame %d)\n", frame);
  pb_draw_text_screen();

  while(pb_busy()) {
    /* Wait for completion... */
  }

  /* Swap buffers (if we can) */
  while (pb_finished()) {
    /* Not ready to swap yet */
  }




  // Prepare next frame

  pb_wait_for_vbl();
  pb_reset();
  pb_target_back_buffer();

  pb_erase_text_screen();

  while(pb_busy()) {
    /* Wait for completion... */
  }

  return EGL_TRUE;
}
#endif





// Initialization

#include <hal/video.h>
#include <hal/xbox.h>
#include <pbkit/pbkit.h>
#include <xboxkrnl/xboxkrnl.h>

__attribute__((destructor)) static void exit_xbox(void) {
  assert(false);
}

static HAL_SHUTDOWN_REGISTRATION shutdown_registration;
static NTAPI VOID shutdown_notification_routine (PHAL_SHUTDOWN_REGISTRATION ShutdownRegistration) {
  exit_xbox();
}

__attribute__((constructor)) static void setup_xbox(void) {

  // Open log file
  freopen("D:\\log.txt", "wb", stdout);
  printf("Log started\n");

  // Register shutdown routine to catch errors
  shutdown_registration.NotificationRoutine = shutdown_notification_routine;
  shutdown_registration.Priority = 0;
  HalRegisterShutdownNotification(&shutdown_registration, TRUE);

  // Set up display mode
  XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
  //FIXME: Re-enable
  //pb_set_color_format(NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8, false);

#if 1
  // We consume a lot of memory, so we need to claim the framebuffer
  size_t fb_size = 640 * 480 * 4;
  extern uint8_t* _fb;
  _fb = (uint8_t*)MmAllocateContiguousMemoryEx(fb_size, 0, 0xFFFFFFFF, 0x1000, PAGE_READWRITE | PAGE_WRITECOMBINE);
  memset(_fb, 0x00, fb_size);
#define _PCRTC_START				0xFD600800
  *(unsigned int*)(_PCRTC_START) = (unsigned int)_fb & 0x03FFFFFF;
  debugPrint("FB: 0x%X\n", _fb);
#endif

  // Initialize pbkit
  int err = pb_init();
  if (err) {
    debugPrint("pb_init Error %d\n", err);
    //XSleep(2000);
    //XReboot();
    return;
  }

  // Show framebuffer, not debug-screen
#if 1
  pb_show_front_screen();
#else
  pb_show_debug_screen();
#endif


#if 0
  // GL self-test (might leak some memory)
  GLuint b;
  glGenBuffers(1, &b);
  glBindBuffer(GL_ARRAY_BUFFER, b);
  uint8_t data[] = {0,-1,-1,3,4,5,6,7,8};
  glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, 0);
  uint8_t subdata[] = {1,2};
  glBufferSubData(GL_ARRAY_BUFFER, 1, sizeof(subdata), subdata);
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_SHORT, 4, (void*)0);
  prepare_drawing();
  Sleep(2000);
#endif

#if 0
  debugPrintFloat(0.01f); debugPrint(" ");
  debugPrintFloat(0.001f); debugPrint(" ");
  debugPrintFloat(0.0001f); debugPrint(" ");
  debugPrintFloat(1.0f); debugPrint(" ");
  debugPrintFloat(-1.0f); debugPrint(" ");
  debugPrintFloat(123.456f); debugPrint(" ");
  debugPrintFloat(123.4f); debugPrint(" ");
  debugPrintFloat(123.44444f); debugPrint(" ");
  debugPrintFloat(123.45555f); debugPrint(" ");
  debugPrint("\n");
#endif

  // Workaround for nxdk-sdl bug?
  //FIXME: Why is this necessary?
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

  debugPrint("\n\n");
  debugPrint("\n\n");
  debugPrint("\n\n");
  debugPrint("\n\n");

  //FIXME: Bump GPU in right state?

  glFrontFace(GL_CW);

}
