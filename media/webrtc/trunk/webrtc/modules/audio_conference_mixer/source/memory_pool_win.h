/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_MEMORY_POOL_WINDOWS_H_
#define WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_MEMORY_POOL_WINDOWS_H_

#include <assert.h>
#include <windows.h>

#include "aligned_malloc.h"
#include "atomic32.h"
#include "typedefs.h"

namespace webrtc {
template<class MemoryType> struct MemoryPoolItem;

template<class MemoryType>
struct MemoryPoolItemPayload
{
    MemoryPoolItemPayload()
        : memoryType(),
          base(NULL)
    {
    }
    MemoryType                  memoryType;
    MemoryPoolItem<MemoryType>* base;
};

template<class MemoryType>
struct MemoryPoolItem
{
    // Atomic single linked list entry header.
    MSPS_SLIST_ENTRY itemEntry;
    // Atomic single linked list payload.
    MemoryPoolItemPayload<MemoryType>* payload;
};

template<class MemoryType>
class MemoryPoolImpl
{
public:
    // MemoryPool functions.
    WebRtc_Word32 PopMemory(MemoryType*&  memory);
    WebRtc_Word32 PushMemory(MemoryType*& memory);

    MemoryPoolImpl(WebRtc_Word32 /*initialPoolSize*/);
    ~MemoryPoolImpl();

    // Atomic functions.
    WebRtc_Word32 Terminate();
    bool Initialize();
private:
    // Non-atomic function.
    MemoryPoolItem<MemoryType>* CreateMemory();

    // Windows implementation of single linked atomic list, documented here:
    // http://msdn.microsoft.com/en-us/library/ms686962(VS.85).aspx

    // Atomic single linked list head.
    MSPS_PSLIST_HEADER _pListHead;

    Atomic32 _createdMemory;
    Atomic32 _outstandingMemory;
};

template<class MemoryType>
MemoryPoolImpl<MemoryType>::MemoryPoolImpl(
    WebRtc_Word32 /*initialPoolSize*/)
    : _pListHead(NULL),
      _createdMemory(0),
      _outstandingMemory(0)
{
}

template<class MemoryType>
MemoryPoolImpl<MemoryType>::~MemoryPoolImpl()
{
    Terminate();
    if(_pListHead != NULL)
    {
        AlignedFree(reinterpret_cast<void*>(_pListHead));
        _pListHead = NULL;
    }
    // Trigger assert if there is outstanding memory.
    assert(_createdMemory.Value() == 0);
    assert(_outstandingMemory.Value() == 0);
}

template<class MemoryType>
WebRtc_Word32 MemoryPoolImpl<MemoryType>::PopMemory(MemoryType*& memory)
{
    MSPS_PSLIST_ENTRY pListEntry = InterlockedPopEntrySList_kex(_pListHead);
    if(pListEntry == NULL)
    {
        MemoryPoolItem<MemoryType>* item = CreateMemory();
        if(item == NULL)
        {
            return -1;
        }
        pListEntry = &(item->itemEntry);
    }
    ++_outstandingMemory;
    memory = &((MemoryPoolItem<MemoryType>*)pListEntry)->payload->memoryType;
    return 0;
}

template<class MemoryType>
WebRtc_Word32 MemoryPoolImpl<MemoryType>::PushMemory(MemoryType*& memory)
{
    if(memory == NULL)
    {
        return -1;
    }

    MemoryPoolItem<MemoryType>* item =
        ((MemoryPoolItemPayload<MemoryType>*)memory)->base;

    const WebRtc_Word32 usedItems  = --_outstandingMemory;
    const WebRtc_Word32 totalItems = _createdMemory.Value();
    const WebRtc_Word32 freeItems  = totalItems - usedItems;
    if(freeItems < 0)
    {
        assert(false);
        delete item->payload;
        AlignedFree(item);
        return -1;
    }
    if(freeItems >= totalItems>>1)
    {
        delete item->payload;
        AlignedFree(item);
        --_createdMemory;
        return 0;
    }
    InterlockedPushEntrySList_kex(_pListHead,&(item->itemEntry));
    return 0;
}

template<class MemoryType>
bool MemoryPoolImpl<MemoryType>::Initialize()
{
    _pListHead = (MSPS_PSLIST_HEADER)AlignedMalloc(sizeof(MSPS_SLIST_HEADER),
                                              MEMORY_ALLOCATION_ALIGNMENT);
    if(_pListHead == NULL)
    {
        return false;
    }
    InitializeSListHead_kex(_pListHead);
    return true;
}

template<class MemoryType>
WebRtc_Word32 MemoryPoolImpl<MemoryType>::Terminate()
{
    WebRtc_Word32 itemsFreed = 0;
    MSPS_PSLIST_ENTRY pListEntry = InterlockedPopEntrySList_kex(_pListHead);
    while(pListEntry != NULL)
    {
        MemoryPoolItem<MemoryType>* item = ((MemoryPoolItem<MemoryType>*)pListEntry);
        delete item->payload;
        AlignedFree(item);
        --_createdMemory;
        itemsFreed++;
        pListEntry = InterlockedPopEntrySList_kex(_pListHead);
    }
    return itemsFreed;
}

template<class MemoryType>
MemoryPoolItem<MemoryType>* MemoryPoolImpl<MemoryType>::CreateMemory()
{
    MemoryPoolItem<MemoryType>* returnValue = (MemoryPoolItem<MemoryType>*)
        AlignedMalloc(sizeof(MemoryPoolItem<MemoryType>),
                      MEMORY_ALLOCATION_ALIGNMENT);
    if(returnValue == NULL)
    {
        return NULL;
    }

    returnValue->payload = new MemoryPoolItemPayload<MemoryType>();
    if(returnValue->payload == NULL)
    {
        delete returnValue;
        return NULL;
    }
    returnValue->payload->base = returnValue;
    ++_createdMemory;
    return returnValue;
}
} // namespace webrtc

#endif // WEBRTC_MODULES_AUDIO_CONFERENCE_MIXER_SOURCE_MEMORY_POOL_WINDOWS_H_
