#pragma once

#include "Common.h"

// Media Foundation headers
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <Ks.h>
#include <Codecapi.h>


class CTranscodeApiTopoBuilder
{
    public:
        CTranscodeApiTopoBuilder(void);
        ~CTranscodeApiTopoBuilder(void);

        // create the transcode topology
        HRESULT CreateTranscodeTopology(PCWSTR pszInput, PCWSTR pszOutput);

        // create the transcode profile based on the specified formats
        HRESULT SetTranscodeProfile(const GUID& audioFormat, const GUID& videoFormat,
            const GUID& containerType);

        // get the topology
        IMFTopology* GetTopology(void) { return m_pTopology; }

        // shut down the source
        void ShutdownSource(void) { if(m_pSource != NULL) m_pSource->Shutdown(); }

    private:
        CComPtr<IMFTranscodeProfile>    m_pTranscodeProfile;
        CComQIPtr<IMFMediaSource>       m_pSource;          // pointer to the MF source
        CComPtr<IMFTopology>            m_pTopology;

        // get all possible output types for all encoders
        HRESULT GetVideoOutputAvailableTypes(const GUID& videoSubtypeGuid, DWORD flags, 
            CComPtr<IMFCollection>& pTypeCollection);

        // helper functions that deal with collections
        HRESULT GetTypeAttributesFromTypeCollection(CComPtr<IMFCollection>& pTypeCollection,
            int typeIndex, CComPtr<IMFAttributes>& pAttrCollection);
        HRESULT GetCollectionElement(CComPtr<IMFCollection>& pCollection, int index,
            CComQIPtr<IUnknown>& pObject);

        // configure the transcode profile attributes
        HRESULT SetAudioAttributes(const GUID& audioFormat);
        HRESULT SetVideoAttributes(const GUID& videoFormat);
        HRESULT SetContainerAttributes(const GUID& containerType);

        // create the media source
        HRESULT CreateMediaSource(PCWSTR sURL);
};

