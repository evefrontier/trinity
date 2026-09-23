/* Copyright © 2026 CCP ehf.
 *
 * nxt.h -- Noesis Host Interface. The C ABI between frontier-noesis and its rendering
 * host.
 *
 * frontier-noesis builds `_noesis`: a Python extension module that owns the NoesisGUI
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
 * inside frontier-noesis instead of breaking the host.
 *
 * C ABI rules: C linkage, no C++ types, no exceptions across the boundary, no ownership
 * transfer except where a comment says so.
 */

#ifndef NXT_H
#define NXT_H

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
#define NXT_ABI_VERSION_MAJOR 1

/* Bumped when slots are appended to the end of a vtable. Informational: `struct_size` is
 * what compatibility is actually decided on, because it measures the same thing directly.
 * A receiver built against a lower minor than the sender is fine -- it reads the prefix it
 * knows, and struct_size proves the prefix is there. The reverse is what gets refused, and
 * struct_size catches it whether or not the sender remembered to bump this. */
#define NXT_ABI_VERSION_MINOR 0

#if defined(__cplusplus)
#define NXT_INLINE inline
#elif defined(_MSC_VER)
#define NXT_INLINE __inline
#else
#define NXT_INLINE inline
#endif

typedef int32_t nxt_bool;
#define NXT_FALSE 0
#define NXT_TRUE  1

typedef enum nxt_result
{
    NXT_OK = 0,
    NXT_ERROR_ABI_MISMATCH = 1, /* major differs, or the sender has fewer slots than
                                 * this build needs */
    NXT_ERROR_ALREADY_INIT = 2,
    NXT_ERROR_NOT_INIT     = 3,
    NXT_ERROR_INVALID_ARG  = 4,
    NXT_ERROR_SDK_INIT     = 5,
    NXT_ERROR_INTERNAL     = 6
} nxt_result;

/* Every vtable in this header begins with one of these.
 *
 * It is first so that a receiver can read the version out of an interface whose layout
 * it does not otherwise understand -- which is exactly the case where it most needs to.
 * `struct_size` is the sizeof of the containing struct as the sender built it, so a
 * receiver can tell an appended slot from a truncated one rather than reading past the
 * end of a struct that is genuinely shorter than its own copy of the declaration. */
typedef struct nxt_interface_header
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
} nxt_interface_header;

/* True when an interface can be called at all: same major, and at least as many bytes as
 * the declaration this translation unit was built against. Both sides check on receipt
 * rather than trusting the sender.
 *
 * `expected_size` is the sizeof of the CONCRETE interface -- nxt_render_device, nxt_view,
 * nxt_shader_source -- not of the header. Checking against the header alone would accept a
 * sender that stopped after four slots, which is the case this exists to catch; use
 * NXT_INTERFACE_USABLE below and the size comes from the pointer's own type.
 *
 * Shorter than our declaration means the sender genuinely has fewer slots, and reading our
 * full struct would run off the end of theirs. Longer means the sender appended slots this
 * build does not know about: everything this build calls is present and in place, so that
 * is accepted, which is what makes appending a slot a minor bump rather than a major one.
 *
 * Inline rather than exported, because the check has to work before either side is
 * willing to call into the other -- which is the whole point of it. */
static NXT_INLINE nxt_bool nxt_interface_usable( const nxt_interface_header* header,
                                                 size_t expected_size )
{
    if( header == NULL || header->self == NULL )
    {
        return NXT_FALSE;
    }
    if( header->abi_version_major != NXT_ABI_VERSION_MAJOR )
    {
        return NXT_FALSE;
    }
    return header->struct_size >= expected_size ? NXT_TRUE : NXT_FALSE;
}

/* The form to use at a call site: takes a pointer to an interface and derives the size
 * from its type, so the size cannot disagree with the header being checked. */
#define NXT_INTERFACE_USABLE( iface ) \
    nxt_interface_usable( &( iface )->header, sizeof( *( iface ) ) )

/* ------------------------------------------------------------------------- */
/* Capsule names                                                              */
/* ------------------------------------------------------------------------- */

/* An interface crosses Python as a PyCapsule whose pointer is the struct and whose name
 * is one of these. The name carries the major version, so a mismatched pair is refused by
 * PyCapsule_GetPointer before a single field is read. The header check that follows is
 * the real one; this is a cheap first gate that keeps a wrong capsule from getting far
 * enough to need it.
 *
 * Bump the vN suffix with NXT_ABI_VERSION_MAJOR, and only then. */
#define NXT_CAPSULE_RENDER_DEVICE "nxt.render_device.v1"
#define NXT_CAPSULE_SHADER_SOURCE "nxt.shader_source.v1"
#define NXT_CAPSULE_VIEW          "nxt.view.v1"

/* nxt_frame_context has no capsule: it is a parameter to the view render calls and never
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
 *   nxt_render_device    owned     host-created, retained by the library for its lifetime
 *   nxt_shader_source  owned     library-created, retained by the host's render device
 *   nxt_view       owned     library-created, retained by the host's render step
 *   nxt_frame_context     BORROWED  valid only inside the nxt_view call it is passed to
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

/* GPU resources. Owned by the HOST -- created through nxt_render_device and released
 * through it. The library only ever passes them back. */
typedef struct nxt_texture_t* nxt_texture;
typedef struct nxt_render_target_t* nxt_render_target;

/* A compiled custom pixel shader, from nxt_render_device::create_pixel_shader.
 * Host-owned; travels in nxt_batch::pixel_shader. */
typedef void* nxt_pixel_shader;

/* ------------------------------------------------------------------------- */
/* Mirrored render types                                                      */
/* ------------------------------------------------------------------------- */

typedef enum nxt_texture_format
{
    NXT_TEXTURE_FORMAT_RGBA8 = 0,
    NXT_TEXTURE_FORMAT_RGBX8 = 1,
    NXT_TEXTURE_FORMAT_R8    = 2
} nxt_texture_format;

typedef enum nxt_shader_stage
{
    NXT_SHADER_STAGE_VERTEX = 0,
    NXT_SHADER_STAGE_PIXEL  = 1
} nxt_shader_stage;

typedef struct nxt_device_caps
{
    nxt_bool linear_rendering;
    nxt_bool subpixel_rendering;
    nxt_bool depth_range_zero_to_one;
    nxt_bool clip_space_y_inverted;
} nxt_device_caps;

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
typedef uint8_t nxt_render_state;
typedef uint8_t nxt_sampler_state;

/* The values the render state bits take, for the same reason as the sampler ones below:
 * a documented layout without documented values leaves the host guessing what a blend
 * mode of 3 means, and guessing wrong blends wrongly rather than failing. */
typedef enum nxt_blend_mode
{
    NXT_BLEND_SRC              = 0,
    NXT_BLEND_SRC_OVER         = 1,
    NXT_BLEND_SRC_OVER_MULTIPLY = 2,
    NXT_BLEND_SRC_OVER_SCREEN  = 3,
    NXT_BLEND_SRC_OVER_ADDITIVE = 4,
    NXT_BLEND_SRC_OVER_DUAL    = 5
} nxt_blend_mode;

typedef enum nxt_stencil_mode
{
    NXT_STENCIL_DISABLED   = 0,
    NXT_STENCIL_EQUAL_KEEP = 1,
    NXT_STENCIL_EQUAL_INCR = 2,
    NXT_STENCIL_EQUAL_DECR = 3,
    NXT_STENCIL_CLEAR      = 4,
    NXT_STENCIL_DISABLED_ZTEST   = 5,
    NXT_STENCIL_EQUAL_KEEP_ZTEST = 6
} nxt_stencil_mode;

static NXT_INLINE nxt_bool nxt_render_state_color_enable( nxt_render_state s )
{
    return ( s & 0x1 ) != 0 ? NXT_TRUE : NXT_FALSE;
}
static NXT_INLINE nxt_blend_mode nxt_render_state_blend_mode( nxt_render_state s )
{
    return (nxt_blend_mode)( ( s >> 1 ) & 0x7 );
}
static NXT_INLINE nxt_stencil_mode nxt_render_state_stencil_mode( nxt_render_state s )
{
    return (nxt_stencil_mode)( ( s >> 4 ) & 0x7 );
}
static NXT_INLINE nxt_bool nxt_render_state_wireframe( nxt_render_state s )
{
    return ( s & 0x80 ) != 0 ? NXT_TRUE : NXT_FALSE;
}

/* The values those sampler bits take. Documenting the layout without the values would
 * leave the host guessing what a wrap mode of 3 means, and guessing wrong produces
 * wrong sampling rather than an error. */
typedef enum nxt_wrap_mode
{
    NXT_WRAP_CLAMP_TO_EDGE = 0,
    NXT_WRAP_CLAMP_TO_ZERO = 1,
    NXT_WRAP_REPEAT        = 2,
    NXT_WRAP_MIRROR_U      = 3,
    NXT_WRAP_MIRROR_V      = 4,
    NXT_WRAP_MIRROR        = 5
} nxt_wrap_mode;

typedef enum nxt_minmag_filter
{
    NXT_MINMAG_NEAREST = 0,
    NXT_MINMAG_LINEAR  = 1
} nxt_minmag_filter;

typedef enum nxt_mip_filter
{
    NXT_MIP_DISABLED = 0,
    NXT_MIP_NEAREST  = 1,
    NXT_MIP_LINEAR   = 2
} nxt_mip_filter;

/* Unpacking helpers, inline so the host does not re-derive the shifts from the comment
 * above and get one of them wrong. */
static NXT_INLINE nxt_wrap_mode nxt_sampler_wrap_mode( nxt_sampler_state s )
{
    return (nxt_wrap_mode)( s & 0x7 );
}
static NXT_INLINE nxt_minmag_filter nxt_sampler_minmag_filter( nxt_sampler_state s )
{
    return (nxt_minmag_filter)( ( s >> 3 ) & 0x1 );
}
static NXT_INLINE nxt_mip_filter nxt_sampler_mip_filter( nxt_sampler_state s )
{
    return (nxt_mip_filter)( ( s >> 4 ) & 0x3 );
}

/* Uniform buffer contents. `values` points into library-owned scratch valid only for the
 * duration of the draw_batch call. `hash` lets the host skip redundant updates. */
typedef struct nxt_uniform_data
{
    const void* values;
    uint32_t    num_dwords;
    uint32_t    hash;
} nxt_uniform_data;

/* One indexed triangle list. vertex_offset is a byte offset into the buffer last
 * returned by map_vertices; start_index counts 16-bit indices into the buffer last
 * returned by map_indices. Unused textures are null. */
typedef struct nxt_batch
{
    /* The pixel shader for this batch, indexing the NXT_SHADER_STAGE_PIXEL table. */
    uint8_t          shader;

    /* The vertex shader and vertex format that go with it, already resolved.
     *
     * The SDK keeps these as lookup tables beside the shader enum, and the library has
     * them; the host does not and must not need them. Sending the answer costs two bytes
     * and removes the host's only remaining reason to know anything about the SDK's
     * shader taxonomy. vertex_shader indexes the NXT_SHADER_STAGE_VERTEX table;
     * vertex_format indexes the formats nxt_shader_source describes. */
    uint8_t          vertex_shader;
    uint8_t          vertex_format;

    nxt_render_state render_state;
    uint8_t          stencil_ref;
    nxt_bool         single_pass_stereo;

    uint32_t vertex_offset;
    uint32_t num_vertices;
    uint32_t start_index;
    uint32_t num_indices;

    nxt_texture pattern;
    nxt_texture ramps;
    nxt_texture image;
    nxt_texture glyphs;
    nxt_texture shadow;

    nxt_sampler_state pattern_sampler;
    nxt_sampler_state ramps_sampler;
    nxt_sampler_state image_sampler;
    nxt_sampler_state glyphs_sampler;
    nxt_sampler_state shadow_sampler;

    nxt_uniform_data vertex_uniforms[2];
    nxt_uniform_data pixel_uniforms[2];

    nxt_pixel_shader pixel_shader; /* null unless a custom effect is active */
} nxt_batch;

/* A region of the render target. Origin is the LOWER LEFT corner, as the SDK defines it;
 * the host flips to its own convention. */
typedef struct nxt_tile
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} nxt_tile;

/* ------------------------------------------------------------------------- */
/* Shader blobs -- provided by the library, consumed by the host              */
/* ------------------------------------------------------------------------- */

/* The permutation set and the HLSL are SDK details, so the library compiles them and the
 * host only ever sees bytecode. An SDK upgrade that adds or renames a permutation is a
 * change in frontier-noesis alone.
 *
 * `id` indexes the stage's own table: for NXT_SHADER_STAGE_PIXEL it is the value the
 * host sees in nxt_batch::shader. `name` is for logging and GPU markers only.
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
 * line up with the texture slots in nxt_batch.
 *
 * This is SDK knowledge, which is why it travels with the blob: a host that had to
 * maintain its own table would have to revisit it on every SDK upgrade, and a stale
 * entry binds the wrong resources rather than failing. */
#define NXT_SHADER_USES_VS_CB0 (1u << 0)
#define NXT_SHADER_USES_VS_CB1 (1u << 1)
#define NXT_SHADER_USES_PS_CB0 (1u << 2)
#define NXT_SHADER_USES_PS_CB1 (1u << 3)
#define NXT_SHADER_USES_PS_T0  (1u << 4) /* pattern */
#define NXT_SHADER_USES_PS_T1  (1u << 5) /* ramps   */
#define NXT_SHADER_USES_PS_T2  (1u << 6) /* image   */
#define NXT_SHADER_USES_PS_T3  (1u << 7) /* glyphs  */
#define NXT_SHADER_USES_PS_T4  (1u << 8) /* shadow  */

typedef struct nxt_shader_blob
{
    nxt_shader_stage stage;
    uint8_t          id;
    const char*      name;
    const void*      bytecode; /* library-owned, valid for the process lifetime */
    uint32_t         size;

    /* NXT_SHADER_USES_* for this shader. A vertex blob is not flag-free: every one binds
     * a constant buffer, and the SDF variants bind a second. Zero only for the
     * custom-effect slot, where the effect supplies the shader and nothing is known in
     * advance. */
    uint32_t         resource_flags;

    /* Which vertex shader and vertex format this blob involves, so a blob is enough on
     * its own to build a pipeline for it and the host never needs a lookup table.
     *
     * For a pixel blob, `vertex_shader` is the stock vertex shader it pairs with. For a
     * vertex blob it is the blob's own id. Either way `vertex_format` is the format that
     * vertex shader consumes, and indexes the formats get_vertex_format describes. */
    uint8_t          vertex_shader;
    uint8_t          vertex_format;
} nxt_shader_blob;

/* ------------------------------------------------------------------------- */
/* Vertex formats -- provided by the library, consumed by the host            */
/* ------------------------------------------------------------------------- */

typedef enum nxt_vertex_attr_type
{
    NXT_VERTEX_ATTR_FLOAT         = 0,
    NXT_VERTEX_ATTR_FLOAT2        = 1,
    NXT_VERTEX_ATTR_FLOAT4        = 2,
    NXT_VERTEX_ATTR_UBYTE4_NORM   = 3,
    NXT_VERTEX_ATTR_USHORT4_NORM  = 4
} nxt_vertex_attr_type;

/* One attribute of a vertex format, in declaration order.
 *
 * `semantic` and `semantic_index` are the HLSL semantic the blobs were actually compiled
 * against, including the renames described on nxt_shader_blob -- so a host that builds
 * its input layout from these is matching the compiled signature by construction rather
 * than by remembering to apply the same renames. */
typedef struct nxt_vertex_attribute
{
    nxt_vertex_attr_type type;
    const char*          semantic;       /* "POSITION", "COLOR", "TEXCOORD" */
    uint32_t             semantic_index;
} nxt_vertex_attribute;

/* Created by the library, delivered as NXT_CAPSULE_SHADER_SOURCE. The host retains one
 * when building its device, so it has the bytecode and the layouts before it is asked to
 * draw anything. */
typedef struct nxt_shader_source
{
    nxt_interface_header header;

    uint32_t (*get_count)(void* self);
    nxt_result (*get_blob)(void* self, uint32_t index, nxt_shader_blob* out_blob);

    /* How many vertex formats nxt_batch::vertex_format can name. */
    uint32_t (*get_vertex_format_count)(void* self);

    /* Fills `out` with the attributes of one format, in declaration order, and returns
     * how many there are. Returns the count needed and writes nothing when `capacity` is
     * too small, so a caller can size a buffer without a second entry point. */
    uint32_t (*get_vertex_format)(void* self, uint32_t format,
                                  nxt_vertex_attribute* out, uint32_t capacity);
} nxt_shader_source;

/* ------------------------------------------------------------------------- */
/* Device host -- provided by the host, consumed by the library               */
/* ------------------------------------------------------------------------- */

/* Long-lived GPU resources, and nothing that needs a frame in progress. Trinity creates
 * one of these over TrinityAL; Python hands it to the library, which builds a Noesis
 * render device around it.
 *
 * Split from nxt_frame_context deliberately. These calls reach the primary context and are
 * valid whenever the device is alive; the frame ones are only valid inside a frame. Two
 * objects makes that difference checkable rather than a rule in a comment.
 *
 * Because they carry no frame, they can arrive before the host has drawn anything -- and
 * a host that builds its GPU device lazily may not have one yet. Returning null for a
 * resource it cannot create is expected and handled; the library treats it as a failed
 * creation. Do not assume a first call arrives inside a frame. */
typedef struct nxt_render_device
{
    nxt_interface_header header;

    void (*get_caps)(void* self, nxt_device_caps* out_caps);

    nxt_render_target (*create_render_target)(void* self, const char* label,
                                              uint32_t width, uint32_t height,
                                              uint32_t sample_count, nxt_bool needs_stencil);
    nxt_render_target (*clone_render_target)(void* self, const char* label,
                                             nxt_render_target surface);
    /* Releases the target only, never its colour texture: that is a separate handle the
     * library took from get_render_target_texture and releases itself, with
     * release_texture, after this call returns. A host that frees the colour texture here
     * double-frees it. */
    void (*release_render_target)(void* self, nxt_render_target surface);

    /* The colour texture of a render target, so the library can answer the SDK's
     * RenderTarget::GetTexture. A separate handle with its own lifetime -- see
     * release_render_target. */
    nxt_texture (*get_render_target_texture)(void* self, nxt_render_target surface);

    /* `data` is an array of num_levels pointers, or null for an empty texture. */
    nxt_texture (*create_texture)(void* self, const char* label,
                                  uint32_t width, uint32_t height,
                                  uint32_t num_levels, nxt_texture_format format,
                                  const void** data);

    /* "Drop this handle", not "destroy the GPU resource". The library releases every
     * handle it is given, including ones from wrap_native_texture that refer to a
     * resource the host owns and must keep. What a release means belongs on the host
     * side, where the ownership is known. */
    void (*release_texture)(void* self, nxt_texture texture);

    void (*get_texture_info)(void* self, nxt_texture texture,
                             uint32_t* out_width, uint32_t* out_height,
                             uint32_t* out_levels, nxt_bool* out_has_alpha);

    /* Custom effects. The library forwards ShaderEffect::SetPixelShader and
     * BrushShader::SetPixelShader here; the handle comes back in nxt_batch::pixel_shader.
     * Null on failure, which the library treats as "no custom shader". */
    nxt_pixel_shader (*create_pixel_shader)(void* self, const char* label, uint8_t shader,
                                            const void* blob, uint32_t size);
    void (*release_pixel_shader)(void* self, nxt_pixel_shader shader);

    /* Wraps a texture the host already owns so XAML can sample it. Used by the video
     * sink.
     *
     * `native` is the PyObject the host's own Python handed to the sink, passed straight
     * through: the library never dereferences it and has no idea what it is. Unwrapping
     * it is the host's job, because only the host knows what it asked for. Called with
     * the GIL held. */
    nxt_texture (*wrap_native_texture)(void* self, void* native, nxt_bool has_alpha);

    /* Size of a host-owned texture without wrapping it, false when `native` is not a
     * usable texture. The video element needs the size on the thread that sets the
     * texture, which is not the render thread and must not create GPU resources; and it
     * is the one point where the host can tell a caller it passed the wrong object. */
    nxt_bool (*get_native_texture_size)(void* self, void* native,
                                        uint32_t* out_width, uint32_t* out_height);
} nxt_render_device;

/* ------------------------------------------------------------------------- */
/* Frame host -- provided by the host, consumed by the library                */
/* ------------------------------------------------------------------------- */

/* Everything that needs the frame's deferred context bound. The host hands one of these
 * to nxt_view::render for the duration of that call and no longer: holding on to it
 * past the frame is a use-after-free, which is why it is a parameter rather than state
 * and why its retain and release are null.
 *
 * There is no scissor entry here on purpose. The host knows the parent clip rect and
 * begin_onscreen_render is a host call, so both ends are already on its side; routing
 * the clip through this ABI would end where it started. */
typedef struct nxt_frame_context
{
    nxt_interface_header header;

    void (*update_texture)(void* self, nxt_texture texture, uint32_t level,
                           uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                           const void* data);

    void (*begin_offscreen_render)(void* self);
    void (*end_offscreen_render)(void* self);
    void (*begin_onscreen_render)(void* self);
    void (*end_onscreen_render)(void* self);

    void (*set_render_target)(void* self, nxt_render_target surface);
    void (*begin_tile)(void* self, nxt_render_target surface, const nxt_tile* tile);
    void (*end_tile)(void* self, nxt_render_target surface);
    void (*resolve_render_target)(void* self, nxt_render_target surface,
                                  const nxt_tile* tiles, uint32_t num_tiles);

    void* (*map_vertices)(void* self, uint32_t bytes);
    void (*unmap_vertices)(void* self);
    void* (*map_indices)(void* self, uint32_t bytes);
    void (*unmap_indices)(void* self);

    void (*draw_batch)(void* self, const nxt_batch* batch);
} nxt_frame_context;

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
typedef struct nxt_view
{
    nxt_interface_header header;

    nxt_bool (*is_loaded)(void* self);

    /* Brings up the SDK renderer on first use. Creates GPU resources, so the host must
     * call it inside its managed rendering bracket. False means this view cannot render
     * this frame; the host should skip it without logging per-frame. */
    nxt_bool (*ensure_renderer)(void* self, const nxt_frame_context* frame);

    void (*sync_size)(void* self, uint32_t width, uint32_t height);
    void (*update)(void* self, double seconds);

    void (*update_render_tree)(void* self, const nxt_frame_context* frame);
    nxt_bool (*render_offscreen)(void* self, const nxt_frame_context* frame);
    void (*render)(void* self, const nxt_frame_context* frame, nxt_bool flip_y, nxt_bool clear);
} nxt_view;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* NXT_H */
