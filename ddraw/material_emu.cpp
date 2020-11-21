#include "material_emu.h"
#include "structure_log.h"
#include <assert.h>

namespace emu {
namespace {

/**
 * @brief Converts color from its DX representation.
 */
DWORD convert_color(const D3DCOLORVALUE &color)
{
    const unsigned char red = static_cast<unsigned char>(color.r * 255);
    const unsigned char green = static_cast<unsigned char>(color.g * 255);
    const unsigned char blue = static_cast<unsigned char>(color.b * 255);
    const unsigned char alpha = static_cast<unsigned char>(color.a * 255);
    return blue | (green << 8) | (red << 16) | (alpha << 24);
}

} // anonymous namespace.

Direct3DMaterialEmu::Direct3DMaterialEmu()
    : diffuse_color(0)
{
    LOG_METHOD();
    memset(&material, 0, sizeof(material));
    material.dwSize = sizeof(material);
}

Direct3DMaterialEmu::~Direct3DMaterialEmu()
{
    LOG_METHOD();
}

/**
 * @brief Retrieves the diffuse component of the material color.
 */
DWORD Direct3DMaterialEmu::get_diffuse_color(void) const
{
    return diffuse_color;
}

/**
 * @brief Sets parameters of the material.
 */
STDMETHODIMP Direct3DMaterialEmu::SetMaterial(LPD3DMATERIAL material)
{
    LOG_METHOD();
    CHECK_STRUCTURE(material, D3DMATERIAL);

    this->material = *material;
    log_structure(MSG_ULTRA_VERBOSE, 1, this->material);

    // Cache relevant color info.

    diffuse_color = convert_color(material->diffuse);
    return DD_OK;
}

/**
 * @brief Gets parameters of the material.
 */
STDMETHODIMP Direct3DMaterialEmu::GetMaterial(LPD3DMATERIAL material)
{
    LOG_METHOD();
    CHECK_STRUCTURE(material, D3DMATERIAL);

    *material = this->material;
    return DD_OK;
}

/**
 * @brief Returns handle for this material for specified d3d device.
 */
STDMETHODIMP Direct3DMaterialEmu::GetHandle(LPDIRECT3DDEVICE device,LPD3DMATERIALHANDLE handle)
{
    LOG_METHOD();
    CHECK_NOT_NULL(device);
    CHECK_NOT_NULL(handle);

    // HACK: Pointer to us is our handle. This works only for 32 bit process.
    // The api we are emulating is ancient so it was never used by
    // any 64 bit process.

    *handle = reinterpret_cast<DWORD>(this);
    return DD_OK;
}

} // namespace emu

// EOF //
