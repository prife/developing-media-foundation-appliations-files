#pragma once

#include "Common.h"

// Media Foundation headers
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <evr.h>
#include <Wmcontainer.h>
#include <InitGuid.h>

#include <new>

#include "HttpOutputStreamActivate.h"


class CTopoBuilder
{
    public:
        CTopoBuilder(void)  { m_addRenderers = false; m_addNetworkSink = false; };
        ~CTopoBuilder(void) { ShutdownSource(); };

        HRESULT RenderURL(PCWSTR fileUrl, HWND videoHwnd, bool addNetwork);

        IMFTopology* GetTopology(void) { return m_pTopology; }

        HRESULT ShutdownSource(void);

    private:
        CComQIPtr<IMFTopology>                  m_pTopology;     // the topology itself
        CComQIPtr<IMFMediaSource>               m_pSource;       // the MF source
        CComQIPtr<IMFVideoDisplayControl>       m_pVideoDisplay; // pointer to the mixer
        HWND                                    m_videoHwnd;     // the target window

        CComPtr<IMFActivate> m_pNetworkSinkActivate;
        DWORD m_nextNetworkSinkStreamIndex;

        bool m_addRenderers;
        bool m_addNetworkSink;

        HRESULT CreateMediaSource(PCWSTR sURL);
        HRESULT CreateNetworkSink(DWORD requestPort);
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
            IMFTopologyNode* pSourceNode,
            IMFTopologyNode** ppOutputNode);

        HRESULT CreateTeeNetworkTwig(IMFStreamDescriptor* pStreamDescriptor, 
            IMFTopologyNode* pRendererNode, IMFTopologyNode** ppTeeNode);
};

