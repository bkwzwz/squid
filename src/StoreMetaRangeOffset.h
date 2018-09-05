/*
 * Copyright (C) 1996-2018 The Squid Software Foundation and contributors
 *
 * Squid software is distributed under GPLv2+ license and includes
 * contributions from numerous individuals and organizations.
 * Please see the COPYING and CONTRIBUTORS files for details.
 */

#ifndef SQUID_STOREMETARANGEOFFSET_H
#define SQUID_STOREMETARANGEOFFSET_H

#include "StoreMeta.h"

class StoreMetaRangeOffset : public StoreMeta
{

MEMPROXY_CLASS(StoreMetaRangeOffset);

public:

    char getType() const {return STORE_META_RANGE_OFFSET;}
};

#endif /* SQUID_STOREMETARANGEOFFSET_H */

