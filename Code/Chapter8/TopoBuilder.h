#pragma once

#include "Common.h"

// Media Foundation headers
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <evr.h>


class CTopoBuilder
{
public:
    CTopoBuilder(void)  {};
    ~CTopoBuilder(void) { ShutdownSource(); };

    HRESULT RenderURL(PCWSTR sURL, HWND videoHwnd);

    IMFTopology* GetTopology(void) { return m_pTopology; }

    HRESULT ShutdownSource(void);

private:
    CComQIPtr<IMFTopology>                  m_pTopology;        // pointer to the topology itself
    CComQIPtr<IMFMediaSource>               m_pSource;          // pointer to the MF source
    CComQIPtr<IMFVideoDisplayControl>       m_pVideoDisplay;    // pointer to the EVR interface IMFVideoDisplayControl
    HWND                                    m_videoHwnd;        // pointer to the target window

    HRESULT CreateMediaSource(PCWSTR sURL);
    HRESULT CreateTopology(void);

    HRESULT AddBranchToPartialTopology(
        CComPtr<IMFPresentationDescriptor> pPresDescriptor, 
        DWORD iStream);

    HRESULT CreateSourceStreamNode(
        CComPtr<IMFPresentationDescriptor> pPresDescriptor, 
        CComPtr<IMFStreamDescriptor> pStreamDescriptor, 
        CComPtr<IMFTopologyNode> &ppNode);
    
    HRESULT CreateOutputNode(
        CComPtr<IMFStreamDescriptor> pSourceSD, 
        HWND hwndVideo, 
        CComPtr<IMFTopologyNode> &pNode);
};

