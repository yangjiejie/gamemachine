﻿#include "stdafx.h"
#include "gmlua_functions.h"

#define POP_GUARD() \
struct __PopGuard							\
{											\
	__PopGuard(lua_State* __L) : m_L(__L) {}\
	~__PopGuard() { lua_pop(m_L, 1); }		\
	lua_State* m_L;							\
} __guard(L);

BEGIN_NS
using namespace luaapi;

GMint32 GMArgumentHelper::getArgumentsCount(GMLuaCoreState* L)
{
	GM_CHECK_LUA_STACK_BALANCE(0);
	return lua_gettop(L);
}

const char* GMArgumentHelper::popArgumentAsString(GMLuaCoreState* L, const GMString& invoker)
{
	GM_CHECK_LUA_STACK_BALANCE(-1);
	POP_GUARD();
	if (!lua_isstring(L, -1))
		gm_error(gm_dbg_wrap("GMArgumentHelper::popArgumentAsString: {0}: argument is '{1}', not a string."), invoker, lua_typename(L, -1));
	return luaL_optstring(L, -1, "");
}

GMVariant GMArgumentHelper::peekArgument(GMLuaCoreState* L, GMint32 index, const GMString& invoker)
{
	GM_CHECK_LUA_STACK_BALANCE(0);
	GMVariant v = GMLua(L).get(index);
	if (v.isInvalid())
		gm_error(gm_dbg_wrap("GMArgumentHelper::popArgument: {0}: Cannot match the type of the argument. The type of the argument is {1}."), invoker, lua_typename(L, -1));
	return v;
}

GMVariant GMArgumentHelper::popArgument(GMLuaCoreState* L, const GMString& invoker)
{
	GM_CHECK_LUA_STACK_BALANCE(-1);
	GMLua lua(L);
	bool isFunction = false;
	GMLuaReference funcRef = lua.popFunction(&isFunction);
	if (isFunction)
		return funcRef; // 不需要pop，已经pop过了

	POP_GUARD();
	GMVariant v = lua.getTop();
	if (v.isInvalid())
		gm_error(gm_dbg_wrap("GMArgumentHelper::popArgument: {0}: Cannot match the type of the argument. The type of the argument is {1}."), invoker, lua_typename(L, -1));
	return v;
}

GMVariant GMArgumentHelper::popArgumentAsVec2(GMLuaCoreState* L, const GMString& invoker)
{
	GM_CHECK_LUA_STACK_BALANCE(-1);
	POP_GUARD();
	GMVec2 v;
	if (!GMLua(L).popVector(v))
	{
		gm_error(gm_dbg_wrap("GMLua::popArgumentAsVec2: {0}: argument is not a GMVec2."), invoker);
		return GMVariant();
	}
	return GMVariant(std::move(v));
}

GMVariant GMArgumentHelper::popArgumentAsVec3(GMLuaCoreState* L, const GMString& invoker)
{
	GM_CHECK_LUA_STACK_BALANCE(-1);
	POP_GUARD();
	GMVec3 v;
	if (!GMLua(L).popVector(v))
	{
		gm_error(gm_dbg_wrap("GMLua::popArgumentAsVec3: {0}: argument is not a GMVec3."), invoker);
		return GMVariant();
	}
	return GMVariant(std::move(v));
}

GMVariant GMArgumentHelper::popArgumentAsVec4(GMLuaCoreState* L, const GMString& invoker)
{
	GM_CHECK_LUA_STACK_BALANCE(-1);
	POP_GUARD();
	GMVec4 v;
	if (!GMLua(L).popVector(v))
	{
		gm_error(gm_dbg_wrap("GMLua::popArgumentAsVec4: {0}: argument is not a GMVec4."), invoker);
		return GMVariant();
	}
	return GMVariant(std::move(v));
}

GMVariant GMArgumentHelper::popArgumentAsMat4(GMLuaCoreState* L, const GMString& invoker)
{
	GM_CHECK_LUA_STACK_BALANCE(-1);
	POP_GUARD();
	GMMat4 v;
	if (!GMLua(L).popMatrix(v))
	{
		gm_error(gm_dbg_wrap("GMLua::popArgumentAsVec4: {0}: argument is not a GMMat4."), invoker);
		return GMVariant();
	}
	return GMVariant(std::move(v));
}

bool GMArgumentHelper::popArgumentAsObject(GMLuaCoreState* L, REF GMObject& obj, const GMString& invoker)
{
	GM_CHECK_LUA_STACK_BALANCE(-1);
	POP_GUARD();
	if (!GMLua(L).popTable(obj))
	{
		gm_error(gm_dbg_wrap("GMLua::popArgumentAsObject: {0}: argument is not an object."), invoker);
		return false;
	}
	return true;
}

void GMReturnValues::pushArgument(const GMVariant& arg)
{
	if (arg.isObject())
	{
		m_L.pushNewTable(*arg.toObject());
	}
	else
	{
		m_L.push(arg);
	}
}

END_NS