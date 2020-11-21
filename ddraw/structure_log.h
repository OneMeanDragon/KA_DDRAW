#ifndef STRUCTURE_LOG_H
#define STRUCTURE_LOG_H

#include "../helpers/log.h"
#include "ddraw.h"
#include "d3d.h"

namespace emu {

void log_structure(const MsgType type, const size_t level, const DDPIXELFORMAT &format);
void log_structure(const MsgType type, const size_t level, const DDSCAPS &caps);
void log_structure(const MsgType type, const size_t level, const DDSURFACEDESC &desc);
void log_structure(const MsgType type, const size_t level, const char * const name, const D3DCOLORVALUE &color);
void log_structure(const MsgType type, const size_t level, const D3DMATERIAL &material);
void log_structure(const MsgType type, const size_t level, const D3DVIEWPORT &viewport);
void log_structure(const MsgType type, const size_t level, const D3DEXECUTEBUFFERDESC &desc);
void log_structure(const MsgType type, const size_t level, const D3DSTATUS &str);
void log_structure(const MsgType type, const size_t level, const D3DEXECUTEDATA &str);

} // namespace emu

#endif // STRUCTURE_LOG_H

// EOF //
