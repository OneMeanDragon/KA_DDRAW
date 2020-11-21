#include "viewport_emu.h"
#include "material_emu.h"
#include "structure_log.h"
#include <assert.h>
#include "../helpers/config.h"

namespace emu {

Direct3DViewportEmu::Direct3DViewportEmu(HWLayer &the_hw_layer)
    : hw_layer(the_hw_layer)
    , background_material(NULL)
{
    LOG_METHOD();
    memset(&viewport, 0, sizeof(viewport));
}

Direct3DViewportEmu::~Direct3DViewportEmu()
{
    LOG_METHOD();
    if (background_material) {
        background_material->Release();
    }
}

/**
 * @brief Get parameters of the viewport.
 */
STDMETHODIMP Direct3DViewportEmu::GetViewport(LPD3DVIEWPORT viewport)
{
    LOG_METHOD();
    CHECK_STRUCTURE(viewport, D3DVIEWPORT);

    *viewport = this->viewport;
    return DD_OK;
}

/**
 * @brief Sets parameters of the viewport.
 */
STDMETHODIMP Direct3DViewportEmu::SetViewport(LPD3DVIEWPORT viewport)
{
    LOG_METHOD();
    CHECK_STRUCTURE(viewport, D3DVIEWPORT);

    this->viewport = *viewport;
    log_structure(MSG_VERBOSE, 1, this->viewport);
    return DD_OK;    
}

/**
 * @brief Sets material used for background clears.
 */
STDMETHODIMP Direct3DViewportEmu::SetBackground(D3DMATERIALHANDLE material_handle)
{
    LOG_METHOD();
    logKA(MSG_VERBOSE, 1, "handle: %08X", material_handle);

    Direct3DMaterialEmu * const new_material = material_handle ? reinterpret_cast<Direct3DMaterialEmu *>(material_handle) : NULL;
    if (new_material) {
        new_material->AddRef();
    }

    if (background_material) {
        background_material->Release();
    }

    background_material = new_material;
    return DD_OK;
}

/**
 * @brief Returns material used for background clears.
 */
STDMETHODIMP Direct3DViewportEmu::GetBackground(LPD3DMATERIALHANDLE material, LPBOOL valid)
{
    LOG_METHOD();
    CHECK_NOT_NULL(material);
    CHECK_NOT_NULL(valid);

    *material = reinterpret_cast<DWORD>(background_material);
    *valid = (background_material != NULL) ? TRUE : FALSE;
    return DD_OK;
}

/**
 * @brief Clear this viewport.
 */
STDMETHODIMP Direct3DViewportEmu::Clear(DWORD count,LPD3DRECT rects, DWORD flags)
{
    LOG_METHOD();

    // logKA informations.

    logKA(MSG_VERBOSE, 1, "count: %u", count);
    if (flags & D3DCLEAR_TARGET) {
        logKA(MSG_VERBOSE, 1, "TARGET");
    }
    if (flags & D3DCLEAR_ZBUFFER) {
        logKA(MSG_VERBOSE, 1, "ZBUFFER");
    }

    // Only one clear rectangle is supported.

    if (count != 1) {
        logKA(MSG_ERROR, 0, "Exactly one clear rectangle is supported");
        return DDERR_INVALIDPARAMS;
    }
    CHECK_NOT_NULL(rects);

    // Handle background material properties. The Klingon Academy seems to
    // always use black color. The Starfleet Academy on the other hand
    // uses the diffuse color to clear background for nebulaes.

    DWORD color = 0;
    if (is_inside_sfad3d() && background_material) {
        color = background_material->get_diffuse_color();
    }

    // Do the clear.

    RECT rect;
    rect.left = rects[0].x1;
    rect.top = rects[0].y1;
    rect.right = rects[0].x2;
    rect.bottom = rects[0].y2;

    // The Starfleet Academy uses many small clears to reduce clearing bandwidth.
    // The torpedo rectangles do not work too well with anti-aliasing so increase the clearing
    // rectangle to compensate. To ensure that we do not break the screen border, we avoid
    // increasing sides which look like to be snapped on edge of the bridge screen.

    if (is_inside_sfad3d() && (get_msaa_quality_level() > 0)) {
        if (rect.left != 44) {
            rect.left -= 1;
        }
        if (rect.top != 42) {
            rect.top -= 1;
        }
        if (rect.right != 598) {
            rect.right += 1;
        }
        if (rect.bottom != 234) {
            rect.bottom += 1;
        }
    }

    hw_layer.clear(
        rect,
        (flags & D3DCLEAR_TARGET) != 0,
        (flags & D3DCLEAR_ZBUFFER) != 0,
        color,
        1.0f
    );
    return DD_OK;
}

} // namespace emu

// EOF //
