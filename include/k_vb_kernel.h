/* Copyright (c) 2023, Canaan Bright Sight Co., Ltd
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef K_VB_KERNEL_H
#define K_VB_KERNEL_H

#include <rtthread.h>
#include "comm/k_vb_comm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One distinct block a holder holds. Private to VB. */
typedef struct vb_holder_slot
{
    void *blk;
    k_u32 count;
} vb_holder_slot;

/* Kernel drivers embed a holder, one per channel, layer, open file or the
 * like, and serialize its use. vb_holder_init() it with its capacity, the
 * most distinct blocks it holds at once (its queue depth): a reference past
 * that fails with K_ERR_VB_BUSY, for this holder alone. Drop every
 * reference and vb_holder_deinit() it before its storage goes away. Holder
 * operations other than init and deinit do not sleep. */
typedef struct vb_holder
{
    /* Private to VB. */
    vb_holder_slot *slots;
    k_u32 capacity;
    k_u32 uid;
    k_u32 refs;
    const char *name;
} vb_holder;

/* One block reference as a holder took it. Enough to drop it again, and to
 * name the block to other modules and user space. */
typedef struct
{
    k_vb_blk_handle handle;
    k_u32 pool_id;
    k_u64 phys_addr;
    k_u64 blk_size;
    k_u32 alloc_gen;    /* tells this allocation from later ones at phys_addr */
} vb_ref_t;

/* @uid: the module, for statistics; @name: for logs. Allocates @capacity
 * slots (1 to VB_HOLDER_MAX_CAPACITY): thread context, may sleep;
 * K_ERR_VB_NOMEM or K_ERR_VB_ILLEGAL_PARAM, and @h unusable, on failure. */
#define VB_HOLDER_MAX_CAPACITY  1024
extern k_s32 vb_holder_init(vb_holder *h, k_u32 uid, const char *name, k_u32 capacity);
/* Free what vb_holder_init() allocated. @h must hold nothing. Thread
 * context. */
extern void vb_holder_deinit(vb_holder *h);
/* A free block of pool @pool_id (VB_INVALID_POOLID: the smallest common pool
 * that fits) of at least @blk_size bytes, held by @h. K_ERR_VB_NOBUF if none. */
extern k_s32 vb_holder_get_blk(vb_holder *h, k_u32 pool_id, k_u64 blk_size, vb_ref_t *ref);
/* Drop one reference of @h on the block of @pool_id at @phys_addr.
 * K_ERR_VB_NOT_PERM, changing nothing, if @h holds none on it. */
extern k_s32 vb_ref_put(vb_holder *h, k_u32 pool_id, k_u64 phys_addr);
/* Drop every reference @h holds. Returns how many there were. */
extern k_u32 vb_holder_release_all(vb_holder *h);
/* References @h holds on all blocks. */
extern k_u32 vb_holder_refs(vb_holder *h);

/*****************************************************************************
 prototype    : vb_create_pool
 Description  : create a private VB pool
 input        : blk_cnt     : count of buffer blocks
                blk_size    : size of buffer blocks
 output       : pool_id     : pointer to the ID of pool created
 return value : k_s32         : 0 -- success; !0 : failure
*****************************************************************************/
extern k_s32 vb_create_pool(k_u32 *pool_id, k_u32 blk_cnt, k_u64 blk_size,
                            const char *mmz_name, const char *buf_name, k_vb_remap_mode vb_remap_mode);

/*****************************************************************************
 prototype    : vb_pool_put
 Description  : give up a private VB pool the caller created. Never waits for
                blocks other modules still hold (VDEC keeping a stream block
                until the firmware returns it, VO showing the last frame): the
                pool hands out no more blocks and is gone for lookups by id
                alone; its memory is freed once the last block held comes
                back. Its id goes to a new pool meanwhile only if no other id
                is free. Those holders keep using handles and pool id plus
                address as before. The caller forgets the id. Do not retry.
                Thread context; may sleep.
 input        : pool_id     : ID of a private pool the caller owns
 output       : NULL
 return value : k_s32         : 0 -- given up;
                K_ERR_VB_UNEXIST -- no such pool;
                K_ERR_VB_ILLEGAL_PARAM / K_ERR_VB_NOT_PERM -- bad or common pool
*****************************************************************************/
extern k_s32 vb_pool_put(k_u32 pool_id);

/*****************************************************************************
 Prototype    : vb_blk_handle_to_kern
 Description  : convert handle to kernel virtual address of a buffer
 Input        : handle     : hanle of a buffer
 Output       :
 Return Value : void Pointer
                success : Not NULL
                failure : NULL
*****************************************************************************/
extern void *vb_blk_handle_to_kern(k_vb_blk_handle handle);

/*****************************************************************************
 Prototype    : vb_inquire_user_cnt
 Description  : Inquire how many users are using the buffer
 Input        : handle     : hanle of a buffer
 Output       :
 Return Value : k_s32
******************************************************************************/
extern k_s32 vb_inquire_user_cnt(k_vb_blk_handle handle);

#ifdef __cplusplus
}
#endif

#endif
