#ifndef SURFACE_EMU_H
#define SURFACE_EMU_H

#include "../helpers/interface.h"
#include "../helpers/log.h"
#include "ddraw_emu.h"
#include "ddraw.h"
#include "d3d.h"
#include <deque>
#include <vector>
#include <list>

namespace emu {

/**
 * @brief Emulation of KA specific behavior.
 */
struct EmulationInfo {

    enum EmulationState {

        /**
         * @brief Waits for next unlock or load after expired timeout.
         *
         * Any 3d event switches to 3D_SCENE mode.
         */
        EMULATION_STATE_WAITING_FOR_TIME,

        /**
         * @brief 3D scene rendering is active.
         */
        EMULATION_STATE_3D_SCENE,

        /**
         * @brief Point based geometry (starfield) was already drawn into the scene.
         */
        EMULATION_STATE_3D_SCENE_POINT_GEOMETRY_DRAWN,

        /**
         * @brief Triangle based geometry was already drawn into the scene.
         */
        EMULATION_STATE_3D_SCENE_TRIANGLE_GEOMETRY_DRAWN,

        /**
         * @brief We are waiting for flip of the finalized scene.
         */
        EMULATION_STATE_3D_FLIP,

        /**
         * @brief Waiting for start of next 3d rendering.
         *
         * Can transition to 3D_SCENE if new scene starts or
         * to WAITING_FOR_TIME if timeout expires.
         */
        EMULATION_STATE_WAITING_FOR_3D_SCENE,
    };

    /**
     * @brief Which event we are waiting for.
     */
    EmulationState emulation_state;

    /**
     * @brief Time of start of current emulation related timeout.
     */
    DWORD emulation_timeout_start;

    /**
     * @brief Window used for timer delivery.
     */
    HWND timer_window;

    EmulationInfo(const HINSTANCE instance, DirectDrawSurfaceEmu &surface);
    ~EmulationInfo();
};

class DirectDrawSurfaceEmu : public IUnknownImpl, public IDirectDrawSurface, public IDirectDrawSurface3, public IDirectDrawSurface4, public IDirect3DDevice, public IDirect3DTexture {

private:

    HWLayer &hw_layer;
    const HINSTANCE instance;

private:

    /**
     * @brief Descriptor used to create this surface.
     */
    DDSURFACEDESC desc;

    /**
     * @brief Surface we are attached to.
     *
     * Set to 'this' if we are not attached to anything.
     */
    DirectDrawSurfaceEmu * master_surface;

    /**
     * @brief Are we owned by the master surface?
     *
     * Owned surfaces are destroyed with the master surface.
     */
    bool owned;

    /**
     * @brief Surfaces attached to this surface.
     */
    std::list<DirectDrawSurfaceEmu *> attached_surfaces;

private:

    /**
     * @brief Viewports attached to 3d device associated
     * with this surface.
     */
    std::deque<IDirect3DViewport *> viewports;

private:

    // Surface memory management. Content is switched during the Flip() operation.

    /**
     * @brief Backing memory for the surface.
     */
    void * memory;

    /**
     * @brief Handle of the hardware surface, if allocated.
     */
    HWSurfaceHandle hw_surface;

    enum Master {
        MASTER_NONE,            // The surface is in a freshly initialized state.
        MASTER_MEMORY,          // The system memory contains the latest data.
        MASTER_HW,              // The HW surface contains the latest data.
        MASTER_SYNCHRONIZED,    // Both copies are synchronized.
        MASTER_COMPOSITION,     // The system memory contains the composition content which might be zero. Content will be checked before composition.
        MASTER_COMPOSITION_NONKEY, // The system memory contains the composition content which is very likely non-key value. Content will be directly composed.
    };

    /**
     * @brief Indicates who contains the master copy of this surface.
     */
    Master master;

private:

    /**
     * @brief Emulation state.
     *
     * Created only for the complex 3d surfaces.
     */
    EmulationInfo * emulation;

private:

    // Device state.

    /**
     * @brief Is scene rendering active?
     */
    bool scene_active;

    /**
     * @brief Number of pending locks.
     */
    size_t lock_count;

    enum LockHack {
        LOCK_HACK_NONE,
        LOCK_HACK_COMPOSITION,
        LOCK_HACK_STARFIELD,
    };

    /**
     * @brief Custom locking behavior active for current lock.
     */
    LockHack active_lock_hack;

    /**
     * @brief Emulated render states.
     *
     * Used by apply_render_states() to apply the state to the HW.
     */
    struct RenderStateInfo {

        /**
         * @brief Value of the state.
         */
        union {
            DWORD dw_value;
            D3DVALUE float_value;
        } value ;
    };

    /**
     * @brief Set of render states.
     */
    struct RenderStateSet {

    public:

        enum {
            RENDER_STATE_COUNT = D3DRENDERSTATE_FOGTABLEDENSITY + 1
        };

    private:

        /**
         * @brief State values.
         */
        RenderStateInfo states[RENDER_STATE_COUNT];

        /**
         * @brief Hash of the state.
         *
         * If two state sets have different value of the hash, they are
         * different. The opposite is not true.
         */
        unsigned __int64 hash;

        /**
         * @brief Set sequence number.
         *
         * Increased during each change of stored value. If one
         * set is created by copying another set, they will have
         * the same value.
         */
        unsigned sequence_number;

    public:

        RenderStateSet();

        // Changes.

        bool set_rs_dw(const D3DRENDERSTATETYPE type, const DWORD value);
        bool set_rs_float(const D3DRENDERSTATETYPE type, const float value);

        // Queries.

        size_t get_rs_dw(const D3DRENDERSTATETYPE type) const;
        bool get_rs_bool(const D3DRENDERSTATETYPE type) const;
        float get_rs_float(const D3DRENDERSTATETYPE type) const;

        // Comparison.

        bool compare_states(const RenderStateSet &other) const;
        bool equals(const RenderStateSet &other) const;
        bool equals_with_sequence(const RenderStateSet &other) const;

        // Application to the hardware.

        void apply_render_states(HWLayer &hw_layer) const;

        DepthTest get_depth_test_state(void) const;
        AlphaTest get_alpha_test_state(void) const;
        Blend get_alpha_blend_state(void) const;
        Fog get_fog_state(void) const;
        TextureBlend get_texture_blend(void) const;

    } active_render_states;

    /**
     * @brief Indication which states we support for automatic error reporting.
     */
    bool supported_states[RenderStateSet::RENDER_STATE_COUNT];

private:

    enum GeometryMode {
        GEOMETRY_MODE_POINTS,
        GEOMETRY_MODE_LINES,
        GEOMETRY_MODE_TRIANGLES,
    };

    /**
     * @brief Vertices from current execute buffer.
     */
    std::vector<TLVertex> vertices;

    /**
     * @brief Information about geometry queued for rendering.
     */
    struct GeometryInfo {

    private:

        /**
         * @brief How to interpret the indices.
         */
        GeometryMode geometry_mode;

        /**
         * @brief Indices of points or triangles to draw.
         */
        std::vector<unsigned short> indices;

        /**
         * @note Range of vertices used by currently active triangles/points.
         */
        //@{
        size_t min_vertex;
        size_t max_vertex;
        //@}

        /**
         * @brief State set to use to render this geometry.
         *
         * Valid only if the geometry is not empty.
         */
        RenderStateSet state_set;

    public:

        GeometryInfo();
        void reset(void);

        bool is_empty(void) const;
        GeometryMode get_mode(void) const;

        void set_mode(const GeometryMode mode);
        void set_state_set(const RenderStateSet &set);
        bool is_state_set_unchanged(const RenderStateSet &set) const;
        size_t get_shade_mode_render_state(void) const;

        void add_triangle(const unsigned short v0, const unsigned short v1, const unsigned short v2);
        void add_line(const unsigned short v0, const unsigned short v1);
        void add_points(const size_t first, const size_t count);

        void apply_state(HWLayer &hw_layer);
        void draw_geometry(HWLayer &hw_layer, const TLVertex * const vertices);
    };

    /**
     * @brief Informations about currently queued geometry.
     */
    GeometryInfo queued_geometry;

    /**
     * @brief Geometry belonging to the overlay hack.
     *
     * This geometry is flushed together with the queued geometry. Must be empty
     * whenever the queued_geometry is empty.
     */
    GeometryInfo queued_overlay_geometry;

public:

    DirectDrawSurfaceEmu(HWLayer &the_hw_layer, const HINSTANCE the_instance);
    virtual ~DirectDrawSurfaceEmu();

    // API support.

    HRESULT initialize(const DDSURFACEDESC &descriptor);
    void attach_sub_surface(DirectDrawSurfaceEmu * const surface, const bool as_owned);
    void detach_sub_surface(DirectDrawSurfaceEmu * const surface);

    EmulationInfo *find_emulation_info(void);
    EmulationInfo &get_emulation_info(void);
    DirectDrawSurfaceEmu * find_surface(const size_t flag, const bool allow_up = true);
    DirectDrawSurfaceEmu * find_front_buffer(void);
    DirectDrawSurfaceEmu * find_back_buffer(void);
    DirectDrawSurfaceEmu * find_depth_buffer(void);

    // Presentation support.

    void set_present_timer(const size_t timeout);
    void kill_present_timer(void);
    void update_presentation_emulation(void);
    void show_primary(void);

    // Memory management.

    void synchronize_memory(void);
    void synchronize_hw(void);
    HWSurfaceHandle get_hw_surface(const bool for_rendering_into);
    HWFormat get_hw_format(void) const;

    // State management.

    void set_default_render_states(void);
    void set_render_state(const D3DSTATE &state);


    // Triangle processing.

    void begin_geometry(const size_t count);
    bool set_vertices(const size_t start, const D3DTLVERTEX * const vertices, const size_t count);
    void add_triangle(const unsigned short v0, const unsigned short v1, const unsigned short v2);
    void add_line(const size_t first, const size_t second);
    void add_points(const size_t first, const size_t count);
    void flush_geometry(void);
    void end_geometry(void);

    // IUnknown.

    IUNKNOWN_IMPLEMENTATION()
    GET_IMPLEMENTATION_BEGIN()
    GET_IMPLEMENTATION_IFACE(IID_IDirect3DTexture, IDirect3DTexture)
    GET_IMPLEMENTATION_IFACE(IID_IDirectDrawSurface, IDirectDrawSurface)
    GET_IMPLEMENTATION_IFACE(IID_IDirectDrawSurface3, IDirectDrawSurface3)
    GET_IMPLEMENTATION_IFACE(IID_IDirectDrawSurface4, IDirectDrawSurface4)
    GET_IMPLEMENTATION_IFACE(EMULATED_DEVICE_GUID, IDirect3DDevice)
    GET_IMPLEMENTATION_END()

    //  IDirectDrawSurface methods

    STDMETHODIMP AddAttachedSurface(LPDIRECTDRAWSURFACE);
    STDMETHODIMP AddOverlayDirtyRect(LPRECT) DUMMY;
    STDMETHODIMP Blt(LPRECT,LPDIRECTDRAWSURFACE, LPRECT,DWORD, LPDDBLTFX) DUMMY;
    STDMETHODIMP BltBatch(LPDDBLTBATCH, DWORD, DWORD ) DUMMY;
    STDMETHODIMP BltFast(DWORD,DWORD,LPDIRECTDRAWSURFACE, LPRECT,DWORD);
    STDMETHODIMP DeleteAttachedSurface(DWORD,LPDIRECTDRAWSURFACE) UNIMPLEMENTED;
    STDMETHODIMP EnumAttachedSurfaces(LPVOID,LPDDENUMSURFACESCALLBACK) UNIMPLEMENTED;
    STDMETHODIMP EnumOverlayZOrders(DWORD,LPVOID,LPDDENUMSURFACESCALLBACK) UNIMPLEMENTED;
    STDMETHODIMP Flip(LPDIRECTDRAWSURFACE, DWORD);
    STDMETHODIMP GetAttachedSurface(LPDDSCAPS, LPDIRECTDRAWSURFACE FAR *);
    STDMETHODIMP GetBltStatus(DWORD) UNIMPLEMENTED;
    STDMETHODIMP GetCaps(LPDDSCAPS) UNIMPLEMENTED;
    STDMETHODIMP GetClipper(LPDIRECTDRAWCLIPPER FAR*) UNIMPLEMENTED;
    STDMETHODIMP GetColorKey(DWORD, LPDDCOLORKEY) UNIMPLEMENTED;
    STDMETHODIMP GetDC(HDC FAR *) UNIMPLEMENTED;
    STDMETHODIMP GetFlipStatus(DWORD) UNIMPLEMENTED;
    STDMETHODIMP GetOverlayPosition(LPLONG, LPLONG ) UNIMPLEMENTED;
    STDMETHODIMP GetPalette(LPDIRECTDRAWPALETTE FAR*) UNIMPLEMENTED;
    STDMETHODIMP GetPixelFormat(LPDDPIXELFORMAT);
    STDMETHODIMP GetSurfaceDesc(LPDDSURFACEDESC);
    STDMETHODIMP Initialize(LPDIRECTDRAW, LPDDSURFACEDESC) UNSUPPORTED;
    STDMETHODIMP IsLost() UNIMPLEMENTED;
    STDMETHODIMP Lock(LPRECT,LPDDSURFACEDESC,DWORD,HANDLE);
    STDMETHODIMP ReleaseDC(HDC) UNIMPLEMENTED;
    STDMETHODIMP Restore() DUMMY;
    STDMETHODIMP SetClipper(LPDIRECTDRAWCLIPPER) UNIMPLEMENTED;
    STDMETHODIMP SetColorKey(DWORD, LPDDCOLORKEY) UNIMPLEMENTED;
    STDMETHODIMP SetOverlayPosition(LONG, LONG) UNIMPLEMENTED;
    STDMETHODIMP SetPalette(LPDIRECTDRAWPALETTE) UNIMPLEMENTED;
    STDMETHODIMP Unlock(LPVOID);
    STDMETHODIMP UpdateOverlay(LPRECT, LPDIRECTDRAWSURFACE,LPRECT,DWORD, LPDDOVERLAYFX) UNIMPLEMENTED;
    STDMETHODIMP UpdateOverlayDisplay(DWORD) UNIMPLEMENTED;
    STDMETHODIMP UpdateOverlayZOrder(DWORD, LPDIRECTDRAWSURFACE) UNIMPLEMENTED;

    // IDirect3DDevice methods

    STDMETHODIMP Initialize(LPDIRECT3D,LPGUID,LPD3DDEVICEDESC) UNSUPPORTED;
    STDMETHODIMP GetCaps(LPD3DDEVICEDESC,LPD3DDEVICEDESC) UNIMPLEMENTED;
    STDMETHODIMP SwapTextureHandles(LPDIRECT3DTEXTURE,LPDIRECT3DTEXTURE) UNIMPLEMENTED;
    STDMETHODIMP CreateExecuteBuffer(LPD3DEXECUTEBUFFERDESC,LPDIRECT3DEXECUTEBUFFER*,IUnknown*);
    STDMETHODIMP GetStats(LPD3DSTATS) UNIMPLEMENTED;
    STDMETHODIMP Execute(LPDIRECT3DEXECUTEBUFFER,LPDIRECT3DVIEWPORT,DWORD);
    STDMETHODIMP AddViewport(LPDIRECT3DVIEWPORT);
    STDMETHODIMP DeleteViewport(LPDIRECT3DVIEWPORT);
    STDMETHODIMP NextViewport(LPDIRECT3DVIEWPORT,LPDIRECT3DVIEWPORT*,DWORD) UNIMPLEMENTED;
    STDMETHODIMP Pick(LPDIRECT3DEXECUTEBUFFER,LPDIRECT3DVIEWPORT,DWORD,LPD3DRECT) UNIMPLEMENTED;
    STDMETHODIMP GetPickRecords(LPDWORD,LPD3DPICKRECORD) UNIMPLEMENTED;
    STDMETHODIMP EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK,LPVOID);
    STDMETHODIMP CreateMatrix(LPD3DMATRIXHANDLE) UNIMPLEMENTED;
    STDMETHODIMP SetMatrix(D3DMATRIXHANDLE,const LPD3DMATRIX) DUMMY;
    STDMETHODIMP GetMatrix(D3DMATRIXHANDLE,LPD3DMATRIX) UNIMPLEMENTED;
    STDMETHODIMP DeleteMatrix(D3DMATRIXHANDLE) UNIMPLEMENTED;
    STDMETHODIMP BeginScene();
    STDMETHODIMP EndScene();
    STDMETHODIMP GetDirect3D(LPDIRECT3D*) UNIMPLEMENTED;

    // IDirect3DTexture methods

    STDMETHODIMP Initialize(LPDIRECT3DDEVICE,LPDIRECTDRAWSURFACE) UNSUPPORTED;
    STDMETHODIMP GetHandle(LPDIRECT3DDEVICE,LPD3DTEXTUREHANDLE);
    STDMETHODIMP PaletteChanged(DWORD,DWORD) DUMMY;
    STDMETHODIMP Load(LPDIRECT3DTEXTURE);
    STDMETHODIMP Unload();

    // Fake IDirectDrawSurface3 for version detection by the launcher.

    STDMETHODIMP AddAttachedSurface(LPDIRECTDRAWSURFACE3) UNIMPLEMENTED;
    STDMETHODIMP Blt(LPRECT,LPDIRECTDRAWSURFACE3, LPRECT,DWORD, LPDDBLTFX) UNIMPLEMENTED;
    STDMETHODIMP BltFast(DWORD,DWORD,LPDIRECTDRAWSURFACE3, LPRECT,DWORD) UNIMPLEMENTED;
    STDMETHODIMP DeleteAttachedSurface(DWORD,LPDIRECTDRAWSURFACE3) UNIMPLEMENTED;
    STDMETHODIMP Flip(LPDIRECTDRAWSURFACE3, DWORD) UNIMPLEMENTED;
    STDMETHODIMP GetAttachedSurface(LPDDSCAPS, LPDIRECTDRAWSURFACE3 FAR *) UNIMPLEMENTED;
    STDMETHODIMP UpdateOverlay(LPRECT, LPDIRECTDRAWSURFACE3,LPRECT,DWORD, LPDDOVERLAYFX) UNIMPLEMENTED;
    STDMETHODIMP UpdateOverlayZOrder(DWORD, LPDIRECTDRAWSURFACE3) UNIMPLEMENTED;
    STDMETHODIMP GetDDInterface(LPVOID FAR *) UNIMPLEMENTED;
    STDMETHODIMP PageLock(DWORD) UNIMPLEMENTED;
    STDMETHODIMP PageUnlock(DWORD) UNIMPLEMENTED;
    STDMETHODIMP SetSurfaceDesc(LPDDSURFACEDESC, DWORD) UNIMPLEMENTED;

    // Fake IDirectDrawSurface4 for version detection by the launcher.

    STDMETHODIMP AddAttachedSurface(LPDIRECTDRAWSURFACE4) UNIMPLEMENTED;
    STDMETHODIMP Blt(LPRECT,LPDIRECTDRAWSURFACE4, LPRECT,DWORD, LPDDBLTFX) UNIMPLEMENTED;
    STDMETHODIMP BltFast(DWORD,DWORD,LPDIRECTDRAWSURFACE4, LPRECT,DWORD) UNIMPLEMENTED;
    STDMETHODIMP DeleteAttachedSurface(DWORD,LPDIRECTDRAWSURFACE4) UNIMPLEMENTED;
    STDMETHODIMP EnumAttachedSurfaces(LPVOID,LPDDENUMSURFACESCALLBACK2) UNIMPLEMENTED;
    STDMETHODIMP EnumOverlayZOrders(DWORD,LPVOID,LPDDENUMSURFACESCALLBACK2) UNIMPLEMENTED;
    STDMETHODIMP Flip(LPDIRECTDRAWSURFACE4, DWORD) UNIMPLEMENTED;
    STDMETHODIMP GetAttachedSurface(LPDDSCAPS2, LPDIRECTDRAWSURFACE4 FAR *) UNIMPLEMENTED;
    STDMETHODIMP GetCaps(LPDDSCAPS2) UNIMPLEMENTED;
    STDMETHODIMP GetSurfaceDesc(LPDDSURFACEDESC2) UNIMPLEMENTED;
    STDMETHODIMP Initialize(LPDIRECTDRAW, LPDDSURFACEDESC2) UNSUPPORTED;
    STDMETHODIMP Lock(LPRECT,LPDDSURFACEDESC2,DWORD,HANDLE) UNIMPLEMENTED;
    STDMETHODIMP Unlock(LPRECT) UNIMPLEMENTED;
    STDMETHODIMP UpdateOverlay(LPRECT, LPDIRECTDRAWSURFACE4,LPRECT,DWORD, LPDDOVERLAYFX) UNIMPLEMENTED;
    STDMETHODIMP UpdateOverlayZOrder(DWORD, LPDIRECTDRAWSURFACE4) UNIMPLEMENTED;
    STDMETHODIMP SetSurfaceDesc(LPDDSURFACEDESC2, DWORD) UNIMPLEMENTED;
    STDMETHODIMP SetPrivateData(REFGUID, LPVOID, DWORD, DWORD) UNIMPLEMENTED;
    STDMETHODIMP GetPrivateData(REFGUID, LPVOID, LPDWORD) UNIMPLEMENTED;
    STDMETHODIMP FreePrivateData(REFGUID) UNIMPLEMENTED;
    STDMETHODIMP GetUniquenessValue(LPDWORD) UNIMPLEMENTED;
    STDMETHODIMP ChangeUniquenessValue(void) UNIMPLEMENTED;
};

} // namespace emu

#endif // DDSURFACE_EMU_H

// EOF //
