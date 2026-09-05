#include <ctype.h>
#include <string.h>

#include "tag_filter.h"
#include "mutex_manager.h"

bool tagFilterValidContentId(const char *content_id)
{
    if (content_id == NULL || strlen(content_id) != 8)
    {
        return false;
    }
    for (size_t i = 0; i < 8; i++)
    {
        if (!isxdigit((unsigned char)content_id[i]))
        {
            return false;
        }
    }
    return true;
}

bool tagFilterBlocksRuid(const char *ruid)
{
    if (ruid == NULL || strlen(ruid) != 16)
    {
        return false;
    }
    for (size_t i = 0; i < 16; i++)
    {
        if (!isxdigit((unsigned char)ruid[i]))
        {
            return false;
        }
    }

    // rUID is stored as <first 8 digits>/<last 8 digits>.json.
    // Settings may be edited through the web UI while box requests are active.
    mutex_lock(MUTEX_SETTINGS);
    const settings_t *settings = get_settings();
    bool blocked = settings->core.tag_filter_enabled &&
                   tagFilterValidContentId(settings->core.tag_filter_content_id) &&
                   osStrcasecmp(ruid + 8, settings->core.tag_filter_content_id) == 0;
    mutex_unlock(MUTEX_SETTINGS);
    return blocked;
}

bool tagFilterBlocksUid(uint64_t uid)
{
    char ruid[17];
    // Convert the numeric UID to the byte-reversed ID used in content paths.
    for (size_t i = 0; i < 8; i++)
    {
        osSnprintf(ruid + i * 2, 3, "%02X", (unsigned int)((uid >> (i * 8)) & 0xff));
    }
    return tagFilterBlocksRuid(ruid);
}
