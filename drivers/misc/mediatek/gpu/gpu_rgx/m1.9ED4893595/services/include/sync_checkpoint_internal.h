/*************************************************************************/ /*!
@File
@Title          Services internal synchronisation checkpoint interface header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Defines the internal server interface for services
                synchronisation checkpoints.
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#ifndef __SYNC_CHECKPOINT__
#define __SYNC_CHECKPOINT__

#include "img_types.h"
#include "sync_checkpoint_internal_fw.h"
#include "sync_checkpoint_external.h"
#include "sync_checkpoint.h"
#include "ra.h"
#include "dllist.h"
#include "lock.h"
#include "devicemem.h"
#include "rgx_fwif_shared.h"
typedef struct _PVRSRV_DEVICE_NODE_ PVRSRV_DEVICE_NODE;

#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
struct SYNC_CHECKPOINT_RECORD;
#endif

/*
	Private structures
*/

typedef struct _SYNC_CHECKPOINT_CONTEXT_CTL_ _SYNC_CHECKPOINT_CONTEXT_CTL;

typedef struct _SYNC_CHECKPOINT_CONTEXT_
{
	PVRSRV_DEVICE_NODE     			*psDevNode;
	IMG_CHAR						azName[PVRSRV_SYNC_NAME_LENGTH];       /*!< Name of the RA */
	RA_ARENA						*psSubAllocRA;                         /*!< RA context */
	IMG_CHAR						azSpanName[PVRSRV_SYNC_NAME_LENGTH];   /*!< Name of the span RA */
	RA_ARENA						*psSpanRA;                             /*!< RA used for span management of SubAllocRA */
	ATOMIC_T						hRefCount;                             /*!< Ref count for this context */
	ATOMIC_T						hCheckpointCount;                      /*!< Checkpoint count for this context */
	POS_LOCK						hLock;
	_SYNC_CHECKPOINT_CONTEXT_CTL	*psContextCtl;
} _SYNC_CHECKPOINT_CONTEXT;

typedef struct _SYNC_CHECKPOINT_BLOCK_
{
	ATOMIC_T                  hRefCount;                  /*!< Ref count for this sync block */
	POS_LOCK                  hLock;
	_SYNC_CHECKPOINT_CONTEXT  *psContext;                 /*!< Our copy of the services connection */
	PVRSRV_DEVICE_NODE        *psDevNode;
	IMG_UINT32                ui32SyncBlockSize;          /*!< Size of the sync checkpoint block */
	IMG_UINT32                ui32FirmwareAddr;           /*!< Firmware address */
	DEVMEM_MEMDESC            *hMemDesc;                  /*!< DevMem allocation for block */
	volatile IMG_UINT32       *pui32LinAddr;              /*!< Server-code CPU mapping */
	IMG_UINT64                uiSpanBase;                 /*!< Base of this import (FW DevMem) in the span RA */
	DLLIST_NODE               sListNode;                  /*!< List node for the sync chkpt block list */
} SYNC_CHECKPOINT_BLOCK;

typedef struct SYNC_CHECKPOINT_RECORD* PSYNC_CHECKPOINT_RECORD_HANDLE;

typedef struct _SYNC_CHECKPOINT_
{
	//_SYNC_CHECKPOINT_CONTEXT		*psContext;				/*!< pointer to the parent context of this checkpoint */
	/* A sync checkpoint is assigned a unique ID, to avoid any confusion should
	 * the same memory be re-used later for a different checkpoint
	 */
	IMG_UINT32                      ui32UID;                 /*!< Unique ID assigned to sync checkpoint (to distinguish checkpoints if memory is re-used)*/
	POS_LOCK                        hLock;
	ATOMIC_T                        hRefCount;               /*!< Ref count for this sync */
	ATOMIC_T                        hEnqueuedCCBCount;       /*!< Num times sync has been put in CCBs */
	SYNC_CHECKPOINT_BLOCK           *psSyncCheckpointBlock;  /*!< Synchronisation block this checkpoint is allocated on */
	IMG_UINT64                      uiSpanAddr;              /*!< Span address of the sync */
	volatile _SYNC_CHECKPOINT_FW_OBJ *psSyncCheckpointFwObj; /*!< CPU view of the data held in the sync block */
	PRGXFWIF_UFO_ADDR               sCheckpointUFOAddr;      /*!< PRGXFWIF_UFO_ADDR struct used to pass update address to FW */
	IMG_CHAR                        azName[PVRSRV_SYNC_NAME_LENGTH]; /*!< Name of the checkpoint */
	PVRSRV_TIMELINE                 hTimeline;               /*!< Timeline on which this sync checkpoint was created */
	IMG_UINT32                      ui32ValidationCheck;
#if defined(PVRSRV_ENABLE_FULL_SYNC_TRACKING)
	PSYNC_CHECKPOINT_RECORD_HANDLE  hRecord;                /*!< Sync record handle */
#endif
	DLLIST_NODE                     sListNode;              /*!< List node for the global sync chkpt list */
	DLLIST_NODE                     sDeferredFreeListNode;  /*!< List node for the deferred free sync chkpt list */
} _SYNC_CHECKPOINT;

/*************************************************************************/ /*!
@Function       SyncCheckpointGetFirmwareAddr

@Description    .

@Input          psSyncCheckpoint        Synchronisation checkpoint to get
                                        the firmware address of

@Return         The firmware address of the sync checkpoint

*/
/*****************************************************************************/
IMG_UINT32
SyncCheckpointGetFirmwareAddr(PSYNC_CHECKPOINT psSyncCheckpoint);

/*************************************************************************/ /*!
@Function       SyncCheckpointGetFirmwareAddrFromList

@Description    .

@Input          psSyncCheckpoint       Synchronisation checkpoint from
                                        which to get the firmware address

@Return         The firmware address of the sync checkpoint

*/
/*****************************************************************************/
IMG_UINT32
SyncCheckpointGetFirmwareAddrFromList(PSYNC_CHECKPOINT psSyncCheckpoint);

/*************************************************************************/ /*!
@Function       SyncCheckpointCCBEnqueued

@Description    Increment the CCB enqueued reference count for a
                synchronisation checkpoint. This indicates how many FW
                operations (checks/update) have been placed into CCBs for the
                sync checkpoint.
                When the FW services these operation, it increments its own
                reference count. When these two values are equal, we know
                there are not outstanding FW operating for the checkpoint
                in any CCB.

@Input          psSyncCheckpoint        Synchronisation checkpoint for which
                                        to increment the enqueued reference
                                        count

@Return         None

*/
/*****************************************************************************/
void
SyncCheckpointCCBEnqueued(PSYNC_CHECKPOINT psSyncCheckpoint);

/*************************************************************************/ /*!
@Function       SyncCheckpointGetEnqueuedCount

@Description    .

@Input          psSyncCheckpoint        Synchronisation checkpoint to get
                                        the enqueued count of

@Return         The enqueued count of the sync checkpoint
                (i.e. the number of FW operations (checks or updates)
                 currently enqueued in CCBs for the sync checkpoint)

*/
/*****************************************************************************/
IMG_UINT32
SyncCheckpointGetEnqueuedCount(PSYNC_CHECKPOINT psSyncCheckpoint);

/*************************************************************************/ /*!
@Function       SyncCheckpointGetId

@Description    .

@Input          psSyncCheckpoint        Synchronisation checkpoint to get
                                        the unique Id of

@Return         The unique Id of the sync checkpoint

*/
/*****************************************************************************/
IMG_UINT32
SyncCheckpointGetId(PSYNC_CHECKPOINT psSyncCheckpoint);

/*************************************************************************/ /*!
@Function       SyncCheckpointGetTimeline

@Description    .

@Input          psSyncCheckpoint        Synchronisation checkpoint to get
                                        the parent timeline of

@Return         The parent timeline of the sync checkpoint

*/
/*****************************************************************************/
PVRSRV_TIMELINE
SyncCheckpointGetTimeline(PSYNC_CHECKPOINT psSyncCheckpoint);

/*************************************************************************/ /*!
@Function       SyncCheckpointGetRGXFWIFUFOAddr

@Description    .

@Input          psSyncCheckpoint        Synchronisation checkpoint to get
                                        the PRGXFWIF_UFO_ADDR of

@Return         The PRGXFWIF_UFO_ADDR of the sync checkpoint, used when
                providing the update in server kick code.

*/
/*****************************************************************************/
PRGXFWIF_UFO_ADDR*
SyncCheckpointGetRGXFWIFUFOAddr(PSYNC_CHECKPOINT psSyncCheckpoint);

#endif	/* __SYNC_CHECKPOINT__ */
