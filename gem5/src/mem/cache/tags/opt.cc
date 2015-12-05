/*
 * opt.cc
 *
 *  Created on: Nov 24, 2015
 *      Author: mzaim
 */

/*
 * Emulating an OPT cache tag store
 * For use with an L2 Shepherd Cache only
 */

#include <string>
#include "base/intmath.hh"
#include "debug/CacheRepl.hh"
#include "mem/cache/tags/cacheset.hh"
#include "mem/cache/tags/opt.hh"
#include "mem/cache/base.hh"
#include "sim/core.hh"

using namespace std;

// create and initialize an OPT-emulated cache structure
OPT::OPT(unsigned _numSetsTotal, unsigned _blkSize,
		unsigned _assoc, unsigned _hit_latency, unsigned _numSetsSC)
    : numSetsTotal(_numSetsTotal), blkSize(_blkSize),
      assoc(_assoc), hitLatency(_hit_latency), numSetsSC(_numSetsSC)
{
    // Check parameters
    if (blkSize < 4 || !isPowerOf2(blkSize)) {
        fatal("Block size must be at least 4 and a power of 2");
    }
    if (numSetsTotal <= 0 || !isPowerOf2(numSetsTotal)) {
        fatal("# of sets must be non-zero and a power of 2");
    }
    if (assoc <= 0) {
        fatal("associativity must be greater than zero");
    }
    if (hitLatency <= 0) {
        fatal("access latency must be greater than zero");
    }

    numSetsMC = assoc - numSetsSC;
    blkMask = blkSize - 1;
    setShift = floorLog2(blkSize);
    setMask = numSetsTotal - 1;
    tagShift = setShift + floorLog2(numSetsTotal);
    warmedUp = false;
    /** @todo Make warmup percentage a parameter. */
    warmupBound = numSetsTotal * assoc;

    sets = new CacheSet[numSetsTotal];
    blks = new BlkType[numSetsTotal * assoc];

    //-- Defining array members for sets(cachesets)
    unsigned ii,jj = 0;

    // allocate data storage in one big chunk
    numBlocks = numSetsTotal * assoc;
    dataBlks = new uint8_t[numBlocks * blkSize];


    unsigned blkIndex = 0;       // index into blks array
    for (unsigned i = 0; i < numSetsTotal; ++i) {

    	sets[i].SC_flag = new int[assoc];
		sets[i].SC_ptr = new int[assoc];
		sets[i].nvc = new int[numSetsSC];
		sets[i].leastImmBlk_SCptr = 0;

		//allocate Count matrix in one big chunk
		sets[i].count_mat = new int [assoc * numSetsSC];

		//initialize SC queue to 0,1,2,3....
		sets[i].SC_queue = new int[numSetsSC];
    	for (jj=0; jj<numSetsSC; jj++){
    		sets[i].SC_queue[jj] = jj;
    	}

        sets[i].assoc = assoc;

        //-- Setup of SC params --//
        sets[i].sc_assoc = numSetsSC;
        sets[i].mc_assoc = assoc - numSetsSC;

        //-- Initializing the SC and MC blocks
        int sc_assoc_seq = 0;
        for (ii = 0 ; ii<assoc ; ii++){
        	//-- Setting the first number of blocks to SC --//
        	if (ii < numSetsSC) {
        			sets[i].SC_flag[ii] = 1;
        			sets[i].SC_ptr[ii] = sc_assoc_seq++;
        			sets[i].nvc[ii] = 0;

        	//-- Setting the remainder to MC --//
        	} else {
        			sets[i].SC_flag[ii] = 0;
        		    sets[i].SC_ptr[ii] = -1;
        	}

            //-- Initializing Count Matrix --//
        	for (jj=0; jj<numSetsSC; jj++){
        		sets[i].count_mat[ii + jj*assoc] = -2;
        	}
        }

        // Initialize ptr to blks array for each set
        sets[i].blks = new BlkType*[assoc];

        // Iterate over all blocks in a set, and
        // link in the data blocks,
        // init to invalid state,
        // and init tags,sets, etc.
        for (unsigned j = 0; j < assoc; ++j) {
            // locate next cache block
            BlkType *blk = &blks[blkIndex];
            blk->data = &dataBlks[blkSize*blkIndex];
            ++blkIndex;

            // invalidate new cache block
            blk->invalidate();

            //EGH Fix Me : do we need to initialize blk?

            // Setting the tag to j is just to prevent long chains in the hash
            // table; won't matter because the block is invalid
            blk->tag = j;
            blk->whenReady = 0;
            blk->isTouched = false;
            blk->size = blkSize;
            sets[i].blks[j]=blk;
            blk->set = i;
        }
    }

}

// Destructor
OPT::~OPT()
{
    delete [] dataBlks;
    delete [] blks;
    for (int i=0; i<numSetsTotal; i++){
    	delete [] sets[i].SC_flag;
    	delete [] sets[i].SC_ptr;
    	delete [] sets[i].nvc;
    	delete [] sets[i].count_mat;
    	delete [] sets[i].SC_queue;
    }
    delete [] sets;
}

OPT::BlkType*
OPT::accessBlock(Addr addr, int &lat, int master_id)
{
    Addr tag = extractTag(addr);
    unsigned set = extractSet(addr);
    BlkType *blk = sets[set].findBlk(tag);
    lat = hitLatency;

    // if it's a miss, exit
    if (blk == NULL) { return blk; }

    //-- Get the Hit Block Index --//
    int hitBlockIndex = sets[set].getSCFIFOHead();

	//-- Determine if count matrix is defined --//
	//-- If undefined -> Assign CM to NVC, Update NVC
	//-- Else -> Don't Touch
    unsigned ii = 0;
    for(ii=0; ii<numSetsSC; ii++){
		if (sets[set].count_mat[hitBlockIndex + ii*assoc] == -1){
			DPRINTF(Cache, "-MZ- updating count_matx[%d][%d] to %d for hits", hitBlockIndex, ii, sets[set].nvc[ii]);
			sets[set].count_mat[hitBlockIndex + ii*assoc] = sets[set].nvc[ii];
			sets[set].nvc[ii]++;
		}
    }

    if (blk != NULL) {
        DPRINTF(CacheRepl, "set %x: moving blk %x to MRU\n",
                set, regenerateBlkAddr(tag, set));
        if (blk->whenReady > curTick()
            && blk->whenReady - curTick() > hitLatency) {
            lat = blk->whenReady - curTick();
        }
        blk->refCount += 1;
    }

    return blk;
}

// Find a block with given addr in SC & MC.
OPT::BlkType*
OPT::findBlock(Addr addr) const
{
    Addr tag = extractTag(addr);
    unsigned set = extractSet(addr);
    BlkType *blk = sets[set].findBlk(tag);   // blk == 0 if no matching block found

    // Print contents of cache for debugging help
    DPRINTF (Cache, "MZ: In findBlock. Searching for addr %x in cache.\n", addr);
    for (int i=0; i<numSetsTotal; i++){
    	for (int j=0; j<assoc; j++) {
    		DPRINTF (Cache, "MZ: sets[%d] blks[%d] (SCF:%d | SCPtr:%2d | V:%d | T:%5x | D:%x) - CM(\n",
    					i, j, sets[i].SC_flag[j], sets[i].SC_ptr[j], sets[i].blks[j]->isValid(),
    					sets[i].blks[j]->tag, unsigned(*(sets[i].blks[j]->data)));

    		//Print CM
    		/* remove \n from above when uncommenting this..
    		for (int k=0; k<numSetsSC; k++){
    			cout << "blk[" << k << "]:" << sets[i].count_mat[j + k*assoc] << ",";
    		}cout << ")\n";*/
    	}
   }
    return blk;
}

// Find block to replace - could be a valid or an invalid block.
OPT::BlkType*
OPT::findVictim(Addr addr, PacketList &writebacks)
{
	BlkType *SCBlk = findVictimInSC (addr, writebacks);
	return SCBlk;
}

// Find victim to replace in SC. Could be a valid or invalid block.
OPT::BlkType*
OPT::findVictimInSC(Addr addr, PacketList &writebacks)
{
	unsigned set = extractSet(addr);
	int i=0;

	// iterate over the cacheset queue to find an empty SC block
	for (i = 0; i < assoc; i++){
		BlkType *SCblk = sets[set].blks[i];
		if ( sets[set].SC_flag[i] && SCblk->isValid() == 0 ) {
			return SCblk;
		}
	}

	// If no empty block found, grab a victim in SC to move to MC.
	// Since cacheset queue is ordered by FIFO, the head of the
	// queue points to the block that was inserted first.
	// Hence, loop over queue from head to tail, and find the
	// first SC block to evict.
	for (i = 0; i < assoc; i++){
		BlkType *SCblk = sets[set].blks[i];
		if ( sets[set].SC_flag[i] == 1 ) {

			DPRINTF(CacheRepl, "set %x: selecting blk addr %x for replacement in SC\n",
	                set, regenerateBlkAddr(SCblk->tag, set));

			// Move this block from SC to MC
			SCblk = findVictimInMC(SCblk, i, regenerateBlkAddr(SCblk->tag, set), writebacks);
			return SCblk;
		}
	}
	fatal("Should not reach here @ findVictimInSC");
	return 0;
}

// Find victim to replace in MC. Return value is the victim in
// SC to be replaced (and written back if dirty)
OPT::BlkType*
OPT::findVictimInMC(BlkType *SCblk, int SCblkIndex, Addr addr, PacketList &writebacks)
{
	unsigned set = extractSet(addr);
	int i=0;

	// iterate over the cacheset queue to find an empty MC block
	for (i = 0; i < assoc; i++){
		BlkType *MCblk = sets[set].blks[i];
		if ( sets[set].SC_flag[i] == 0 && MCblk->isValid() == 0 ) {
			//Put the empty MC block in SC (to maintain total block counts for SC & MC)
			sets[set].SC_flag[i] = 1;
			sets[set].SC_ptr[i] = sets[set].SC_ptr[SCblkIndex];

			// And Move block from SC to MC, i.e. change flag & reset ptr
			sets[set].SC_flag[SCblkIndex] = 0;
			sets[set].SC_ptr[SCblkIndex] = -1;

			DPRINTF (Cache, "MZ - Found victim in empty MC block  (set[%d], blk[%d]) to be replaced.\n"
					"After move, blk[%d] = SCF:%d | SCptr:%d | T:%d | D:%x\n"
					"            blk[%d] = SCF:%d | SCptr:%d | T:%d | D:%x\n", set, i, i, sets[set].SC_flag[i],
					sets[set].SC_ptr[i], MCblk->tag, unsigned(*(MCblk->data)), SCblkIndex, sets[set].SC_flag[SCblkIndex],
					sets[set].SC_ptr[SCblkIndex], SCblk->tag, unsigned(*(SCblk->data)));

			return MCblk; //return original MC block (now an SC block)
		}
	}

	/*
	 * If no empty block found in MC, need to evict a block from the cache.
	 * Use imminence information gathered by CounterMatrix to decide
	 * which block to evict. It could be either a block from SC, or from MC.
	 */
	int blkToEvict_Index = sets[set].findLeastImminentBlock ();

	/* Now, perform the evictions.
	 * If the block to evict is an SC block, simply return it.
	 * Caller is responsible for invalidating it.
	 */
	if (sets[set].SC_flag[blkToEvict_Index] == 1){
		return sets[set].blks[blkToEvict_Index];
	}

	/* Else, if the block to evict is one from MC, perform the eviction
	 * by swapping MC and SC blocks, so that:
	 *
	 * Block to move to MC --> will be moved from SC to MC
	 *
	 * Block to evict from MC --> will be in SC, from where it will
	 * be replaced by incoming cache miss and written back to
	 * lower-level mem (replacement & WB handled in cache_impl.hh)
	 */

	//Move MC block to SC
	sets[set].SC_flag[blkToEvict_Index] = 1;
	sets[set].SC_ptr[blkToEvict_Index] = sets[set].SC_ptr[SCblkIndex];

	// Move SC block to MC
	sets[set].SC_flag[SCblkIndex] = 0;
	sets[set].SC_ptr[SCblkIndex] = -1;

	return sets[set].blks[blkToEvict_Index]; //return MC Block
}


void
OPT::insertBlock(Addr addr, BlkType *blk, int master_id)
{
	unsigned set = extractSet(addr);
	int blkIndex = sets[set].getSCFIFOHead ();

	//New blocks should always be inserted in SC, and not MC.
	assert (sets[set].SC_flag[blkIndex]);

    if (!blk->isTouched) {
        tagsInUse++;
        blk->isTouched = true;
        if (!warmedUp && tagsInUse.value() >= warmupBound) {
            warmedUp = true;
            warmupCycle = curTick();
        }
    }

    // If we're replacing a block that was previously valid update
    // stats for it. This can't be done in findBlock() because a
    // found block might not actually be replaced there if the
    // coherence protocol says it can't be.
    if (blk->isValid()) {
        replacements[0]++;
        totalRefs += blk->refCount;
        ++sampledRefs;
        blk->refCount = 0;

        // deal with evicted block
        assert(blk->srcMasterId < cache->system->maxMasters());
        occupancies[blk->srcMasterId]--;

        blk->invalidate();
    }

    blk->isTouched = true;
    // Set tag for new block.  Caller is responsible for setting status.
    blk->tag = extractTag(addr);

    // Initialize SC ptr, NVC and CM Matrix for new block.
    // prev sc_ptr val has not been overwritten in findVictim
    int prevSC_ptr = sets[set].SC_ptr[blkIndex];
    sets[set].nvc[prevSC_ptr] = 0;

    int m = 0;

    // Update CM matrix for all SC blocks in current set
    // Initialize this SC's column to -1 (empty)
    for (m=0; m<assoc; m++){
        sets[set].count_mat[m + prevSC_ptr*assoc] = -1;
    }

    // Update CM matrix for current newly inserted block
    // Initialize the row of this block to dummy value 0 for all valid SC blocks.
    for (m=0; m<numSetsSC; m++){
    	if (m != prevSC_ptr && sets[set].count_mat[prevSC_ptr + m*assoc] == -1) { //don't assign for self, valid blk == has a -1, instead of -2
    		sets[set].count_mat[prevSC_ptr + m*assoc] = 0;
    		//sets[set].nvc[prevSC_ptr]++; since dummy zero, no need to update nvc?
    	}
    }

    // Update imminence information for all other valid blocks in the set
    // that corresponds to current SC block's CM column.
    // This is similar to the update of CM that happens in accessBlock().
    for (m=0; m<assoc; m++){
    	if (sets[set].blks[m]->isValid()){ //if valid block
    		sets[set].count_mat[m + prevSC_ptr*assoc] = sets[set].nvc[prevSC_ptr];
    		sets[set].nvc[prevSC_ptr]++;
    	}
    }

    // no need to change SC_ptr ?

    // deal with what we are bringing in
    assert(master_id < cache->system->maxMasters());
    occupancies[master_id]++;
    blk->srcMasterId = master_id;

    sets[set].moveSCToTail(prevSC_ptr); // FIFO ordered SC. Newer blocks inserted at tail
}

void
OPT::invalidate(BlkType *blk)
{
    assert(blk);
    assert(blk->isValid());
    tagsInUse--;
    assert(blk->srcMasterId < cache->system->maxMasters());
    occupancies[blk->srcMasterId]--;
    blk->srcMasterId = Request::invldMasterId;

    // should be evicted before valid blocks
    Addr addr = regenerateBlkAddr (blk->tag, blk->set);
    unsigned set = extractSet(addr);
    sets[set].moveSCToHead();
}

void
OPT::clearLocks()
{
    for (int i = 0; i < numBlocks; i++){
        blks[i].clearLoadLocks();
    }
}

void
OPT::cleanupRefs()
{
    for (unsigned i = 0; i < numSetsTotal*assoc; ++i) {
        if (blks[i].isValid()) {
            totalRefs += blks[i].refCount;
            ++sampledRefs;
        }
    }
}

