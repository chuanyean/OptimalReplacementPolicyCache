/*
 * Copyright (c) 2009 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Lisa Hsu
 */

/**
 * @file
 * Declaration of an associative set
 */

#ifndef __CACHESET_HH__
#define __CACHESET_HH__

#include <cassert>

#include "mem/cache/blk.hh" // base class

/**
 * An associative set of cache blocks.
 */
class CacheSet
{
  public:
    /** The total associativity of this set. */
    int assoc;
    /** The SC associativity of this set */
    int sc_assoc;
    /** The MC associativity of this set */
    int mc_assoc;

    int leastImmBlk_SCptr;

    /** Block indexes array to maintain the least recently used blocks **/
    int *LRU_Order;

    /** Cache blocks in this set, maintained in LRU order 0 = MRU. */
    CacheBlk **blks;

    /** Array of SC_flag for each block in a set */
    int *SC_flag;

    /** Array of SC_ptr for each block in a set */
    int *SC_ptr;

    /** NVC -> Next Value Counter */
    int *nvc;

    /** Count Matrix (CM) */
    //int **count_mat;
    int *count_mat;

    /** SC queue to track SC blocks in FIFO order.
     * Head = oldest block, i.e. newer blocks always
     * added to tail.
     * Contains integer SC_ptr vals
     */
    int *SC_queue;

    /**
     * Find a block matching the tag in this set.
     * @param asid The address space ID.
     * @param tag The Tag to find.
     * @return Pointer to the block if found.
     */
    CacheBlk* findBlk(Addr tag) const;

    /**
     * Move the given block to the head of the list.
     * @param blk The block to move.
     * Only used in LRU, not in OPT
     */
    void moveToHead(CacheBlk *blk);

    /**
     * Move the given block to the tail of the list.
     * @param blk The block to move
     * Only used in LRU, not in OPT
     */
    void moveToTail(CacheBlk *blk);

    /** Update LRU_Order array **/
    void moveBlkToTail(int blkIndex);
    void moveBlkToHead(int blkIndex);

	/**
	 * Move the given SC block to the tail of the list.
	 * @param blk The block to move
	 */
	void moveSCToTail();

	/**
	 * Move the given SC block to the tail of the list.
	 * @param blk The block to move
	 */
	void moveSCToHead();

	/*
	 * Uses the global var 'leastImmblkSC_ptr' to determine
	 * the SC # (SC ptr) to be replaced according to FIFO.
	 */
	int getSCFIFOHead();

	/**
     * Uses the Count Matrix to find the least imminent block
     * which is the largest count value for a particular
     * SC block.
     * Returns index to the least imminent block.
     */
    int findLeastImminentBlock ();


};

#endif
