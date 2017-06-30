#pragma once

#include <assert.h>
#include <windows.h>
#include <atlbase.h>

using namespace std;
using namespace ATL;

static void CheckHR(HRESULT hr);

#define BREAK_ON_FAIL(value)            if(FAILED(value)) break;
#define BREAK_ON_NULL(value, newHr)     if(value == NULL) { hr = newHr; break; }
