#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tag_filter.h"
#include "handler.h"
#include "handler_cloud.h"
#include "toniebox_state.h"
#include "mutex_manager.h"

uint64_t read_big_endian64(const uint8_t *buf);
uint64_t read_little_endian64(const uint8_t *buf);
tonie_info_t *getTonieInfoForRequest(HttpConnection *, const char_t *, int, const char_t *, client_ctx_t *, bool_t, char *, bool_t *, error_t *);
void process_freshness_check(client_ctx_t *, TonieFreshnessCheckRequest *, TonieFreshnessCheckResponse *, TonieFreshnessCheckRequest *, size_t *);

int main(void)
{
    char temp[] = "/tmp/teddycloud-tag-filter-XXXXXX";
    char *base = mkdtemp(temp);
    assert(base != NULL);
    char config[512];
    snprintf(config, sizeof(config), "%s/config", base);
    assert(mkdir(config, 0700) == 0);
    mutex_manager_init();
    assert(settings_init(base, base) == NO_ERROR);
    settings_t *settings = get_settings();

    // Regression: a low word with bit 31 set must not overwrite the high word.
    const uint8_t big[] = {0x92, 0x34, 0x56, 0x78, 0xe0, 0x04, 0x03, 0x00};
    const uint8_t little[] = {0x00, 0x03, 0x04, 0xe0, 0x78, 0x56, 0x34, 0x92};
    assert(read_big_endian64(big) == UINT64_C(0xe004030092345678));
    assert(read_little_endian64(little) == UINT64_C(0xe004030092345678));
    const uint8_t edges[][8] = {
        {0x7f, 0xff, 0xff, 0xff, 0xe0, 0x04, 0x03, 0x00},
        {0x80, 0x00, 0x00, 0x00, 0xe0, 0x04, 0x03, 0x00},
        {0xff, 0xff, 0xff, 0xff, 0xe0, 0x04, 0x03, 0x00}
    };
    const uint64_t expected[] = {UINT64_C(0xe00403007fffffff), UINT64_C(0xe004030080000000), UINT64_C(0xe0040300ffffffff)};
    for (size_t i = 0; i < 3; i++) assert(read_big_endian64(edges[i]) == expected[i]);

    assert(tagFilterBlocksRuid("78563412FFFFFFFF"));
    assert(tagFilterBlocksRuid("78563412ffffffff"));
    assert(tagFilterBlocksRuid("FFFFFFFFFFFFFFFF"));
    assert(!tagFilterBlocksRuid("FFFFFFFF000304E0"));
    assert(!tagFilterBlocksRuid("78563492000304E0"));
    assert(!tagFilterBlocksRuid("FFFFFFFF"));
    assert(!tagFilterBlocksRuid("garbage!FFFFFFFF"));
    assert(!tagFilterBlocksRuid(NULL));
    assert(tagFilterBlocksUid(UINT64_C(0xffffffff12345678)));
    assert(!tagFilterBlocksUid(UINT64_C(0xe0040300ffffffff)));
    assert(!tagFilterBlocksUid(read_big_endian64(big)));
    puts("PASS: UID conversion, signed-word edges, rUID filename versus folder");

    assert(!settings_set_string("core.tag_filter.content_id", "FFFFFF"));
    assert(!settings_set_string("core.tag_filter.content_id", "GGGGGGGG"));
    assert(!settings_set_string("core.tag_filter.content_id", "FFFFFFFF0"));
    assert(!settings_set_string("core.tag_filter.content_id", "FFFFFFFF\n"));
    assert(!settings_set_string("core.tag_filter.content_id", ""));
    assert(strcmp(settings->core.tag_filter_content_id, "FFFFFFFF") == 0);
    assert(!settings_set_bool_id("core.tag_filter.enabled", false, 1));
    assert(!settings_set_string_id("core.tag_filter.content_id", "00000000", 1));
    assert(settings_set_string("core.tag_filter.content_id", "89abCDef"));
    assert(tagFilterBlocksRuid("1234567889ABCDEF"));
    assert(!tagFilterBlocksRuid("12345678FFFFFFFF"));
    assert(settings_save() == NO_ERROR);
    assert(settings_set_string("core.tag_filter.content_id", "FFFFFFFF"));
    assert(settings_load() == NO_ERROR);
    assert(tagFilterBlocksRuid("1234567889abcdef"));
    assert(settings_set_bool("core.tag_filter.enabled", false));
    assert(!tagFilterBlocksRuid("1234567889abcdef"));
    assert(settings_set_string("core.tag_filter.content_id", "FFFFFFFF"));
    assert(settings_set_bool("core.tag_filter.enabled", true));
    puts("PASS: validation, save/reload, enable/disable, global-only settings");

    settings_t overlay = *settings;
    overlay.core.tag_filter_enabled = false;
    toniebox_state_t state = {0};
    client_ctx_t ctx = {.settings = &overlay, .settingsNoOverlay = &overlay, .state = &state};
    HttpConnection *connection = calloc(1, sizeof(HttpConnection));
    assert(connection != NULL);
    // These call the real handlers. With cloud enabled and no socket they must
    // return before any network access, tag creation or automatic assignment.
    overlay.cloud.enabled = true;
    assert(settings_set_string("internal.assign_unknown", "/pending-assignment.taf"));
    assert(handleCloudClaim(connection, "/v1/claim/78563412FFFFFFFF", "", &ctx) == ERROR_NOT_FOUND);
    assert(handleCloudContentV1(connection, "/v1/content/78563412FFFFFFFF", "", &ctx) == ERROR_NOT_FOUND);
    connection->request.auth.found = true;
    connection->request.auth.mode = HTTP_AUTH_MODE_DIGEST;
    assert(handleCloudContentV2(connection, "/v2/content/78563412FFFFFFFF", "", &ctx) == ERROR_NOT_FOUND);
    assert(handleCloudContentMetaV3(connection, "/v3/content-meta/78563412FFFFFFFF", "", &ctx) == ERROR_NOT_FOUND);
    char ruid[] = "78563412FFFFFFFF";
    setLastRuid(ruid, &overlay);
    tbs_tag_placed(&ctx, UINT64_C(0xffffffff12345678), true);
    tbs_tag_removed(&ctx, UINT64_C(0xffffffff12345678), true);
    assert(state.tag.uid == 0);
    assert(strcmp(settings_get_string("internal.assign_unknown"), "/pending-assignment.taf") == 0);
    char *content = NULL;
    getContentPathFromCharRUID(ruid, &content, &overlay);
    char json[1024];
    snprintf(json, sizeof(json), "%s.json", content);
    assert(access(json, F_OK) == -1);
    free(content);
    free(connection);
    puts("PASS: claim/content v1/v2/v3, last tag and RTNL state have no blocked-tag side effects");

    TonieFCInfo blocked = TONIE_FCINFO__INIT;
    blocked.uid = UINT64_C(0xffffffff12345678);
    TonieFCInfo *items[] = {&blocked};
    TonieFreshnessCheckRequest req = TONIE_FRESHNESS_CHECK_REQUEST__INIT;
    req.n_tonie_infos = 1;
    req.tonie_infos = items;
    TonieFreshnessCheckRequest cloud = TONIE_FRESHNESS_CHECK_REQUEST__INIT;
    TonieFreshnessCheckResponse response = TONIE_FRESHNESS_CHECK_RESPONSE__INIT;
    uint64_t cached[] = {UINT64_C(0xffffffff87654321)};
    assert(settings_set_u64_array("internal.freshnessCache", cached, 1));
    overlay.internal.overlayNumber = 0;
    size_t cache_len;
    process_freshness_check(&ctx, &req, &response, &cloud, &cache_len);
    assert(cloud.n_tonie_infos == 0);
    assert(response.n_tonie_marked == 0);
    free(cloud.tonie_infos);
    free(response.tonie_marked);
    puts("PASS: blocked freshness requests and pre-existing cache entries are excluded");

    settings_deinit();
    mutex_manager_deinit();
    printf("All tag-filter tests passed. Temporary fixtures: %s\n", base);
    return 0;
}
