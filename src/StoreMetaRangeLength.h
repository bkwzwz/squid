/*
 * Copyright (C) 1996-2018 The Squid Software Foundation and contributors
 *
 * Squid software is distributed under GPLv2+ license and includes
 * contributions from numerous individuals and organizations.
 * Please see the COPYING and CONTRIBUTORS files for details.
 */

#ifndef SQUID_STOREMETARANGELENGTH_H
#define SQUID_STOREMETARANGELENGTH_H

#include "MemPool.h"
#include "StoreMeta.h"

class StoreMetaRangeLength : public StoreMeta
{

public:
MEMPROXY_CLASS(StoreMetaRangeLength);

    char getType() const {return STORE_META_RANGE_LENGTH;}
};

MEMPROXY_CLASS_INLINE(StoreMetaRangeLength);

#endif /* SQUID_STOREMETARANGELENGTH_H */
