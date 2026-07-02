/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SHARED_SURFACE_ANGLE_H_
#define SHARED_SURFACE_ANGLE_H_

#include "SharedSurfaceGL.h"
#include "SurfaceFactory.h"
#include "GLLibraryEGL.h"
#include "SurfaceTypes.h"

#include <windows.h>
#ifdef CAIRO_HAS_D2D_SURFACE
#include <d3d10_1.h>
#endif

namespace mozilla {
namespace gl {

class GLContext;

class SharedSurface_ANGLEShareHandle
    : public SharedSurface_GL
{
public:
#ifdef CAIRO_HAS_D2D_SURFACE
    static SharedSurface_ANGLEShareHandle* Create(GLContext* gl, ID3D10Device1* d3d,
                                                  EGLContext context, EGLConfig config,
                                                  const gfxIntSize& size,
                                                  bool hasAlpha);
#endif

    static SharedSurface_ANGLEShareHandle* Cast(SharedSurface* surf) {
        MOZ_ASSERT(surf->Type() == SharedSurfaceType::EGLSurfaceANGLE);

        return (SharedSurface_ANGLEShareHandle*)surf;
    }

protected:
    GLLibraryEGL* const mEGL;
    const EGLContext mContext;
    const EGLSurface mPBuffer;
#ifdef CAIRO_HAS_D2D_SURFACE
    nsRefPtr<ID3D10Texture2D> mTexture;
    nsRefPtr<ID3D10ShaderResourceView> mSRV;

    SharedSurface_ANGLEShareHandle(GLContext* gl,
                                   GLLibraryEGL* egl,
                                   const gfxIntSize& size,
                                   bool hasAlpha,
                                   EGLContext context,
                                   EGLSurface pbuffer,
                                   ID3D1000Texture2D* texture,
                                   ID3D10ShaderResourceView* srv)
        : SharedSurface_GL(SharedSurfaceType::EGLSurfaceANGLE,
                           AttachmentType::Screen,
                           gl,
                           size,
                           hasAlpha)
        , mEGL(egl)
        , mContext(context)
        , mPBuffer(pbuffer)
        , mTexture(texture)
        , mSRV(srv)
    {}
#endif
	

    EGLDisplay Display();

public:
    virtual ~SharedSurface_ANGLEShareHandle();

    virtual void LockProdImpl();
    virtual void UnlockProdImpl();

    virtual void Fence();
    virtual bool WaitSync();

#ifdef CAIRO_HAS_D2D_SURFACE
    // Implementation-specific functions below:
    ID3D10ShaderResourceView* GetSRV() {
        return mSRV;
    }
#endif
};



class SurfaceFactory_ANGLEShareHandle
    : public SurfaceFactory_GL
{
protected:
    GLContext* const mProdGL;
    GLLibraryEGL* const mEGL;
#ifdef CAIRO_HAS_D2D_SURFACE
    nsRefPtr<ID3D10Device1> mConsD3D;
#endif
    EGLContext mContext;
    EGLConfig mConfig;

public:
#ifdef CAIRO_HAS_D2D_SURFACE
    static SurfaceFactory_ANGLEShareHandle* Create(GLContext* gl,
                                                   ID3D10Device1* d3d,
                                                   const SurfaceCaps& caps);
#endif

protected:
#ifdef CAIRO_HAS_D2D_SURFACE
    SurfaceFactory_ANGLEShareHandle(GLContext* gl,
                                    GLLibraryEGL* egl,
                                    ID3D10Device1* d3d,
                                    const SurfaceCaps& caps);

    virtual SharedSurface* CreateShared(const gfxIntSize& size) {
        bool hasAlpha = mReadCaps.alpha;
        return SharedSurface_ANGLEShareHandle::Create(mProdGL, mConsD3D,
                                                      mContext, mConfig,
                                                      size, hasAlpha);
    }
#endif
};

} /* namespace gfx */
} /* namespace mozilla */

#endif /* SHARED_SURFACE_ANGLE_H_ */
