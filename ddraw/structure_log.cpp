#include "structure_log.h"

namespace emu {

void log_structure(const MsgType type, const size_t level, const DDPIXELFORMAT &format)
{
    logKA(type, level, "DDPIXELFORMAT begin");
#define LOG_FLAG(name) if (format.dwFlags & DDPF_##name) { logKA(type, (level + 1), #name); }
    LOG_FLAG(COMPRESSED);
    LOG_FLAG(PALETTEINDEXED1);
    LOG_FLAG(PALETTEINDEXED2);
    LOG_FLAG(PALETTEINDEXED4);
    LOG_FLAG(PALETTEINDEXED8);
    LOG_FLAG(PALETTEINDEXEDTO8);
    LOG_FLAG(RGBTOYUV);
#undef LOG_FLAG

    if (format.dwFlags & DDPF_FOURCC){
        logKA(type, (level + 1), "dwFourCC: %X", format.dwFourCC);
    }
    if (format.dwFlags & DDPF_RGB){
        logKA(type, (level + 1), "dwRGBBitCount: %u", format.dwRGBBitCount);
        logKA(type, (level + 1), "dwRBitMask: %X", format.dwRBitMask);
        logKA(type, (level + 1), "dwGBitMask: %X", format.dwGBitMask);
        logKA(type, (level + 1), "dwBBitMask: %X", format.dwBBitMask);
        if (format.dwFlags & DDPF_ALPHAPIXELS) {
            logKA(type, (level + 1), "dwRGBAlphaBitMask: %X", format.dwRGBAlphaBitMask);
        }
    }
    if (format.dwFlags & DDPF_YUV){
        logKA(type, (level + 1), "dwYUVBitCount: %u", format.dwYUVBitCount);
        logKA(type, (level + 1), "dwYBitMask: %X", format.dwYBitMask);
        logKA(type, (level + 1), "dwUBitMask: %X", format.dwUBitMask);
        logKA(type, (level + 1), "dwVBitMask: %X", format.dwVBitMask);
        if (format.dwFlags & DDPF_ALPHAPIXELS) {
            logKA(type, (level + 1), "dwYUVAlphaBitMask: %X", format.dwYUVAlphaBitMask);
        }
    }
    if (format.dwFlags & DDPF_ZBUFFER){
        logKA(type, (level + 1), "dwYUVBitCount: %u", format.dwZBufferBitDepth);
    }
    if (format.dwFlags & DDPF_ALPHA){
        logKA(type, (level + 1), "dwAlphaBitDepth: %u", format.dwAlphaBitDepth);
    }
    logKA(type, level, "DDPIXELFORMAT end");
}

void log_structure(const MsgType type, const size_t level, const DDSCAPS &caps)
{
    logKA(type, level, "DDSCAPS begin");

    // Defines not present in newer versions of the DX SDK.

#ifndef DDSCAPS_PRIMARYSURFACELEFT
#define DDSCAPS_PRIMARYSURFACELEFT      0x00000400l
#endif

#define LOG_CAP(name) if (caps.dwCaps & DDSCAPS_##name) { logKA(type, (level + 1), #name); }
    LOG_CAP(3DDEVICE);
    LOG_CAP(ALLOCONLOAD);
    LOG_CAP(ALPHA);
    LOG_CAP(BACKBUFFER);
    LOG_CAP(COMPLEX);
    LOG_CAP(FLIP);
    LOG_CAP(FRONTBUFFER);
    LOG_CAP(HWCODEC);
    LOG_CAP(LIVEVIDEO);
    LOG_CAP(MIPMAP);
    LOG_CAP(MODEX);
    LOG_CAP(OFFSCREENPLAIN);
    LOG_CAP(OVERLAY);
    LOG_CAP(OWNDC);
    LOG_CAP(PALETTE);
    LOG_CAP(PRIMARYSURFACE);
    LOG_CAP(PRIMARYSURFACELEFT);
    LOG_CAP(SYSTEMMEMORY);
    LOG_CAP(TEXTURE);
    LOG_CAP(VIDEOMEMORY);
    LOG_CAP(VISIBLE);
    LOG_CAP(WRITEONLY);
    LOG_CAP(ZBUFFER);
#undef LOG_CAP
    logKA(type, level, "DDSCAPS end");
}

void log_structure(const MsgType type, const size_t level, const DDSURFACEDESC &desc)
{
    logKA(type, level, "DDSURFACEDESC begin");
    if (desc.dwFlags & DDSD_WIDTH) {
        logKA(type, (level + 1), "dwWidth: %u", desc.dwWidth);
    }
    if (desc.dwFlags & DDSD_HEIGHT) {
        logKA(type, (level + 1), "dwHeight: %u", desc.dwHeight);
    }
    if (desc.dwFlags & DDSD_ZBUFFERBITDEPTH) {
        logKA(type, (level + 1), "dwZBufferBitDepth: %u", desc.dwZBufferBitDepth);
    }
    if (desc.dwFlags & DDSD_ALPHABITDEPTH) {
        logKA(type, (level + 1), "dwAlphaBitDepth: %u", desc.dwAlphaBitDepth);
    }
    if (desc.dwFlags & DDSD_MIPMAPCOUNT) {
        logKA(type, (level + 1), "dwMipMapCount: %u", desc.dwMipMapCount);
    }
    if (desc.dwFlags & DDSD_PITCH) {
        logKA(type, (level + 1), "lPitch: %u", desc.lPitch);
    }
    if (desc.dwRefreshRate & DDSD_REFRESHRATE) {
        logKA(type, (level + 1), "dwRefreshRate: %u", desc.dwRefreshRate);
    }
    if (desc.dwFlags & DDSD_BACKBUFFERCOUNT) {
        logKA(type, (level + 1), "dwBackBufferCount: %u", desc.dwBackBufferCount);
    }
    if (desc.dwFlags & DDSD_CKDESTBLT) {
        logKA(type, (level + 1), "ddckCKDestBlt: %u", desc.ddckCKDestBlt.dwColorSpaceLowValue, desc.ddckCKDestBlt.dwColorSpaceHighValue);
    }
    if (desc.dwFlags & DDSD_CKDESTOVERLAY) {
        logKA(type, (level + 1), "ddckCKDestBlt: %u", desc.ddckCKDestOverlay.dwColorSpaceLowValue, desc.ddckCKDestOverlay.dwColorSpaceHighValue);
    }
    if (desc.dwFlags & DDSD_CKSRCBLT) {
        logKA(type, (level + 1), "ddckCKDestBlt: %u", desc.ddckCKSrcBlt.dwColorSpaceLowValue, desc.ddckCKSrcBlt.dwColorSpaceHighValue);
    }
    if (desc.dwFlags & DDSD_CKSRCOVERLAY) {
        logKA(type, (level + 1), "ddckCKDestBlt: %u", desc.ddckCKSrcOverlay.dwColorSpaceLowValue, desc.ddckCKSrcOverlay.dwColorSpaceHighValue);
    }
    if (desc.dwFlags & DDSD_PIXELFORMAT) {
        logKA(type, (level + 1), "Pixel format");
        log_structure(type, (level + 2), desc.ddpfPixelFormat);
    }
    if (desc.dwFlags & DDSD_CAPS) {
        logKA(type, (level + 1), "Caps");
        log_structure(type, (level + 2), desc.ddsCaps);
    }
    logKA(type, level, "DDSURFACEDESC end");
}

void log_structure(const MsgType type, const size_t level, const char * const name, const D3DCOLORVALUE &color)
{
    logKA(type, level, "%s %f,%f,%f,%f", name, color.r, color.g, color.b, color.a);
}

void log_structure(const MsgType type, const size_t level, const D3DMATERIAL &material)
{
    logKA(type, level, "D3DMATERIAL begin");
    log_structure(type, (level + 1), "diffuse:", material.diffuse);
    log_structure(type, (level + 1), "ambient:", material.ambient);
    log_structure(type, (level + 1), "specular:", material.specular);
    log_structure(type, (level + 1), "emissive:", material.emissive);
    logKA(type, (level + 1), "power: %f", material.power);
    logKA(type, (level + 1), "hTexture: %08X", material.hTexture);
    logKA(type, (level + 1), "dwRampSize: %u", material.dwRampSize);
    logKA(type, level, "D3DMATERIAL end");
}

void log_structure(const MsgType type, const size_t level, const D3DVIEWPORT &viewport)
{
    logKA(type, level, "D3DVIEWPORT begin");
    logKA(type, (level + 1), "dwX: %u", viewport.dwX);
    logKA(type, (level + 1), "dwY: %u", viewport.dwY);
    logKA(type, (level + 1), "dwWidth: %u", viewport.dwWidth);
    logKA(type, (level + 1), "dwHeight: %u", viewport.dwHeight);
    logKA(type, (level + 1), "dvScaleX: %f", viewport.dvScaleX);
    logKA(type, (level + 1), "dvScaleY: %f", viewport.dvScaleY);
    logKA(type, (level + 1), "dvMaxX: %f", viewport.dvMaxX);
    logKA(type, (level + 1), "dvMaxY: %f", viewport.dvMaxY);
    logKA(type, (level + 1), "dvMinZ: %f", viewport.dvMinZ);
    logKA(type, (level + 1), "dvMaxZ: %f", viewport.dvMaxZ);
    logKA(type, level, "D3DVIEWPORT end");
}

void log_structure(const MsgType type, const size_t level, const D3DEXECUTEBUFFERDESC &desc)
{
    logKA(type, level, "D3DEXECUTEBUFFERDESC begin");
    if (desc.dwFlags & D3DDEB_CAPS) {
        if (desc.dwCaps & D3DDEBCAPS_SYSTEMMEMORY) {
            logKA(type, (level + 1), "D3DDEBCAPS_SYSTEMMEMORY");
        }
        if (desc.dwCaps & D3DDEBCAPS_VIDEOMEMORY) {
            logKA(type, (level + 1), "D3DDEBCAPS_VIDEOMEMORY");
        }
    }
    if (desc.dwFlags & D3DDEB_BUFSIZE) {
        logKA(type, (level + 1), "dwBufferSize: %u", desc.dwBufferSize);
    }
    if (desc.dwFlags & D3DDEB_LPDATA) {
        logKA(type, (level + 1), "lpData: %p", desc.lpData);
    }
    logKA(type, level, "D3DEXECUTEBUFFERDESC end");
}

void log_structure(const MsgType type, const size_t level, const D3DSTATUS &str)
{
    logKA(type, level, "D3DSTATUS begin");
    if (str.dwFlags & D3DSETSTATUS_STATUS) {
        logKA(type, (level + 1), "dwStatus: %u", str.dwStatus);
    }
    if (str.dwFlags & D3DSETSTATUS_EXTENTS) {
        logKA(type, (level + 1), "drExtent: %u", str.drExtent);
    }
    logKA(type, level, "D3DSTATUS end");
}

void log_structure(const MsgType type, const size_t level, const D3DEXECUTEDATA &str)
{
    logKA(type, level, "D3DEXECUTEDATA begin");
    logKA(type, (level + 1), "dwVertexOffset: %u", str.dwVertexOffset);
    logKA(type, (level + 1), "dwVertexCount: %u", str.dwVertexCount);
    logKA(type, (level + 1), "dwInstructionOffset: %u", str.dwInstructionOffset);
    logKA(type, (level + 1), "dwInstructionLength: %u", str.dwInstructionLength);
    logKA(type, (level + 1), "dwHVertexOffset: %u", str.dwHVertexOffset);
    log_structure(type, (level + 1), str.dsStatus);
    logKA(type, level, "D3DEXECUTEDATA end");
}

} // namespace emu

// EOF //
