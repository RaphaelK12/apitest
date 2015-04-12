#pragma once

// Is there anything more annoying? Honestly.
#define _CRT_SECURE_NO_WARNINGS 1

#include <assert.h>

#include <algorithm>
#include <list>
#include <map>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <tuple>
#include <vector>

#ifdef _WIN32
#   define NOMINMAX 1
#   include <Windows.h>
#   include <d3d11.h>

#if WITH_D3D12
	#include <d3d12.h>
#endif
#endif

// Needs to be included before SDL.h
#include "framework/console.h"
#include "framework/mathlib.h"

#include "SDL.h"
#include "framework/dds_helper.h"
#include "GL/glextensions.h"

#ifdef _WIN32
#   pragma warning ( disable : 4351 )
#endif

template <typename T>
void SafeRelease(T*& _p) { if (_p) { _p->Release(); _p = nullptr; } }

template <typename T> 
void SafeDelete(T*& _p) { delete _p; _p = nullptr; }

template <typename T> 
void SafeDeleteArray(T*& _p) { delete [] _p; _p = nullptr; }


#define ArraySize(_arr) ( sizeof((_arr)) / sizeof((_arr)[0]) )

// Auto-releasing wrapper for COM pointers
template <typename T>
struct comptr
{
	T * p;

	comptr() : p(nullptr) {}
	comptr(T * other) : p(other) {}
	comptr(const comptr<T> & other) : p(other.p)
	{
		if (p) p->AddRef();
	}

	void release()
	{
		if (p) { p->Release(); p = nullptr; }
	}
	~comptr()
	{
		release();
	}

	comptr<T> & operator = (T * other)
	{
		release(); p = other; return *this;
	}
	comptr<T> & operator = (const comptr<T> & other)
	{
		release(); p = other.p; p->AddRef(); return *this;
	}

	T ** operator & () { return &p; }
	T * operator * () { return p; }
	T * operator -> () { return p; }
	operator T * () { return p; }
};