#ifndef INTERFACE_H
#define INTERFACE_H

#include <windows.h>
#include "log.h"
#include "common.h"

namespace emu {

/**
 * @brief Base class for classes implementing com interface.
 */
class IUnknownImpl : public IUnknown {

private:

    /**
     * @brief Number of references.
     */
    volatile long ref_count ;

public:

    IUnknownImpl()
        : ref_count(1)
    {
    }

    virtual ~IUnknownImpl()
    {
    }

    // Standard AddRef/Release interface.

    STDMETHODIMP_(ULONG) AddRef(void)
    {
        logKA(::emu::MSG_ULTRA_VERBOSE, 0, "%08x: %u" __FUNCTION__, this, ref_count + 1);
        return InterlockedIncrement(&ref_count);
    }

    STDMETHODIMP_(ULONG) Release(void)
    {
        logKA(::emu::MSG_ULTRA_VERBOSE, 0, "%08x: %u" __FUNCTION__, this, ref_count - 1);
        const long new_count = InterlockedDecrement(&ref_count);
        if (new_count > 0) {
            return new_count;
        }
        delete this;
        return 0;
    }

    // Common part of the QueryInterface method.

    STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR * object)
    {
        // Each class implements this interface.

        if (riid == IID_IUnknown) {
            AddRef();
            *object = static_cast<IUnknown *>(this);
            return S_OK;
        }

        // Ask the derived class for the implementation.

        *object = GetImplementation(riid);
        if (*object != NULL) {
            static_cast<IUnknown*>(*object)->AddRef();
            return S_OK;
        }

        logKA(
            MSG_ERROR,
            0,
            "Interface not implemented: %8.8X-%4.4X-%4.4X-%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X",
            riid.Data1, riid.Data2, riid.Data3,
            riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7]
        );
        return E_NOINTERFACE;
    }

protected:

    /**
     * @brief Returns pointer to implementation of specified interface or NULL.
     */
    virtual LPVOID GetImplementation(REFIID UNUSED_PARAMETER(riid))
    {
        return NULL;
    }
};

/**
 * @brief Implementation of the IUnknown interface.
 */
#define IUNKNOWN_IMPLEMENTATION()                                                                                       \
    STDMETHODIMP_(ULONG) AddRef(void) { return IUnknownImpl::AddRef(); }                                                \
    STDMETHODIMP_(ULONG) Release(void) { return IUnknownImpl::Release(); }                                              \
    STDMETHODIMP QueryInterface(REFIID riid, LPVOID FAR * object) { return IUnknownImpl::QueryInterface(riid, object); }

/**
 * @name Helpers for easy Definition of the GetImplementation method.
 *
 * @code
 * GET_IMPLEMENTATION_BEGIN()
 * GET_IMPLEMENTATION_IFACE(IID_IFooo, FooType)
 * GET_IMPLEMENTATION_END()
 */
//@{
#define GET_IMPLEMENTATION_BEGIN() virtual LPVOID GetImplementation(REFIID riid) {
#define GET_IMPLEMENTATION_IFACE(clsid, type) if (riid == (clsid)) { return (type *)(this); }
#define GET_IMPLEMENTATION_END() return NULL; }
//@}

/**
 * @brief Prints name of current function to logKA.
 */
#define LOG_FUNCTION_NAME() logKA(::emu::MSG_VERBOSE, 0, __FUNCTION__)

/**
 * @brief Prints this pointer and name of current function to logKA.
 */
#define LOG_METHOD() logKA(::emu::MSG_VERBOSE, 0, "%08x:" __FUNCTION__, this)

/**
 * @brief Returns with DDERR_INVALIDPARAMS if specified parameter is NULL.
 */
#define CHECK_NOT_NULL(parameter) if (! (parameter)) return DDERR_INVALIDPARAMS;

/**
 * @brief Returns with DDERR_INVALIDPARAMS if specified parameter is not NULL.
 */
#define CHECK_NULL(parameter) if (parameter) return DDERR_INVALIDPARAMS;

/**
 * @brief Checks that specified structure exists and its dwSize field has same
 * size as specified structure.
 */
#define CHECK_STRUCTURE(parameter, type) { CHECK_NOT_NULL(parameter); if ((parameter)->dwSize != sizeof(type)) { return DDERR_INVALIDPARAMS; } }

/**
 * @name Define function body which logs function name and returns specified error code.
 */
//@{
#define UNSUPPORTED { logKA(MSG_ERROR, 0, "%08x:" __FUNCTION__ " is not supported", this); return DDERR_UNSUPPORTED; }
#define UNIMPLEMENTED { logKA(MSG_ERROR, 0, "%08x:" __FUNCTION__ " is not implemented", this); return DDERR_UNSUPPORTED; }
#define DUMMY { logKA(MSG_INFORM, 0, "%08x:" __FUNCTION__ " is dummmy", this); return DD_OK; }
//@}

} // namespace emu

#endif // INTERFACE_H

// EOF //
