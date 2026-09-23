/* Copyright © 2026 CCP ehf.
 *
 * pynr.h -- pynoesis render. The C ABI for rendering between pynoesis and the renderer
 * that hosts it, in both directions: the host's render device and frame context, which
 * the library draws through, and the library's view and shader source, which the host
 * drives. Shipped as the pynoesis-render package.
 *
 * pynoesis builds `_noesis`: a Python extension module that owns the NoesisGUI
 * SDK, the view, the data model and the Noesis render device. It does not link Trinity
 * or TrinityAL. The host -- carbon-trinity -- implements the GPU side of this header
 * over TrinityAL and keeps two thin adapter classes that drive the per-frame sequence.
 *
 * NOTHING IN HERE IS A GLOBAL, AND NEITHER MODULE LINKS THE OTHER. Every interface below
 * is a struct of function pointers, and every one of them arrives as an argument. Neither
 * side looks the other up, loads the other's binary, or resolves a symbol from it: the
 * only C symbol either module exports is its Python entry point.
 *
 * Interfaces cross as PyCapsule. Both modules are CPython extensions, so a capsule is a
 * transport both can build and read with nothing but Python.h -- no shared C++ runtime,
 * no shared object system, no third library either has to agree with. The capsule's
 * pointer is the interface struct; its name is the interface and its major version. See
 * the capsule names and the ownership rules below.
 *
 * Python owns the wiring, which is where the lifecycle already lives -- it creates the
 * views and builds the render jobs. This header plus the capsule-name convention is the
 * entire contract: two modules that agree on it need agree on nothing else.
 *
 * Deliberately mirrored, not shared: the structs below are our own, not the SDK's, even
 * where they currently match field for field. The library translates at the boundary.
 * That keeps a NoesisGUI SDK upgrade -- including one that reshuffles Noesis::Batch --
 * inside pynoesis instead of breaking the host.
 *
 * C ABI rules: C linkage, no C++ types, no exceptions across the boundary, no ownership
 * transfer except where a comment says so.
 */

#ifndef PYNR_H
#define PYNR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Versioning                                                                 */
/* ------------------------------------------------------------------------- */

/* Bumped when an existing entry changes meaning, a struct changes layout, or a vtable
 * slot is removed or reordered. Both sides must agree on major. */
#define PYNR_ABI_VERSION_MAJOR 1

/* Bumped when slots are appended to the end of a vtable. Informational: `struct_size` is
 * what compatibility is actually decided on, because it measures the same thing directly.
 * A receiver built against a lower minor than the sender is fine -- it reads the prefix it
 * knows, and struct_size proves the prefix is there. The reverse is what gets refused, and
 * struct_size catches it whether or not the sender remembered to bump this. */
#define PYNR_ABI_VERSION_MINOR 0

#if defined(__cplusplus)
#define PYNR_INLINE inline
#elif defined(_MSC_VER)
#define PYNR_INLINE __inline
#else
#define PYNR_INLINE inline
#endif

typedef int32_t pynr_bool;
#define PYNR_FALSE 0
#define PYNR_TRUE  1

typedef enum pynr_result
{
    PYNR_OK = 0,
    PYNR_ERROR_ABI_MISMATCH = 1, /* major differs, or the sender has fewer slots than
                                  * this build needs */
    PYNR_ERROR_ALREADY_INIT = 2,
    PYNR_ERROR_NOT_INIT     = 3,
    PYNR_ERROR_INVALID_ARG  = 4,
    PYNR_ERROR_SDK_INIT     = 5,
    PYNR_ERROR_INTERNAL     = 6
} pynr_result;

/* Every vtable in this header begins with one of these.
 *
 * It is first so that a receiver can read the version out of an interface whose layout
 * it does not otherwise understand -- which is exactly the case where it most needs to.
 * `struct_size` is the sizeof of the containing struct as the sender built it, so a
 * receiver can tell an appended slot from a truncated one rather than reading past the
 * end of a struct that is genuinely shorter than its own copy of the declaration. */
typedef struct pynr_interface_header
{
    uint32_t abi_version_major;
    uint32_t abi_version_minor;
    uint32_t struct_size;

    /* The instance every call below belongs to. Passed back as the first argument. */
    void* self;

    /* Lifetime, for interfaces that have one. See the ownership rules below.
     *
     * Null on a BORROWED interface -- one valid only for the call that handed it over.
     * Null is not "refcounting is optional": it means retaining is a contract violation,
     * because there is nothing behind the pointer to keep alive past the call. */
    void (*retain)(void* self);
    void (*release)(void* self);
} pynr_interface_header;

/* True when an interface can be called at all: same major, and at least as many bytes as
 * the declaration this translation unit was built against. Both sides check on receipt
 * rather than trusting the sender.
 *
 * `expected_size` is the sizeof of the CONCRETE interface -- pynr_render_device, pynr_view,
 * pynr_shader_source -- not of the header. Checking against the header alone would accept a
 * sender that stopped after four slots, which is the case this exists to catch; use
 * PYNR_INTERFACE_USABLE below and the size comes from the pointer's own type.
 *
 * Shorter than our declaration means the sender genuinely has fewer slots, and reading our
 * full struct would run off the end of theirs. Longer means the sender appended slots this
 * build does not know about: everything this build calls is present and in place, so that
 * is accepted, which is what makes appending a slot a minor bump rather than a major one.
 *
 * Inline rather than exported, because the check has to work before either side is
 * willing to call into the other -- which is the whole point of it. */
static PYNR_INLINE pynr_bool pynr_interface_usable( const pynr_interface_header* header,
                                                    size_t expected_size )
{
    if( header == NULL || header->self == NULL )
    {
        return PYNR_FALSE;
    }
    if( header->abi_version_major != PYNR_ABI_VERSION_MAJOR )
    {
        return PYNR_FALSE;
    }
    return header->struct_size >= expected_size ? PYNR_TRUE : PYNR_FALSE;
}

/* The form to use at a call site: takes a pointer to an interface and derives the size
 * from its type, so the size cannot disagree with the header being checked. */
#define PYNR_INTERFACE_USABLE( iface ) \
    pynr_interface_usable( &( iface )->header, sizeof( *( iface ) ) )

/* ------------------------------------------------------------------------- */
/* Capsule names                                                              */
/* ------------------------------------------------------------------------- */

/* An interface crosses Python as a PyCapsule whose pointer is the struct and whose name
 * is one of these. The name carries the major version, so a mismatched pair is refused by
 * PyCapsule_GetPointer before a single field is read. The header check that follows is
 * the real one; this is a cheap first gate that keeps a wrong capsule from getting far
 * enough to need it.
 *
 * Bump the vN suffix with PYNR_ABI_VERSION_MAJOR, and only then. */
#define PYNR_CAPSULE_RENDER_DEVICE "pynr.render_device.v1"
#define PYNR_CAPSULE_SHADER_SOURCE "pynr.shader_source.v1"
#define PYNR_CAPSULE_VIEW          "pynr.view.v1"

/* pynr_frame_context has no capsule: it is a parameter to the view render calls and never
 * crosses Python. */

/* ------------------------------------------------------------------------- */
/* Ownership                                                                  */
/* ------------------------------------------------------------------------- */

/* An interface arrives BORROWED. Use it for the duration of the call that handed it to
 * you, and no longer.
 *
 * To keep it past that call, retain it; release it when you drop it. A receiver that
 * stores an interface without retaining it has a use-after-free, not a race: the capsule
 * that delivered it is a Python object and can be collected the moment Python stops
 * naming it.
 *
 * The capsule holds the creator's reference and releases it when destroyed. A capsule
 * dying while a receiver holds a retained reference is normal and safe -- the object
 * outlives the capsule.
 *
 * Retain and release are the only calls that need no GIL, and the only ones a render
 * thread may make. In practice it should make neither: retain when Python wires, release
 * when Python unwires, and on the render path hold nothing but the pointer already
 * retained.
 *
 *   pynr_render_device  owned     host-created, retained by the library for its lifetime
 *   pynr_shader_source  owned     library-created, retained by the host's render device
 *   pynr_view           owned     library-created, retained by the host's render step
 *   pynr_frame_context  BORROWED  valid only inside the pynr_view call it is passed to
 *
 * The frame host is why borrowing is spelled out rather than assumed. It points at state
 * the host rebuilds every frame, so storing one and using it later reaches a render
 * context the host has finished with. Its retain and release are null rather than no-ops
 * so that the mistake fails at the call instead of silently.
 *
 * A receiver that caches anything DERIVED from an interface -- a pointer into it, a field
 * copied out of it -- owns that cache's correctness. Retaining keeps the object alive; it
 * does not keep a derived value in step with a later re-wiring. Prefer holding the
 * retained interface pointer and reading through it. */

/* ------------------------------------------------------------------------- */
/* Opaque handles                                                             */
/* ------------------------------------------------------------------------- */

/* GPU resources. Owned by the HOST -- created through pynr_render_device and released
 * through it. The library only ever passes them back. */
typedef struct pynr_texture_t* pynr_texture;
typedef struct pynr_render_target_t* pynr_render_target;

/* A compiled custom pixel shader, from pynr_render_device::create_pixel_shader.
 * Host-owned; travels in pynr_batch::pixel_shader. */
typedef void* pynr_pixel_shader;

/* ------------------------------------------------------------------------- */
/* Mirrored render types                                                      */
/* ------------------------------------------------------------------------- */

typedef enum pynr_texture_format
{
    PYNR_TEXTURE_FORMAT_RGBA8 = 0,
    PYNR_TEXTURE_FORMAT_RGBX8 = 1,
    PYNR_TEXTURE_FORMAT_R8    = 2
} pynr_texture_format;

typedef enum pynr_shader_stage
{
    PYNR_SHADER_STAGE_VERTEX = 0,
    PYNR_SHADER_STAGE_PIXEL  = 1
} pynr_shader_stage;

typedef struct pynr_device_caps
{
    pynr_bool linear_rendering;
    pynr_bool subpixel_rendering;
    pynr_bool depth_range_zero_to_one;
    pynr_bool clip_space_y_inverted;
} pynr_device_caps;

/* Packed bitfields, one byte each, matching the SDK unions by value. The library
 * translates; the host reads the bits documented here.
 *
 * render_state:  bit 0     colorEnable
 *                bits 1-3  blendMode
 *                bits 4-6  stencilMode
 *                bit 7     wireframe
 *
 * sampler:       bits 0-2  wrapMode
 *                bit 3     minmagFilter
 *                bits 4-5  mipFilter
 *                bits 6-7  unused
 */
typedef uint8_t pynr_render_state;
typedef uint8_t pynr_sampler_state;

/* The values the render state bits take, for the same reason as the sampler ones below:
 * a documented layout without documented values leaves the host guessing what a blend
 * mode of 3 means, and guessing wrong blends wrongly rather than failing. */
typedef enum pynr_blend_mode
{
    PYNR_BLEND_SRC              = 0,
    PYNR_BLEND_SRC_OVER         = 1,
    PYNR_BLEND_SRC_OVER_MULTIPLY = 2,
    PYNR_BLEND_SRC_OVER_SCREEN  = 3,
    PYNR_BLEND_SRC_OVER_ADDITIVE = 4,
    PYNR_BLEND_SRC_OVER_DUAL    = 5
} pynr_blend_mode;

typedef enum pynr_stencil_mode
{
    PYNR_STENCIL_DISABLED   = 0,
    PYNR_STENCIL_EQUAL_KEEP = 1,
    PYNR_STENCIL_EQUAL_INCR = 2,
    PYNR_STENCIL_EQUAL_DECR = 3,
    PYNR_STENCIL_CLEAR      = 4,
    PYNR_STENCIL_DISABLED_ZTEST   = 5,
    PYNR_STENCIL_EQUAL_KEEP_ZTEST = 6
} pynr_stencil_mode;

static PYNR_INLINE pynr_bool pynr_render_state_color_enable( pynr_render_state s )
{
    return ( s & 0x1 ) != 0 ? PYNR_TRUE : PYNR_FALSE;
}
static PYNR_INLINE pynr_blend_mode pynr_render_state_blend_mode( pynr_render_state s )
{
    return (pynr_blend_mode)( ( s >> 1 ) & 0x7 );
}
static PYNR_INLINE pynr_stencil_mode pynr_render_state_stencil_mode( pynr_render_state s )
{
    return (pynr_stencil_mode)( ( s >> 4 ) & 0x7 );
}
static PYNR_INLINE pynr_bool pynr_render_state_wireframe( pynr_render_state s )
{
    return ( s & 0x80 ) != 0 ? PYNR_TRUE : PYNR_FALSE;
}

/* The values those sampler bits take. Documenting the layout without the values would
 * leave the host guessing what a wrap mode of 3 means, and guessing wrong produces
 * wrong sampling rather than an error. */
typedef enum pynr_wrap_mode
{
    PYNR_WRAP_CLAMP_TO_EDGE = 0,
    PYNR_WRAP_CLAMP_TO_ZERO = 1,
    PYNR_WRAP_REPEAT        = 2,
    PYNR_WRAP_MIRROR_U      = 3,
    PYNR_WRAP_MIRROR_V      = 4,
    PYNR_WRAP_MIRROR        = 5
} pynr_wrap_mode;

typedef enum pynr_minmag_filter
{
    PYNR_MINMAG_NEAREST = 0,
    PYNR_MINMAG_LINEAR  = 1
} pynr_minmag_filter;

typedef enum pynr_mip_filter
{
    PYNR_MIP_DISABLED = 0,
    PYNR_MIP_NEAREST  = 1,
    PYNR_MIP_LINEAR   = 2
} pynr_mip_filter;

/* Unpacking helpers, inline so the host does not re-derive the shifts from the comment
 * above and get one of them wrong. */
static PYNR_INLINE pynr_wrap_mode pynr_sampler_wrap_mode( pynr_sampler_state s )
{
    return (pynr_wrap_mode)( s & 0x7 );
}
static PYNR_INLINE pynr_minmag_filter pynr_sampler_minmag_filter( pynr_sampler_state s )
{
    return (pynr_minmag_filter)( ( s >> 3 ) & 0x1 );
}
static PYNR_INLINE pynr_mip_filter pynr_sampler_mip_filter( pynr_sampler_state s )
{
    return (pynr_mip_filter)( ( s >> 4 ) & 0x3 );
}

/* Uniform buffer contents. `values` points into library-owned scratch valid only for the
 * duration of the draw_batch call. `hash` lets the host skip redundant updates. */
typedef struct pynr_uniform_data
{
    const void* values;
    uint32_t    num_dwords;
    uint32_t    hash;
} pynr_uniform_data;

/* One indexed triangle list. vertex_offset is a byte offset into the buffer last
 * returned by map_vertices; start_index counts 16-bit indices into the buffer last
 * returned by map_indices. Unused textures are null. */
typedef struct pynr_batch
{
    /* The pixel shader for this batch, indexing the PYNR_SHADER_STAGE_PIXEL table. */
    uint8_t           shader;

    /* The vertex shader and vertex format that go with it, already resolved.
     *
     * The SDK keeps these as lookup tables beside the shader enum, and the library has
     * them; the host does not and must not need them. Sending the answer costs two bytes
     * and removes the host's only remaining reason to know anything about the SDK's
     * shader taxonomy. vertex_shader indexes the PYNR_SHADER_STAGE_VERTEX table;
     * vertex_format indexes the formats pynr_shader_source describes. */
    uint8_t           vertex_shader;
    uint8_t           vertex_format;

    pynr_render_state render_state;
    uint8_t           stencil_ref;
    pynr_bool         single_pass_stereo;

    uint32_t vertex_offset;
    uint32_t num_vertices;
    uint32_t start_index;
    uint32_t num_indices;

    pynr_texture pattern;
    pynr_texture ramps;
    pynr_texture image;
    pynr_texture glyphs;
    pynr_texture shadow;

    pynr_sampler_state pattern_sampler;
    pynr_sampler_state ramps_sampler;
    pynr_sampler_state image_sampler;
    pynr_sampler_state glyphs_sampler;
    pynr_sampler_state shadow_sampler;

    pynr_uniform_data vertex_uniforms[2];
    pynr_uniform_data pixel_uniforms[2];

    pynr_pixel_shader pixel_shader; /* null unless a custom effect is active */
} pynr_batch;

/* A region of the render target. Origin is the LOWER LEFT corner, as the SDK defines it;
 * the host flips to its own convention. */
typedef struct pynr_tile
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} pynr_tile;

/* ------------------------------------------------------------------------- */
/* Shader blobs -- provided by the library, consumed by the host              */
/* ------------------------------------------------------------------------- */

/* The permutation set and the HLSL are SDK details, so the library compiles them and the
 * host only ever sees bytecode. An SDK upgrade that adds or renames a permutation is a
 * change in pynoesis alone.
 *
 * `id` indexes the stage's own table: for PYNR_SHADER_STAGE_PIXEL it is the value the
 * host sees in pynr_batch::shader. `name` is for logging and GPU markers only.
 *
 * Bytecode is platform-specific but not backend-specific: DXBC serves dx11 and dx12
 * alike, AIR covers metal.
 *
 * VERTEX INPUT SEMANTICS ARE PART OF THIS CONTRACT. The SDK's HLSL declares COVERAGE,
 * RECT, TILE and IMAGE_POSITION as semantic names; these blobs are compiled with them
 * renamed to TEXCOORD2, TEXCOORD3, TEXCOORD4 and TEXCOORD5. An input layout must match
 * the compiled signature exactly, so a host building vertex layouts has to use those
 * names. Stated here rather than left as an accident of how the blobs are built. */
/* Which constant buffers and textures a shader binds, so the host can build a root
 * signature or its equivalent without knowing what the shader does. The texture bits
 * line up with the texture slots in pynr_batch.
 *
 * This is SDK knowledge, which is why it travels with the blob: a host that had to
 * maintain its own table would have to revisit it on every SDK upgrade, and a stale
 * entry binds the wrong resources rather than failing. */
#define PYNR_SHADER_USES_VS_CB0 (1u << 0)
#define PYNR_SHADER_USES_VS_CB1 (1u << 1)
#define PYNR_SHADER_USES_PS_CB0 (1u << 2)
#define PYNR_SHADER_USES_PS_CB1 (1u << 3)
#define PYNR_SHADER_USES_PS_T0  (1u << 4) /* pattern */
#define PYNR_SHADER_USES_PS_T1  (1u << 5) /* ramps   */
#define PYNR_SHADER_USES_PS_T2  (1u << 6) /* image   */
#define PYNR_SHADER_USES_PS_T3  (1u << 7) /* glyphs  */
#define PYNR_SHADER_USES_PS_T4  (1u << 8) /* shadow  */

typedef struct pynr_shader_blob
{
    pynr_shader_stage stage;
    uint8_t           id;
    const char*       name;
    const void*       bytecode; /* library-owned, valid for the process lifetime */
    uint32_t          size;

    /* PYNR_SHADER_USES_* for this shader. A vertex blob is not flag-free: every one binds
     * a constant buffer, and the SDF variants bind a second. Zero only for the
     * custom-effect slot, where the effect supplies the shader and nothing is known in
     * advance. */
    uint32_t          resource_flags;

    /* Which vertex shader and vertex format this blob involves, so a blob is enough on
     * its own to build a pipeline for it and the host never needs a lookup table.
     *
     * For a pixel blob, `vertex_shader` is the stock vertex shader it pairs with. For a
     * vertex blob it is the blob's own id. Either way `vertex_format` is the format that
     * vertex shader consumes, and indexes the formats get_vertex_format describes. */
    uint8_t           vertex_shader;
    uint8_t           vertex_format;
} pynr_shader_blob;

/* ------------------------------------------------------------------------- */
/* Vertex formats -- provided by the library, consumed by the host            */
/* ------------------------------------------------------------------------- */

typedef enum pynr_vertex_attr_type
{
    PYNR_VERTEX_ATTR_FLOAT         = 0,
    PYNR_VERTEX_ATTR_FLOAT2        = 1,
    PYNR_VERTEX_ATTR_FLOAT4        = 2,
    PYNR_VERTEX_ATTR_UBYTE4_NORM   = 3,
    PYNR_VERTEX_ATTR_USHORT4_NORM  = 4
} pynr_vertex_attr_type;

/* One attribute of a vertex format, in declaration order.
 *
 * `semantic` and `semantic_index` are the HLSL semantic the blobs were actually compiled
 * against, including the renames described on pynr_shader_blob -- so a host that builds
 * its input layout from these is matching the compiled signature by construction rather
 * than by remembering to apply the same renames. */
typedef struct pynr_vertex_attribute
{
    pynr_vertex_attr_type type;
    const char*           semantic;       /* "POSITION", "COLOR", "TEXCOORD" */
    uint32_t              semantic_index;
} pynr_vertex_attribute;

/* Created by the library, delivered as PYNR_CAPSULE_SHADER_SOURCE. The host retains one
 * when building its device, so it has the bytecode and the layouts before it is asked to
 * draw anything. */
typedef struct pynr_shader_source
{
    pynr_interface_header header;

    uint32_t (*get_count)(void* self);
    pynr_result (*get_blob)(void* self, uint32_t index, pynr_shader_blob* out_blob);

    /* How many vertex formats pynr_batch::vertex_format can name. */
    uint32_t (*get_vertex_format_count)(void* self);

    /* Fills `out` with the attributes of one format, in declaration order, and returns
     * how many there are. Returns the count needed and writes nothing when `capacity` is
     * too small, so a caller can size a buffer without a second entry point. */
    uint32_t (*get_vertex_format)(void* self, uint32_t format,
                                  pynr_vertex_attribute* out, uint32_t capacity);
} pynr_shader_source;

/* ------------------------------------------------------------------------- */
/* Device host -- provided by the host, consumed by the library               */
/* ------------------------------------------------------------------------- */

/* Long-lived GPU resources, and nothing that needs a frame in progress. Trinity creates
 * one of these over TrinityAL; Python hands it to the library, which builds a Noesis
 * render device around it.
 *
 * Split from pynr_frame_context deliberately. These calls reach the primary context and are
 * valid whenever the device is alive; the frame ones are only valid inside a frame. Two
 * objects makes that difference checkable rather than a rule in a comment.
 *
 * Because they carry no frame, they can arrive before the host has drawn anything -- and
 * a host that builds its GPU device lazily may not have one yet. Returning null for a
 * resource it cannot create is expected and handled; the library treats it as a failed
 * creation. Do not assume a first call arrives inside a frame. */
typedef struct pynr_render_device
{
    pynr_interface_header header;

    void (*get_caps)(void* self, pynr_device_caps* out_caps);

    pynr_render_target (*create_render_target)(void* self, const char* label,
                                               uint32_t width, uint32_t height,
                                               uint32_t sample_count, pynr_bool needs_stencil);
    pynr_render_target (*clone_render_target)(void* self, const char* label,
                                              pynr_render_target surface);
    /* Releases the target only, never its colour texture: that is a separate handle the
     * library took from get_render_target_texture and releases itself, with
     * release_texture, after this call returns. A host that frees the colour texture here
     * double-frees it. */
    void (*release_render_target)(void* self, pynr_render_target surface);

    /* The colour texture of a render target, so the library can answer the SDK's
     * RenderTarget::GetTexture. A separate handle with its own lifetime -- see
     * release_render_target. */
    pynr_texture (*get_render_target_texture)(void* self, pynr_render_target surface);

    /* `data` is an array of num_levels pointers, or null for an empty texture. */
    pynr_texture (*create_texture)(void* self, const char* label,
                                   uint32_t width, uint32_t height,
                                   uint32_t num_levels, pynr_texture_format format,
                                   const void** data);

    /* "Drop this handle", not "destroy the GPU resource". The library releases every
     * handle it is given, including ones from wrap_native_texture that refer to a
     * resource the host owns and must keep. What a release means belongs on the host
     * side, where the ownership is known. */
    void (*release_texture)(void* self, pynr_texture texture);

    void (*get_texture_info)(void* self, pynr_texture texture,
                             uint32_t* out_width, uint32_t* out_height,
                             uint32_t* out_levels, pynr_bool* out_has_alpha);

    /* Custom effects. The library forwards ShaderEffect::SetPixelShader and
     * BrushShader::SetPixelShader here; the handle comes back in pynr_batch::pixel_shader.
     * Null on failure, which the library treats as "no custom shader". */
    pynr_pixel_shader (*create_pixel_shader)(void* self, const char* label, uint8_t shader,
                                             const void* blob, uint32_t size);
    void (*release_pixel_shader)(void* self, pynr_pixel_shader shader);

    /* Wraps a texture the host already owns so XAML can sample it. Used by the video
     * sink.
     *
     * `native` is the PyObject the host's own Python handed to the sink, passed straight
     * through: the library never dereferences it and has no idea what it is. Unwrapping
     * it is the host's job, because only the host knows what it asked for. Called with
     * the GIL held. */
    pynr_texture (*wrap_native_texture)(void* self, void* native, pynr_bool has_alpha);

    /* Size of a host-owned texture without wrapping it, false when `native` is not a
     * usable texture. The video element needs the size on the thread that sets the
     * texture, which is not the render thread and must not create GPU resources; and it
     * is the one point where the host can tell a caller it passed the wrong object. */
    pynr_bool (*get_native_texture_size)(void* self, void* native,
                                         uint32_t* out_width, uint32_t* out_height);
} pynr_render_device;

/* ------------------------------------------------------------------------- */
/* Frame host -- provided by the host, consumed by the library                */
/* ------------------------------------------------------------------------- */

/* Everything that needs the frame's deferred context bound. The host hands one of these
 * to pynr_view::render for the duration of that call and no longer: holding on to it
 * past the frame is a use-after-free, which is why it is a parameter rather than state
 * and why its retain and release are null.
 *
 * There is no scissor entry here on purpose. The host knows the parent clip rect and
 * begin_onscreen_render is a host call, so both ends are already on its side; routing
 * the clip through this ABI would end where it started. */
typedef struct pynr_frame_context
{
    pynr_interface_header header;

    void (*update_texture)(void* self, pynr_texture texture, uint32_t level,
                           uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                           const void* data);

    void (*begin_offscreen_render)(void* self);
    void (*end_offscreen_render)(void* self);
    void (*begin_onscreen_render)(void* self);
    void (*end_onscreen_render)(void* self);

    void (*set_render_target)(void* self, pynr_render_target surface);
    void (*begin_tile)(void* self, pynr_render_target surface, const pynr_tile* tile);
    void (*end_tile)(void* self, pynr_render_target surface);
    void (*resolve_render_target)(void* self, pynr_render_target surface,
                                  const pynr_tile* tiles, uint32_t num_tiles);

    void* (*map_vertices)(void* self, uint32_t bytes);
    void (*unmap_vertices)(void* self);
    void* (*map_indices)(void* self, uint32_t bytes);
    void (*unmap_indices)(void* self);

    void (*draw_batch)(void* self, const pynr_batch* batch);
} pynr_frame_context;

/* ------------------------------------------------------------------------- */
/* View -- provided by the library, consumed by the host                      */
/* ------------------------------------------------------------------------- */

/* The per-frame sequence, in the order the SDK requires. The host drives it so it can
 * interleave its own viewport, render target and depth-stencil save and restore between
 * the offscreen and onscreen phases:
 *
 *   ensure_renderer(frame)
 *   sync_size(w, h)
 *   update(seconds)              absolute seconds, not a delta
 *   update_render_tree(frame)
 *   [host: push viewport / render target / depth-stencil]
 *   render_offscreen(frame)
 *   [host: pop depth-stencil / render target / viewport, apply its own clip]
 *   render(frame, flip_y, clear)
 *
 * Every call that can reach the GPU takes the frame host, so the library never holds
 * frame state between calls and the host cannot forget to tear it down.
 *
 * This sequence is checked, not just described: test/ drives it against a host written
 * in Python, which refuses a phase that nests, a draw outside one, and a buffer unmapped
 * before it was mapped. A host being brought up can run that harness against its own
 * understanding of the order before it has a GPU to be wrong on. */
typedef struct pynr_view
{
    pynr_interface_header header;

    pynr_bool (*is_loaded)(void* self);

    /* Brings up the SDK renderer on first use. Creates GPU resources, so the host must
     * call it inside its managed rendering bracket. False means this view cannot render
     * this frame; the host should skip it without logging per-frame. */
    pynr_bool (*ensure_renderer)(void* self, const pynr_frame_context* frame);

    void (*sync_size)(void* self, uint32_t width, uint32_t height);
    void (*update)(void* self, double seconds);

    void (*update_render_tree)(void* self, const pynr_frame_context* frame);
    pynr_bool (*render_offscreen)(void* self, const pynr_frame_context* frame);
    void (*render)(void* self, const pynr_frame_context* frame, pynr_bool flip_y, pynr_bool clear);
} pynr_view;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PYNR_H */
