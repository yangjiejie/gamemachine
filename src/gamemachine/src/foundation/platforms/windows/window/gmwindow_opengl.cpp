﻿#include "stdafx.h"
#include <GL/glew.h>
#include <GL/wglew.h>
#include "gmengine/ui/gmwindow.h"
#include "foundation/gamemachine.h"
#include "gmgl/gmglgraphic_engine.h"

#define EXIT __exit
#define RUN_AND_CHECK(i) if (!(i)) { GM_ASSERT(false); goto EXIT; }

namespace
{
	HWND createTempWindow()
	{
		LPWSTR lpszClassName = L"Gamemachine TempWindowClass";
		WNDCLASS wc;
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = ::DefWindowProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = NULL;
		wc.hIcon = ::LoadIcon(0, IDI_APPLICATION);
		wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)::GetStockObject(WHITE_BRUSH);
		wc.lpszMenuName = NULL;
		wc.lpszClassName = lpszClassName;
		::RegisterClass(&wc);

		HWND wnd = CreateWindow(lpszClassName,
			L"GameMachineTempWindow",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			NULL,
			NULL,
			NULL,
			NULL);
		GM_ASSERT(wnd);
		::ShowWindow(wnd, SW_HIDE);
		::UpdateWindow(wnd);
		return wnd;
	}

	PIXELFORMATDESCRIPTOR getDefaultPixelFormatDescriptor(GMbyte colorDepth, GMbyte alphaBits, GMbyte depthBits, GMbyte stencilBits)
	{
		PIXELFORMATDESCRIPTOR pfd =								//pfd tells windows how we want things to be
		{
			sizeof(PIXELFORMATDESCRIPTOR),						//size of Pixel format descriptor
			1,													//Version Number
			PFD_DRAW_TO_WINDOW |								//must support window
			PFD_SUPPORT_OPENGL |								//must support opengl
			PFD_DOUBLEBUFFER,									//must support double buffer
			PFD_TYPE_RGBA,										//request RGBA format
			colorDepth,											//select color depth
			0, 0, 0, 0, 0, 0,									//color bits ignored
			alphaBits,											//alpha buffer bits
			0,													//shift bit ignored
			0,													//no accumulation buffer
			0, 0, 0, 0,											//accumulation bits ignored
			depthBits,											//z buffer bits
			stencilBits,										//stencil buffer bits
			0,													//no auxiliary buffer
			PFD_MAIN_PLANE,										//main drawing layer
			0,													//reserved
			0, 0, 0												//layer masks ignored
		};
		return pfd;
	}
}

GM_PRIVATE_OBJECT(GMWindow_OpenGL)
{
	BYTE depthBits, stencilBits;
	HDC hDC = 0;
	GMSharedPtr<HGLRC> hRC;
	GMWindow_OpenGL* parent = nullptr;
};

class GMWindow_OpenGL : public GMWindow
{
	GM_DECLARE_PRIVATE_AND_BASE(GMWindow_OpenGL, GMWindow)
	GM_FRIEND_CLASS(GMGLRenderContext)

public:
	GMWindow_OpenGL(IWindow* parent);
	~GMWindow_OpenGL();

public:
	inline GMSharedPtr<HGLRC> getGLRC() GM_NOEXCEPT
	{
		D(d);
		return d->hRC;
	}

public:
	virtual void msgProc(const GMMessage& message) override;
	virtual IGraphicEngine* getGraphicEngine() override;
	virtual const IRenderContext* getContext() override;

protected:
	virtual void onWindowCreated(const GMWindowDesc& wndAttrs) override;

private:
	void swapBuffers() const;
	void dispose();
};

class GMGLRenderContext : public GMRenderContext
{
public:
	GMGLRenderContext(GMWindow_OpenGL* window)
	{
		windowImpl = window;
	}

	virtual void switchToContext() const override
	{
		D_OF(d, windowImpl);
		wglMakeCurrent(d->hDC, *d->hRC);
	}

private:
	GMWindow_OpenGL* windowImpl;
};

GMWindow_OpenGL::GMWindow_OpenGL(IWindow* parent)
{
	D(d);
	d->depthBits = 24;
	d->stencilBits = 8;

	if (parent)
	{
		GMWindow_OpenGL* parentGLWindow = dynamic_cast<GMWindow_OpenGL*>(parent);
		if (parentGLWindow)
			d->parent = parentGLWindow;
	}
}

GMWindow_OpenGL::~GMWindow_OpenGL()
{
	dispose();
}

void GMWindow_OpenGL::onWindowCreated(const GMWindowDesc& wndAttrs)
{
	D(d);
	GMWindowDesc attrs = wndAttrs;
	attrs.dwExStyle |= WS_EX_CLIENTEDGE;
	attrs.dwStyle |= WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU | WS_BORDER | WS_CAPTION;

	const GMbyte colorDepth = 32, alphaBits = 8;

	HWND tmpWnd = NULL;
	HDC tmpDC = NULL;
	HGLRC tmpRC = NULL;
	RUN_AND_CHECK(GMWindowFactory::createTempWindow(colorDepth, alphaBits, d->depthBits, d->stencilBits, tmpWnd, tmpDC, tmpRC));
	RUN_AND_CHECK(GLEW_OK == glewInit());

	// 开始创建真正的Window
	GMWindowHandle wnd = getWindowHandle();
	RUN_AND_CHECK(d->hDC = ::GetDC(wnd));

	GMint32 pixAttribs[] =
	{
		WGL_DRAW_TO_WINDOW_ARB,	GL_TRUE,
		WGL_SUPPORT_OPENGL_ARB,	GL_TRUE,
		WGL_DOUBLE_BUFFER_ARB,	GL_TRUE,
		WGL_SAMPLE_BUFFERS_EXT,	GL_TRUE,
		WGL_ACCELERATION_ARB,	WGL_FULL_ACCELERATION_ARB,
		WGL_COLOR_BITS_ARB,		colorDepth,
		WGL_ALPHA_BITS_ARB,		alphaBits,
		WGL_DEPTH_BITS_ARB,		d->depthBits,
		WGL_STENCIL_BITS_ARB,	d->stencilBits,
		WGL_SAMPLES_ARB,		wndAttrs.samples,
		0
	};

	GMint32 nFormat = 0;
	GMuint32 nCount = 0;
	RUN_AND_CHECK(wglChoosePixelFormatARB(d->hDC, &pixAttribs[0], NULL, 1, &nFormat, (UINT*)&nCount));
	RUN_AND_CHECK(wglMakeCurrent(NULL, NULL));
	RUN_AND_CHECK(GMWindowFactory::destroyTempWindow(tmpWnd, tmpDC, tmpRC));

	static PIXELFORMATDESCRIPTOR pfd = getDefaultPixelFormatDescriptor(colorDepth, alphaBits, d->depthBits, d->stencilBits);
	RUN_AND_CHECK(::SetPixelFormat(d->hDC, nFormat, &pfd));
	if (!d->parent)
	{
		d->hRC = GMSharedPtr<HGLRC>(new HGLRC(wglCreateContext(d->hDC)), [](HGLRC* hRC) {
			if (!wglDeleteContext(*hRC))
				gm_error(gm_dbg_wrap("release Rendering Context failed."));
			GM_delete(hRC);
		});
	}
	else
	{
		RUN_AND_CHECK(d->hRC = d->parent->getGLRC());
	}

	RUN_AND_CHECK(wglMakeCurrent(d->hDC, *d->hRC));
	return;

EXIT:
	// 走到这里来说明流程失败
	dispose();
	GMWindowFactory::destroyTempWindow(tmpWnd, tmpDC, tmpRC);
	wglMakeCurrent(NULL, NULL);
	return;
}

IGraphicEngine* GMWindow_OpenGL::getGraphicEngine()
{
	D_BASE(d, Base);
	if (!d->context)
		return getContext()->getEngine();

	if (!d->engine)
		d->engine = gm_makeOwnedPtr<GMGLGraphicEngine>(getContext());

	return d->engine.get();
}

const IRenderContext* GMWindow_OpenGL::getContext()
{
	D_BASE(d, Base);
	if (!d->context)
	{
		GMGLRenderContext* context = new GMGLRenderContext(this);
		d->context.reset(context);
		context->setWindow(this);
		context->setEngine(getGraphicEngine());
	}
	return d->context.get();
}

void GMWindow_OpenGL::swapBuffers() const
{
	D(d);
	::SwapBuffers(d->hDC);
}

void GMWindow_OpenGL::msgProc(const GMMessage& message)
{
	Base::msgProc(message);
	if (message.msgType == GameMachineMessageType::FrameUpdate)
		swapBuffers();
}

void GMWindow_OpenGL::dispose()
{
	D(d);
	::SetWindowLongPtr(getWindowHandle(), GWLP_USERDATA, NULL);
	
	GMWindowHandle wnd = getWindowHandle();
	if (d->hDC)
	{
		::ReleaseDC(wnd, d->hDC);
		d->hDC = 0;
	}
}

bool GMWindowFactory::createWindowWithOpenGL(GMInstance instance, IWindow* parent, OUT IWindow** window)
{
	if (window)
	{
		(*window) = new GMWindow_OpenGL(parent);
		if (*window)
			return true;
	}
	return false;
}

bool GMWindowFactory::createTempWindow(GMbyte colorDepth, GMbyte alphaBits, GMbyte depthBits, GMbyte stencilBits, OUT GMWindowHandle& tmpWnd, OUT GMDeviceContextHandle& tmpDC, OUT GMOpenGLRenderContextHandle& tmpRC)
{
	auto pfd = ::getDefaultPixelFormatDescriptor(colorDepth, alphaBits, depthBits, stencilBits);
	GMint32 pixelFormat = 0;

	RUN_AND_CHECK(tmpWnd = ::createTempWindow());
	RUN_AND_CHECK(tmpDC = ::GetDC(tmpWnd));
	RUN_AND_CHECK(pixelFormat = ::ChoosePixelFormat(tmpDC, &pfd));
	RUN_AND_CHECK(::SetPixelFormat(tmpDC, pixelFormat, &pfd));
	RUN_AND_CHECK(tmpRC = wglCreateContext(tmpDC));
	RUN_AND_CHECK(wglMakeCurrent(tmpDC, tmpRC));
	return true;

EXIT:
	return false;
}

bool GMWindowFactory::destroyTempWindow(GMWindowHandle tmpWnd, GMDeviceContextHandle tmpDC, GMOpenGLRenderContextHandle tmpRC)
{
	BOOL sucRC = FALSE;
	BOOL sucWnd = FALSE;
	if (tmpDC)
		ReleaseDC(tmpWnd, tmpDC);

	if (tmpRC)
		sucRC = wglDeleteContext(tmpRC);

	if (tmpWnd)
		sucWnd = DestroyWindow(tmpWnd);

	return sucRC && sucWnd;
}