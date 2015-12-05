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


#include "mem/cache/tags/cacheset.hh"
using namespace std;

CacheBlk*
CacheSet::findBlk(Addr tag) const
{
    for (int i = 0; i < assoc; ++i) {
        if (blks[i]->tag == tag && blks[i]->isValid()) {
            return blks[i];
        }
    }
    return 0;
}

void
CacheSet::moveToHead(CacheBlk *blk)
{
    // nothing to do if blk is already head
    if (blks[0] == blk)
        return;

    // write 'next' block into blks[i], moving up from MRU toward LRU
    // until we overwrite the block we moved to head.

    // start by setting up to write 'blk' into blks[0]
    int i = 0;
    CacheBlk *next = blk;

    do {
        assert(i < assoc);
        // swap blks[i] and next
        CacheBlk *tmp = blks[i];
        blks[i] = next;
        next = tmp;
        ++i;
    } while (next != blk);
}

void
CacheSet::moveToTail(CacheBlk *blk)
{
    // nothing to do if blk is already tail
    if (blks[assoc-1] == blk)
        return;

    // write 'next' block into blks[i], moving from LRU to MRU
    // until we overwrite the block we moved to tail.

    // start by setting up to write 'blk' into tail
    int i = assoc - 1;
    CacheBlk *next = blk;

    do {
        assert(i >= 0);
        // swap blks[i] and next
        CacheBlk *tmp = blks[i];
        blks[i] = next;
        next = tmp;
        --i;
    } while (next != blk);
}

int
CacheSet::findLeastImminentBlock ()
{
	/*
	 * Search in a CM column to get the highest count value for current head SC#,
	 * since that is the SC block that will be moved to the least imminent
	 * block in MC.
	 *
	 * Iterate over all the rows for this SC
	 */
	int scptr = leastImmBlk_SCptr;
	int maxCount = 0, maxPos = 0;

	//cout << "Printing count matrix for current SC\n";
	for (int i=0; i<assoc; i++){
		//cout << "block" << i << ":" << count_mat[i + scptr*assoc] << ", ";
		if (maxCount < count_mat[i + scptr*assoc]){

			maxCount = count_mat[i + scptr*assoc];
			maxPos = i;
		}
	}
	//cout << "\nFound least imminent block at blk: " << maxPos << ", with count of " << maxCount << "\n";
	return maxPos;
}


int
CacheSet::getSCFIFOHead ()
{
	int scBlkIndex = 0;

	for (int i=0; i<assoc; i++) {
		if (leastImmBlk_SCptr == SC_ptr[i]){
			scBlkIndex = i;

			//cout << "MZ - SC fifo head to be replaced: blk[" << scBlkIndex << "]\n";
			return scBlkIndex;
		}
	}
	fatal ("MZ - getSCFIFOHead - error, no block found.");
	return scBlkIndex;

}


void
CacheSet::moveSCToTail(int blkSCPtr)
{
	leastImmBlk_SCptr = (leastImmBlk_SCptr + 1) % sc_assoc;
	//cout << "After moveToTail, leastImmBlk_SCptr: " << leastImmBlk_SCptr << "\n";
	return;
}

void
CacheSet::moveSCToHead()
{
	leastImmBlk_SCptr = (leastImmBlk_SCptr - 1) < 0 ? (sc_assoc - 1) : (leastImmBlk_SCptr - 1);
	//cout << "After moveToHead, leastImmBlk_SCptr: " << leastImmBlk_SCptr << "\n";
	return;
}
