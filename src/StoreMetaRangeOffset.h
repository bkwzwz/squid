/*
 * Copyright (C) 1996-2018 The Squid Software Foundation and contributors
 *
 * Squid software is distributed under GPLv2+ license and includes
 * contributions from numerous individuals and organizations.
 * Please see the COPYING and CONTRIBUTORS files for details.
 */

#ifndef SQUID_STOREMETARANGEOFFSET_H
#define SQUID_STOREMETARANGEOFFSET_H

#include "MemPool.h"
#include "StoreMeta.h"

class StoreMetaRangeOffset : public StoreMeta
{

public:
MEMPROXY_CLASS(StoreMetaRangeOffset);

    char getType() const {return STORE_META_RANGE_OFFSET;}
};

MEMPROXY_CLASS_INLINE(StoreMetaRangeOffset);

#endif /* SQUID_STOREMETARANGEOFFSET_H */

