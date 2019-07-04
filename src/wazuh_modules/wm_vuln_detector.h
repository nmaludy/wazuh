/*
 * Wazuh Module to analyze vulnerabilities
 * Copyright (C) 2015-2019, Wazuh Inc.
 * January 4, 2018.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#ifndef CLIENT

#ifndef WM_VULNDETECTOR
#define WM_VULNDETECTOR

#include "external/sqlite/sqlite3.h"

#define WM_VULNDETECTOR_LOGTAG ARGV0 ":" VU_WM_NAME
#define WM_VULNDETECTOR_DEFAULT_INTERVAL 300 // 5 minutes
#define WM_VULNDETECTOR_DEFAULT_UPDATE_INTERVAL 3600 // 1 hour
#define WM_VULNDETECTOR_NVD_UPDATE_INTERVAL 86400 // 1 day
#define WM_VULNDETECTOR_DEFAULT_CPE_UPDATE_INTERVAL 86400 // 1 day
#define WM_VULNDETECTOR_RETRY_UPDATE  300 // 5 minutes
#define WM_VULNDETECTOR_ONLY_ONE_UPD UINT_MAX
#define WM_VULNDETECTOR_DOWN_ATTEMPTS  5
#define VU_DEF_IGNORE_TIME 21600 // 6 hours
#define VU_TEMP_FILE "tmp/vuln-temp"
#define VU_FIT_TEMP_FILE VU_TEMP_FILE "-fitted"
#define VU_TEMP_METADATA_FILE VU_TEMP_FILE "-metadata"
#define VU_DICTIONARIES DEFAULTDIR "/queue/vulnerabilities/dictionaries"
#define VU_CPE_HELPER_FILE VU_DICTIONARIES "/cpe_helper.json"
#define VU_MSU_FILE VU_DICTIONARIES "/msu.json"
#define VU_CPEHELPER_FORMAT_VERSION 1
#define CANONICAL_REPO "https://people.canonical.com/~ubuntu-security/oval/com.ubuntu.%s.cve.oval.xml"
#define DEBIAN_REPO "https://www.debian.org/security/oval/oval-definitions-%s.xml"
#define RED_HAT_REPO_DEFAULT_MIN_YEAR 2010
#define RED_HAT_REPO_MAX_REQ_ITS 100
#define RED_HAT_REPO_MAX_FAIL_ITS 5
#define NVD_REPO_DEFAULT_MIN_YEAR 2010
#define MULTI_URL_TAG "[-]"
#define NVD_IT_COMMIT 10000
#define RED_HAT_REPO_MIN_YEAR 1999
#define NVD_REPO_MIN_YEAR 2002
#define RED_HAT_REPO_MAX_ATTEMPTS 3
#define RED_HAT_REPO_REQ_SIZE 1000
#define RED_HAT_REPO "https://access.redhat.com/labs/securitydataapi/cve.json?after=%d-01-01&per_page=%d&page=%d"
#define NVD_CPE_REPO "https://nvd.nist.gov/feeds/xml/cpe/dictionary/official-cpe-dictionary_v2.3.xml.gz"
#define NVD_CVE_REPO_META "https://nvd.nist.gov/feeds/json/cve/1.0/nvdcve-1.0-%d.meta"
#define NVD_CVE_REPO "https://nvd.nist.gov/feeds/json/cve/1.0/nvdcve-1.0-%d.json.gz"
#define NVD_REPO_MAX_ATTEMPTS 3
#define DOWNLOAD_SLEEP_FACTOR 5
#define JSON_FILE_TEST "/tmp/package_test.json"
#define DEFAULT_OVAL_PORT 443
#define KEY_SIZE OS_SIZE_6144
#define VU_SSL_BUFFER OS_MAXSTR
#define VU_MAX_VERSION_ATTEMPS 15
#define VU_MAX_WAZUH_DB_ATTEMPS 5
#define VU_MAX_TIMESTAMP_ATTEMPS 4
#define VU_MAX_VER_COMP_IT 50
#define VU_TIMESTAMP_FAIL 0
#define VU_TIMESTAMP_UPDATED 1
#define VU_TIMESTAMP_OUTDATED 2
#define VU_AGENT_REQUEST_LIMIT   3
#define VU_ALERT_HEADER "[%03d] (%s) %s"
#define VU_ALERT_JSON "1:" VU_WM_NAME ":%s"

// CPE helper action flags
#define CPE_DIC_REP_VEN                                 1 << 0
#define CPE_DIC_REP_PROD                                1 << 1
#define CPE_DIC_REP_VEN_IF_MATCH                        1 << 2
#define CPE_DIC_SET_VER_IF_MATCH                        1 << 3
#define CPE_DIC_REP_PROD_IF_MATCH                       1 << 4
#define CPE_DIC_REP_SWED_IF_PROD_MATCHES                1 << 5
#define CPE_DIC_REP_MSUN_IF_VER_MATCHES                 1 << 6
#define CPE_DIC_IGNORE                                  1 << 7
#define CPE_DIC_CHECK_HOTFIX                            1 << 8
#define CPE_DIC_REP_MSUN                                1 << 9
#define CPE_DIC_SET_VER_IF_PROD_MATCHES                 1 << 10
#define CPE_DIC_REP_ARCH_IF_PRODUCT_MATCHES             1 << 11
#define CPE_DIC_SET_VER_ONLY_IF_PROD_MATCHES            1 << 12
#define CPE_DIC_REP_ARCH_ONLY_IF_PRODUCT_MATCHES        1 << 13

// Patterns for building references
#define VUL_BUILD_REF_MAX 100
#define VU_BUILD_REF_CVE_RH "https://access.redhat.com/security/cve/%s"
#define VU_BUILD_REF_BUGZ "https://bugzilla.redhat.com/show_bug.cgi?id=%s"
#define VU_BUILD_REF_RHSA "https://access.redhat.com/errata/%s"

extern const wm_context WM_VULNDETECTOR_CONTEXT;
extern const char *vu_feed_tag[];
extern const char *vu_feed_ext[];
extern const char *vu_package_comp[];
extern const char *vu_severities[];
extern const char *vu_cpe_tags[];
typedef struct cpe_list cpe_list;
typedef struct nvd_vulnerability nvd_vulnerability;
typedef struct cv_scoring_system cv_scoring_system;
typedef struct vu_cpe_dic vu_cpe_dic;
typedef struct cpe cpe;
typedef struct vu_msu_entry vu_msu_entry;
typedef struct nvd_metadata nvd_metadata;
typedef struct vu_report vu_report;

typedef enum vu_severity {
    VU_LOW,
    VU_MEDIUM,
    VU_MODERATE,
    VU_UNKNOWN,
    VU_HIGH,
    VU_IMPORTANT,
    VU_CRITICAL,
    VU_NONE,
    VU_NEGL,
    VU_UNTR,
    VU_UNDEFINED_SEV
} vu_severity;

typedef enum vu_logic {
    VU_TRUE,
    VU_FALSE,
    VU_OR,
    VU_AND,
    VU_PACKG,
    VU_OBJ,
    VU_FILE_TEST,
    VU_VULNERABLE,
    VU_NOT_VULNERABLE,
    VU_UNDEFINED,
    VU_LESS,
    VU_HIGHER,
    VU_EQUAL,
    VU_ERROR_CMP,
    VU_NOT_FIXED,
    VU_NEED_UPDATE,
    VU_NOT_NEED_UPDATE,
    VU_INV_FEED,
    VU_TRY_NEXT_PAGE,
    VU_SUCC_DOWN_PAGE,
    VU_FINISH_FETCH
} vu_logic;

typedef enum vu_package_dist_id {
    VU_RH_EXT_BASE,
    VU_RH_EXT_7,
    VU_RH_EXT_6,
    VU_RH_EXT_5,
    VU_UB_EXT,
    VU_AMAZ_EXT
} vu_package_dist_id;

typedef enum vu_cpe_dic_index {
    VU_REP_VEN,
    VU_REP_PROD,
    VU_REP_VEN_IF_MATCH,
    VU_REP_PROD_IF_MATCH,
    VU_SET_VER_IF_MATCHES,
    VU_REP_SWED_VER_IF_PR_MATCHES,
    VU_REP_MSUN_IF_VER_MATCHES,
    VU_IGNORE,
    VU_CHECK_HOTFIX,
    VU_REP_MSUN,
    VU_SET_VERSION_IF_PROD_MATCHES,
    VU_REP_ARCH_IF_PROD_MATCHES,
    VU_SET_VER_ONLY_IF_PROD_MATCHES,
    VU_REP_ARCH_ONLY_IF_PROD_MATCHES
} vu_cpe_dic_index;

typedef enum vu_feed {
    FEED_UBUNTU,
    FEED_CANONICAL,
    FEED_DEBIAN,
    FEED_REDHAT,
    FEED_CENTOS,
    FEED_AMAZL,
    FEED_WIN,
    // Ubuntu versions
    FEED_PRECISE,
    FEED_TRUSTY,
    FEED_XENIAL,
    FEED_BIONIC,
    // Debian versions
    FEED_JESSIE,
    FEED_STRETCH,
    FEED_WHEEZY,
    // RedHat versions
    FEED_RHEL5,
    FEED_RHEL6,
    FEED_RHEL7,
    // NVD
    FEED_NVD,
    FEED_CPED,
    // WINDOWS
    FEED_WS2003,
    FEED_WS2003R2,
    FEED_WXP,
    FEED_WVISTA,
    FEED_W7,
    FEED_W8,
    FEED_W81,
    FEED_W10,
    FEED_WS2008,
    FEED_WS2008R2,
    FEED_WS2012,
    FEED_WS2012R2,
    FEED_WS2016,
    FEED_WS2019,
    // OTHER
    FEED_CPEW,
    FEED_MSU,
    FEED_UNKNOW
} vu_feed;

typedef enum vu_ver_comp {
    VU_COMP_L,
    VU_COMP_LE,
    VU_COMP_G,
    VU_COMP_GE,
    VU_COMP_EQ,
    VU_COMP_NEQ,
    VU_COMP_EX
} vu_ver_comp;

typedef enum cpe_tags {
    CPE_VENDOR,
    CPE_PRODUCT,
    CPE_VERSION,
    CPE_SW_EDITION,
    CPE_UPDATE,
    CPE_MSU_NAME
} cpe_tags;


typedef struct wm_vuldet_flags {
    unsigned int enabled:1;
    unsigned int run_on_start:1;
    unsigned int update:1;
    unsigned int patch_scan:1;
    unsigned int permissive_patch_scan:1;
} wm_vuldet_flags;

typedef struct wm_vuldet_state {
    time_t next_time;
} wm_vuldet_state;

typedef struct agent_software {
    char *agent_id;
    char *agent_name;
    char *agent_ip;
    cpe *agent_os_cpe;
    char *arch;
    int build;
    vu_feed dist;
    vu_feed dist_ver;
    char info;
    char *os_release;
    struct agent_software *next;
    struct agent_software *prev;
} agent_software;

typedef enum cve_db{
    CVE_PRECISE,
    CVE_TRUSTY,
    CVE_XENIAL,
    CVE_BIONIC,
    CVE_JESSIE,
    CVE_STRETCH,
    CVE_WHEEZY,
    CVE_REDHAT,
    CVE_NVD,
    CPE_NDIC,
    CPE_WDIC,
    CVE_MSU,
    OS_SUPP_SIZE
} cve_db;

typedef struct update_node {
    char *dist;
    char *version;
    vu_feed dist_ref;
    vu_feed dist_tag_ref;
    const char *dist_ext;
    time_t last_update;
    unsigned long interval;
    int update_from_year; // only for Red Hat and NVD feeds
    vu_logic update_state; // only for Red Hat feed
    int update_it; // only NVD feed
    char *url;
    char *multi_url;
    int multi_url_start;
    int multi_url_end;
    in_port_t port;
    char *path;
    char *multi_path;
    union { // Supported operating systems
        struct { // Single providers (Debian and Canonical)
            char **allowed_os_name;
            char **allowed_os_ver;
        };
        struct { // Multi providers (Red Hat)
            char ***allowed_multios_src_name;
            char ***allowed_multios_src_ver;
            char **allowed_multios_dst_name;
            char **allowed_multios_dst_ver;
        };
    };
    unsigned int attempted:1;
    unsigned int json_format:1;
} update_node;

typedef struct wm_vuldet_t {
    update_node *updates[OS_SUPP_SIZE];
    unsigned long detection_interval;
    unsigned long ignore_time;
    time_t last_detection;
    agent_software *agents_software;
    OSHash *agents_triag;
    int queue_fd;
    wm_vuldet_state state;
    wm_vuldet_flags flags;
} wm_vuldet_t;

typedef enum {
    V_OVALDEFINITIONS,
    V_DEFINITIONS,
    V_TESTS,
    V_OBJECTS,
    V_HEADER,
    V_DESCRIPTION,
    V_SIGNED_TEST,
    V_VARIABLES,
    V_STATES
} parser_state;

// Report queue

typedef struct vu_alerts_node {
    char *version_compare;
    vu_report *report;
    struct vu_alerts_node *next;
} vu_alerts_node;

typedef struct vu_processed_alerts {
    char *cve;
    char *package;
    char *package_version;
    char *package_arch;
    vu_alerts_node *report_queue;
} vu_processed_alerts;

struct vu_report {
    char *cve;
    char *title;
    char *rationale;
    char *published;
    char *updated;
    char *state;
    char pending;
    // Impact
    char *severity;
    cv_scoring_system *cvss2;
    cv_scoring_system *cvss3;
    char *cwe;
    // References
    char *advisories;
    char *bugzilla_reference;
    char *reference;
    char *ref_source;
    // Reported software
    char *software;
    char *generated_cpe;
    char *version;
    char *operation;
    char *operation_value;
    char *condition;
    char *arch;
    // Agent data
    char *agent_id;
    char *agent_name;
    char *agent_ip;
};

typedef struct oval_metadata {
    char *product_name;
    char *product_version;
    char *schema_version;
    char *timestamp;
} oval_metadata;

typedef struct info_state {
    char *id;
    char *operation;
    char *operation_value;
    char *arch_value;
    struct info_state *prev;
} info_state;

typedef struct info_obj {
    char *id;
    char *obj;
    char need_vars;
    struct info_obj *prev;
} info_obj;

typedef struct info_test {
    char *id;
    char *state;
    char *obj;
    struct info_test *prev;
} info_test;

typedef struct file_test {
    char *id;
    char *state;
    struct file_test *prev;
} file_test;

typedef struct info_cve {
    char *cveid;
    char *title; // Not available in Red Hat feed
    char *severity;
    char *published;
    char *updated;
    char *reference;
    char *description;
    char *cvss;
    char *cvss3;
    char *cvss_vector;
    char *bugzilla_reference;
    char *advisories;
    char *cwe;
    int flags;
    struct info_cve *prev;
} info_cve;

typedef struct vulnerability {
    char *cve_id;
    char *state_id;
    char *package_name;
    char pending;
    struct vulnerability *prev;
} vulnerability;

typedef struct rh_vulnerability {
    char *cve_id;
    const char *OS;
    char *OS_minor;
    char *package_name;
    char *package_version;
    struct rh_vulnerability *prev;
} rh_vulnerability;

typedef struct last_scan {
    char *last_scan_id;
    time_t last_scan_time;
} last_scan;

typedef struct variables {
    char *id;
    int elements;
    char **values;
    struct variables *prev;
} variables;

typedef struct wm_vuldet_db {
    vulnerability *vulnerabilities;
    rh_vulnerability *rh_vulnerabilities;
    nvd_vulnerability *nvd_vulnerabilities;
    nvd_metadata *nvd_met;
    vu_cpe_dic *w_cpes;
    vu_msu_entry *msu_entries;
    info_test *info_tests;
    file_test *file_tests;
    info_state *info_states;
    info_obj *info_objs;
    variables *vars;
    info_cve *info_cves;
    cpe_list *nvd_cpes;
    oval_metadata metadata;
    const char *OS;
} wm_vuldet_db;

// NVD - CPE structures

typedef struct translation_cond {
    const char *translation;
    const char *field;
    const char *condition;
} translation_cond;

struct cpe {
    // CPE fields
    char *part;
    char *vendor;
    char *product;
    char *version;
    char *update;
    char *edition;
    char *language;
    char *sw_edition;
    char *target_sw;
    char *target_hw;
    char *other;
    // Extra fields
    int id;
    int pos;
    char *msu_name;
    char *raw;
    char check_hotfix;
    // Multi-fields
    translation_cond *cm_product;
    cpe *next;
};

typedef struct cpe_node {
    cpe *node;
    struct cpe_node *next;
    struct cpe_node *prev;
} cpe_node;

struct cpe_list {
    cpe_node *first;
    cpe_node *last;
};

typedef struct vu_search_terms {
    char *generated_cpe;
    char *o_vendor;
    char *o_product;
    char *o_version;
    char *o_arch;
    char **vendor_terms;
    char **product_terms;
    char **version_terms;
    char **arch_terms;
    struct vu_search_terms *next;
    struct vu_search_terms *prev;
} vu_search_terms;

// NVD -CVE structures

typedef struct nvd_references {
    char *url;
    char *refsource;
    struct nvd_references *next;
} nvd_references;

typedef struct nvd_conf_cpe_match {
    int vulnerable;
    char *cpe_uri23;
    char *version_start_including;
    char *version_start_excluding;
    char *version_end_including;
    char *version_end_excluding;
    cpe *cpe_node;
    struct nvd_conf_cpe_match *next;
} nvd_conf_cpe_match;

typedef struct nvd_configuration {
    char *operator;
    struct nvd_configuration *children;
    nvd_conf_cpe_match *cpe_matches;
    struct nvd_configuration *next;
} nvd_configuration;

struct cv_scoring_system {
    char *version;
    char *vector_string;
    double base_score;
    double exploitability_score;
    double impact_score;
};

struct nvd_vulnerability {
    char *id;
    char *cwe;
    char *description;
    nvd_references *references;
    nvd_configuration *configuration;
    cv_scoring_system *cvss2;
    cv_scoring_system *cvss3;
    char *published;
    char *last_modified;
    struct nvd_vulnerability *next;
};

struct nvd_metadata {
    char *size;
    char *zip_size;
    char *g_size;
    char *sha256;
    char *last_mod;
};

typedef struct vu_nvd_report {
    int id;
    char *raw_product;
    char *raw_version;
    char *raw_arch;
    char *generated_cpe;
    char pending;
    char vulnerable;
    char *necessary_patch;
    char *operator;
    char *version;
    char *v_start_inc;
    char *v_start_exc;
    char *v_end_inc;
    char *v_end_exc;
    char *and_package;
    struct vu_nvd_report *next;
    struct vu_nvd_report *prev;
} vu_nvd_report;

// CPE helper structures

typedef struct vu_cpe_dic_section {
    char ***vendor;
    char ***product;
    char ***version;
    char ***sw_edition;
    char ***msu_name;
} vu_cpe_dic_section;

typedef struct vu_cpe_dic_node {
    vu_cpe_dic_section *source;
    vu_cpe_dic_section *translation;
    unsigned int action;
    struct vu_cpe_dic_node *next;
    struct vu_cpe_dic_node *prev;
} vu_cpe_dic_node;

struct vu_cpe_dic {
    char *version;
    char *format_version;
    char *update_date;
    vu_cpe_dic_node **targets;
    char **targets_name;
};

struct vu_msu_entry {
    char *cveid;
    char *patch;
    char *product;
    char *restart_required;
    char *subtype;
    char *title;
    char *url;
    struct vu_msu_entry *next;
    struct vu_msu_entry *prev;
};

// Macros
#define wm_vuldet_is_single_provider(x) (x == FEED_UBUNTU || x == FEED_DEBIAN)
#define wm_vuldet_silent_feed(x) (x == FEED_CPEW || x == FEED_MSU)
#define wm_vuldet_get_json_value(obj, key) ({void *ret = NULL; cJSON *it = cJSON_GetObjectItem(obj, key); \
                                            if (it) ret = it->valuestring; ret;})

// Function headers
int wm_vuldet_nvd_cpe_parser(char *path, wm_vuldet_db *parsed_vulnerabilities);
int wm_vuldet_insert_cpe_db(sqlite3 *db, cpe_list *node_list, char overwrite);
int wm_vuldet_sql_error(sqlite3 *db, sqlite3_stmt *stmt);
int wm_vuldet_step(sqlite3_stmt *stmt);
int wm_vuldet_get_min_cpe_index(sqlite3 *db, int *min_index);
int wm_vuldet_add_cpe(cpe *ncpe, char *cpe_raw, cpe_list *node_list, int index);
int wm_vuldet_generate_agent_cpes(sqlite3 *db, agent_software *agent, char dic);
int wm_vuldet_fetch_nvd_cve(update_node *update);
int wm_vuldet_fetch_nvd_cpe(char *repo);
int wm_vuldet_json_nvd_parser(cJSON *json_feed, wm_vuldet_db *parsed_vulnerabilities);
int wm_vuldet_clean_nvd_metadata(sqlite3 *db, int year);
int wm_vuldet_insert_nvd_cve(sqlite3 *db, nvd_vulnerability *nvd_data, int year);
void wm_vuldet_free_nvd_node(nvd_vulnerability *data);
int wm_vuldet_nvd_vulnerabilities(sqlite3 *db, agent_software *agent, wm_vuldet_flags *flags);
int wm_checks_package_vulnerability(char *version, const char *operation, const char *operation_value);
int wm_vuldet_send_agent_report(vu_report *report);
void wm_vuldet_free_report(vu_report *report);
void wm_vuldet_free_cv_scoring_system(cv_scoring_system *data);
int wm_vuldet_check_timestamp(const char *target, char *timst, char *ret_timst);
cpe *wm_vuldet_decode_cpe(char *raw_cpe);
int wm_vuldet_index_nvd_metadata(sqlite3 *db, int year, int cve_count, char alternative);
int wm_vuldet_remove_table(sqlite3 *db, char *table);
int wm_vuldet_clean_nvd(sqlite3 *db);
int wm_vuldet_has_offline_update(sqlite3 *db);
int wm_vuldet_clean_nvd_year(sqlite3 *db, int year);
int wm_vuldet_remove_sequence(sqlite3 *db, char *table);
char *wm_vuldet_cpe_str(cpe *cpe_s);
void wm_vuldet_free_cpe(cpe *node);
int wm_vuldet_read(const OS_XML *xml, xml_node **nodes, wmodule *module);
void wm_vuldet_free_update_node(update_node *update);
cpe *wm_vuldet_cpe_cpy(cpe *orig);
cpe *wm_vuldet_generate_cpe(const char *part, const char *vendor, const char *product, const char *version, const char *update, const char *edition, const char *language, const char *sw_edition, const char *target_sw, const char *target_hw, const char *other, const int id, const char *msu_name);

#endif
#endif
