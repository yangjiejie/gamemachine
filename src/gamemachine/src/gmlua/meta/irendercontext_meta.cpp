﻿#include "stdafx.h"
#include "irendercontext_meta.h"
#include <gamemachine.h>
#include <gmlua.h>
#include "igraphicengine_meta.h"

using namespace luaapi;

#define NAME "IRenderContext"

bool IRenderContextProxy::registerMeta()
{
	GM_META_FUNCTION(getWindow);
	GM_META_FUNCTION(getEngine);
	return Base::registerMeta();
}

void IRenderContextProxy::set(const IRenderContext* o)
{
	Base::set(const_cast<IRenderContext*>(o));
}

void IRenderContextProxy::setLuaCoreState(GMLuaCoreState* l)
{
	D_BASE(d, Base);
	d->l = l;
}

IRenderContextProxy::IRenderContextProxy(const IRenderContextProxy& rhs)
	: GMAnyProxy(rhs.getLuaCoreState(), const_cast<IRenderContext*>(static_cast<const IRenderContext*>(rhs.get())))
{
}

/*
 * getWindow([self])
 */
GM_LUA_PROXY_IMPL(IRenderContextProxy, getWindow)
{
	static const GMString s_invoker = NAME ".getWindow";
	GM_LUA_CHECK_ARG_COUNT(L, 1, NAME ".getWindow");
	IRenderContextProxy self(L);
	GMArgumentHelper::popArgumentAsObject(L, self, s_invoker); //self
	if (self)
	{
		IWindowProxy window(L);
		window.set(self->getWindow());
		return GMReturnValues(L, window);
	}
	return GMReturnValues();
}

/*
 * getWindow([self])
 */
GM_LUA_PROXY_IMPL(IRenderContextProxy, getEngine)
{
	static const GMString s_invoker = NAME ".getEngine";
	GM_LUA_CHECK_ARG_COUNT(L, 1, NAME ".getEngine");
	IRenderContextProxy self(L);
	GMArgumentHelper::popArgumentAsObject(L, self, s_invoker); //self
	if (self)
	{
		IGraphicEngineProxy engine(L);
		engine.set(self->getEngine());
		return GMReturnValues(L, engine);
	}
	return GMReturnValues();
}