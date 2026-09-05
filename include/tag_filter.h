#ifndef _TAG_FILTER_H
#define _TAG_FILTER_H

#include "settings.h"

bool tagFilterValidContentId(const char *content_id);
bool tagFilterBlocksRuid(const char *ruid);
bool tagFilterBlocksUid(uint64_t uid);

#endif
