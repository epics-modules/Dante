// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>

#ifdef _WIN32
#include <tchar.h>
#define WIN32_LEAN_AND_MEAN             // Escludere gli elementi utilizzati di rado dalle intestazioni di Windows
#define NOMINMAX						// Evita la definizione delle macro "MIN" e "MAX".

// File di intestazione di Windows:
#include <windows.h>
#endif

#include <iostream>