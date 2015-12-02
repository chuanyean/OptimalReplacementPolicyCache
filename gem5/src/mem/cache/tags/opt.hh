/*
 * opt.hh
 *
 *  Created on: Nov 24, 2015
 *      Author: mzaim
 */

#ifndef __MEM_CACHE_TAGS_OPT_HH__
#define __MEM_CACHE_TAGS_OPT_HH__

#include "mem/cache/tags/base.hh"
#include "mem/cache/blk.hh"
#include "mem/packet.hh"

#include <cassert>
#include <cstring>
#include <list>


class BaseCache;
class CacheSet;

/*
 * Emulating an OPT cache tag store
 * For use with an L2 Shepherd Cache only
 */

class OPT : public BaseTags
{
  public:
    /** Typedef the block type used in this tag store. */
    typedef CacheBlk BlkType;
    /** Typedef for a list of pointers to the local block class. */
    typedef std::list<BlkType*> BlkList;

  protected:
    const unsigned numSetsTotal; // = SC + MC sets

    /** The number of bytes in a block. */
    const unsigned blkSize;
    /** The associativity of the overall cache. */
    const unsigned assoc;
    /** The hit latency. */
    const unsigned hitLatency;
    /** The number of sets in the Shepherd's cache and the Main Cache. */
    const unsigned numSetsSC;
    unsigned numSetsMC;

    /** The cache sets. */
    CacheSet *sets;

    /** The cache blocks. */
    BlkType *blks;
    /** The data blocks, 1 per cache block. */
    uint8_t *dataBlks;

    /** The amount to shift the address to get the set. */
    int setShift;
    /** The amount to shift the address to get the tag. */
    int tagShift;
    /** Mask out all bits that aren't part of the set index. */
    unsigned setMask;
    /** Mask out all bits that aren't part of the block offset. */
    unsigned blkMask;

public:
    /**
     * Construct and initialize this tag store.
     * @param _numSetsTotal The number of sets in the overall cache.
     * @param _blkSize The number of bytes in a block.
     * @param _assoc The associativity of the cache.
     * @param _hit_latency The latency in cycles for a hit.
     * @param _numSetsSC The number of sets in the Shepherd's Cache.
     */
    OPT(unsigned _numSetsTotal, unsigned _blkSize,
    		unsigned _assoc, unsigned _hit_latency, unsigned _numSetsSC);

    /**
     * Destructor
     */
    virtual ~OPT();

    /**
     * Return the block size.
     * @return the block size.
     */
    unsigned
    getBlockSize() const
    {
        return blkSize;
    }

    /**
     * Return the subblock size. In the case of OPT it is always the block
     * size.
     * @return The block size.
     */
    unsigned
    getSubBlockSize() const
    {
        return blkSize;
    }

    /**
     * Generate the tag from the given address.
     * @param addr The address to get the tag from.
     * @return The tag of the address.
     */
    Addr extractTag(Addr addr) const
    {
        return (addr >> tagShift);
    }

    /**
     * Calculate the set index from the address.
     * @param addr The address to get the set from.
     * @return The set index of the address.
     */
    int extractSet(Addr addr) const
    {
        return ((addr >> setShift) & setMask);
    }

    /**
     * Get the block offset from an address.
     * @param addr The address to get the offset of.
     * @return The block offset.
     */
    int extractBlkOffset(Addr addr) const
    {
        return (addr & blkMask);
    }

    /**
     * Align an address to the block size.
     * @param addr the address to align.
     * @return The block address.
     */
    Addr blkAlign(Addr addr) const
    {
        return (addr & ~(Addr)blkMask);
    }

    /**
     * Regenerate the block address from the tag.
     * @param tag The tag of the block.
     * @param set The set of the block.
     * @return The block address.
     */
    Addr regenerateBlkAddr(Addr tag, unsigned set) const
    {
        return ((tag << tagShift) | ((Addr)set << setShift));
    }

    /**
     * Return the hit latency.
     * @return the hit latency.
     */
    int getHitLatency() const
    {
        return hitLatency;
    }

    /**
     * Invalidate the given block.
     * @param blk The block to invalidate.
     */
    void invalidate(BlkType *blk);

    /**
     * Access block and update replacement data.  May not succeed, in which case
     * NULL pointer is returned.  This has all the implications of a cache
     * access and should only be used as such. Returns the access latency as a side effect.
     * @param addr The address to find.
     * @param lat The access latency.
     * @return Pointer to the cache block if found.
     */
    BlkType* accessBlock(Addr addr, int &lat, int context_src);

    /**
     * Finds the given address in the cache (check both Shepherd Cache ways
     * and Main Cache ways for the set), do not update replacement data.
     * Basically calls the findBlk() method in cacheset.cc.
     * i.e. This is a no-side-effect find of a block.
     * @param addr The address to find.
     * @param asid The address space ID.
     * @return Pointer to the cache block if found.
     */
    BlkType* findBlock(Addr addr) const;

    /**
     * Find a block to evict for the address provided.
     * This function will call findVictimInMC or findVictimInSC
     * as needed.
     * @param addr The addr to a find a replacement candidate for.
     * @param writebacks List for any writebacks to be performed.
     * @return The candidate block.
     */
    BlkType* findVictim(Addr addr, PacketList &writebacks);

    /**
     * Find a block to evict for the address provided.
     * Evict from SC using FIFO. This function might call
     * findVictimInMC if required.
     * @param addr The addr to a find a replacement candidate for.
     * @param writebacks List for any writebacks to be performed.
     * @return The candidate block.
     */
    BlkType* findVictimInSC(Addr addr, PacketList &writebacks);

    /**
     * Find a block to evict for the address provided.
     * Evict from MC using imminence data gathered in CounterMatrix
     * of cache set.
     * @param SCblk The block in SC that needs to be moved to MC.
     * @param addr The addr to a find a replacement candidate for.
     * @param writebacks List for any writebacks to be performed.
     * @return The candidate block.
     */
    BlkType* findVictimInMC(BlkType *SCblk, int SCblkIndex, Addr addr, PacketList &writebacks);

    /**
     * Insert the new block into the cache. New blocks are always
     * inserted into SC, so the input argument blk always points
     * to a block in SC.
     * @param addr The address to update.
     * @param blk The block to update/insert at.
     */
     void insertBlock(Addr addr, BlkType *blk, int context_src);

     /**
      *iterate through all blocks and clear all locks
      *Needed to clear all lock tracking at once
      */
     virtual void clearLocks();

     /**
      * Called at end of simulation to complete average block reference stats.
      */
     virtual void cleanupRefs();
};

#endif // __MEM_CACHE_TAGS_OPT_HH
