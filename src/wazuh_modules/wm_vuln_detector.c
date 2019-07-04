/*
 * Wazuh Module to analyze system vulnerabilities
 * Copyright (C) 2015-2019, Wazuh Inc.
 * January 4, 2018.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#ifndef WIN32

#include "../wmodules.h"
#include "wm_vuln_detector_db.h"
#include "addagent/manage_agents.h"
#include "wazuh_db/wdb.h"
#include <netinet/tcp.h>
#include <openssl/ssl.h>
#include <os_net/os_net.h>

#if defined(__MACH__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#define SOL_TCP     6
#endif

static void * wm_vuldet_main(wm_vuldet_t * vuldet);
static void wm_vuldet_destroy(wm_vuldet_t * vuldet);
static int wm_vuldet_updatedb(update_node **updates);
static char * wm_vuldet_oval_xml_preparser(char *path, vu_feed dist);
static int wm_vuldet_index_feed(update_node *update);
static int wm_vuldet_fetch_feed(update_node *update, int *need_update);
static int wm_vuldet_oval_xml_parser(OS_XML *xml, XML_NODE node, wm_vuldet_db *parsed_oval, update_node *update, vu_logic condition);
static int wm_vuldet_json_parser(cJSON *json_feed, wm_vuldet_db *uparsed_vulnerabilities, update_node *update);
static void wm_vuldet_add_rvulnerability(wm_vuldet_db *ctrl_block);
static void wm_vuldet_add_vulnerability_info(wm_vuldet_db *ctrl_block);
static char *wm_vuldet_extract_advisories(cJSON *advisories);
static const char *wm_vuldet_decode_package_version(char *raw, const char **OS, char **OS_minor, char **package_name, char **package_version);
static int wm_vuldet_check_db();
static int wm_vuldet_insert(wm_vuldet_db *parsed_oval, update_node *update);
static int wm_vuldet_remove_target_table(sqlite3 *db, char *TABLE, const char *target);
static int wm_vuldet_get_software_info(agent_software *agent, sqlite3 *db, OSHash *agents_triag, unsigned long ignore_time);
static char *wm_vuldet_report_agent_vulnerabilities(sqlite3 *db, agent_software *agents, wm_vuldet_flags *flags, int max);
static int wm_vuldet_check_agent_vulnerabilities(agent_software *agents, wm_vuldet_flags *flags, OSHash *agents_triag, unsigned long ignore_time);
static int wm_vuldet_create_file(const char *path, const char *source);
static int wm_vuldet_check_update_period(update_node *upd);
static int wm_vuldet_update_feed(update_node *upd);
static int wm_vuldet_sync_feed(update_node *upd);
static int wm_vuldet_run_update(update_node *upd, int *error_code);
static int wm_vuldet_compare(char *version_it, char *cversion_it);
static vu_feed wm_vuldet_set_allowed_feed(const char *os_name, const char *os_version, update_node **updates, vu_feed *agent_dist);
static int wm_vuldet_set_agents_info(agent_software **agents_software, update_node **updates);
static cJSON *wm_vuldet_dump(const wm_vuldet_t * vuldet);
static char *wm_vuldet_build_url(char *pattern, char *value);
static void wm_vuldet_adapt_title(char *title, char *cve);
static char *vu_get_version();
static int wm_vuldet_fetch_redhat(update_node *update);
static int wm_vuldet_fetch_oval(update_node *update, char *repo);
static int wm_vuldet_oval_process(update_node *update, char *path, wm_vuldet_db *parsed_vulnerabilities);
static int wm_vuldet_json_rh_parser(cJSON *json_feed, wm_vuldet_db *parsed_vulnerabilities);
static int wm_vuldet_db_empty();
static const char *wm_vuldet_get_unified_severity(char *severity);
static void wm_vuldet_generate_os_cpe(agent_software *agent);
static void wm_vuldet_generate_win_cpe(agent_software *agent);
static vu_feed wm_vuldet_decode_win_os(char *os_raw);
static int wm_vuldet_insert_agent_data(sqlite3 *db, agent_software *agent, int *cpe_index, const char *vendor, const char *product, const char *version, const char *arch, cpe *ag_cpe, cpe_list **node_list);
static void wm_vuldet_free_agent_software(agent_software *agent);
static int wm_vuldet_fetch_MSU();
static int wm_vuldet_fetch_wazuh_cpe(update_node *update);
static int wm_vuldet_json_wcpe_parser(cJSON *json_feed, wm_vuldet_db *parsed_vulnerabilities);
static int wm_vuldet_json_dic_parser(cJSON *json_feed, vu_cpe_dic *wcpes);
static int wm_vuldet_fill_dic_section(cJSON *json_feed, vu_cpe_dic_section **wcpes);
static unsigned int wm_vuldet_fill_action_mask(cJSON *json_feed);
static int wm_vuldet_json_msu_parser(cJSON *json_feed, wm_vuldet_db *parsed_vulnerabilities);
static int wm_vuldet_insert_cpe_dic(sqlite3 *db, vu_cpe_dic *w_cpes);
static int wm_vuldet_insert_cpe_dic_array(sqlite3 *db, vu_query query, int id, vu_cpe_dic_section *section);
static vu_cpe_dic_node *wm_vuldet_free_dic_node(vu_cpe_dic_node *dic_node);
static int wm_vuldet_insert_MSU(sqlite3 *db, vu_msu_entry *msu);
static vu_msu_entry *wm_vuldet_free_msu_node(vu_msu_entry *node);
static int wm_vuldet_clean_agent_cpes(int sock, char *agent_id);
static int wm_vuldet_get_oswindows_info(int sock, agent_software *agent);
static int wm_vuldet_request_hotfixes(sqlite3 *db, int sock, char *agent_id);
static void wm_vuldet_reset_tables(sqlite3 *db);
static int wm_vuldet_index_json(wm_vuldet_db *parsed_vulnerabilities, update_node *update, char *path, char multi_path);
static int wm_vuldet_index_nvd(sqlite3 *db, update_node *upd, nvd_vulnerability *nvd_it);
static int wm_vuldet_index_redhat(sqlite3 *db, update_node *upd, rh_vulnerability *r_it);
static int wm_vuldet_clean_rh(sqlite3 *db);
static int wm_vuldet_clean_wcpe(sqlite3 *db);
static void wm_vuldet_get_package_os(const char *version, const char **os_major, char **os_minor);
static void wm_vuldet_set_subversion(char *version, char **os_minor);
static void wm_vuldet_queue_report(vu_processed_alerts **alerts_queue, char *cve, char *package, char *package_version, char *package_arch, char *version_compare, vu_report *report);
static void wm_vuldet_queue_report_higher(vu_processed_alerts **alerts_queue);
static void wm_vuldet_queue_report_add(vu_processed_alerts **alerts_queue, char *version_compare, vu_report *report);
static void wm_vuldet_queue_report_clean(vu_processed_alerts **alerts_queue);
static cJSON *wm_vuldet_json_fread(char *json_file);
static cJSON *wm_vuldet_get_cvss(const char *scoring_vector);
static int wm_vuldet_get_dist_ref(const char *dist_name, const char *dist_ver, int *dist_ref, int *dist_ver_ref);
static int wm_vuldet_get_term_condition(char *i_term, char **term, char **comp_field, char **condition);

int *vu_queue;
// Define time to sleep between messages sent
int usec;

const wm_context WM_VULNDETECTOR_CONTEXT = {
    "vulnerability-detector",
    (wm_routine)wm_vuldet_main,
    (wm_routine)(void *)wm_vuldet_destroy,
    (cJSON * (*)(const void *))wm_vuldet_dump
};

const char *vu_package_dist[] = {
    ".el",
    ".el7",
    ".el6",
    ".el5",
    "ubuntu",
    ".amzn"
};

const char *vu_feed_tag[] = {
    "UBUNTU",
    "CANONICAL",
    "DEBIAN",
    "REDHAT",
    "CENTOS",
    "AMAZLINUX",
    "WIN",
    // Ubuntu versions
    "PRECISE",
    "TRUSTY",
    "XENIAL",
    "BIONIC",
    // Debian versions
    "JESSIE",
    "STRETCH",
    "WHEEZY",
    // RedHat versions
    "RHEL5",
    "RHEL6",
    "RHEL7",
    // NVD
    "NVD",
    "CPED",
    // WINDOWS
    "WS2003",
    "WS2003R2",
    "WXP",
    "WVISTA",
    "W7",
    "W8",
    "W81",
    "W10",
    "WS2008",
    "WS2008R2",
    "WS2012",
    "WS2012R2",
    "WS2016",
    "WS2019",
    // OTHER
    "CPEW",
    "MICROSOFT",
    "UNKNOWN"
};

const char *vu_feed_ext[] = {
    "Ubuntu",
    "Canonical",
    "Debian",
    "Red Hat Enterprise Linux",
    "CentOS",
    "Amazon Linux",
    "Microsoft Windows",
    // Ubuntu versions
    "Ubuntu Precise",
    "Ubuntu Trusty",
    "Ubuntu Xenial",
    "Ubuntu Bionic",
    // Debian versions
    "Debian Jessie",
    "Debian Stretch",
    "Debian Wheezy",
    // RedHat versions
    "Red Hat Enterprise Linux 5",
    "Red Hat Enterprise Linux 6",
    "Red Hat Enterprise Linux 7",
    // NVD
    "National Vulnerability Database",
    "Common Platform Enumeration Dictionary",
    // WINDOWS
    "Windows Server 2003",
    "Windows Server 2003 R2",
    "Windows XP",
    "Windows Vista",
    "Windows 7",
    "Windows 8",
    "Windows 8.1",
    "Windows 10",
    "Windows Server 2008",
    "Windows Server 2008 R2",
    "Windows Server 2012",
    "Windows Server 2012 R2",
    "Windows Server 2016",
    "Windows Server 2019",
    // OTHER
    "Wazuh CPE dictionary",
    "Microsoft Security Update",
    "Unknown OS"
};

const char *vu_package_comp[] = {
    "less than",
    "less than or equal",
    "greater than",
    "greater than or equal",
    "equal",
    "not equal",
    "exists"
};

const char *vu_severities[] = {
    "Low",
    "Medium",
    "Moderate",
    "Unknown",
    "High",
    "Important",
    "Critical",
    "None",
    "Negligible",
    "Untriaged",
    "-"
};

const char *vu_cpe_dic_option[] = {
    "replace_vendor",
    "replace_product",
    "replace_vendor_if_matches",
    "replace_product_if_matches",
    "set_version_if_matches",
    "replace_sw_edition_if_product_matches",
    "replace_msu_name_if_version_matches",
    "ignore",
    "check_hotfix",
    "replace_msu_name",
    "set_version_if_product_matches",
    "replace_arch_if_product_matches",
    "set_version_only_if_product_matches",
    "replace_arch_only_if_product_matches"
};

vu_feed wm_vuldet_set_allowed_feed(const char *os_name, const char *os_version, update_node **updates, vu_feed *agent_dist) {
    vu_feed retval = FEED_UNKNOW;
    int i, j, k;

    for (i = 0; i < OS_SUPP_SIZE; i++) {
        if (updates[i]) {
            if (wm_vuldet_is_single_provider(updates[i]->dist_ref)) {
                if (updates[i]->allowed_os_name) {
                    int j;
                    char *allowed_os;
                    char *allowed_ver;
                    for (allowed_os = *updates[i]->allowed_os_name, allowed_ver = *updates[i]->allowed_os_ver, j = 0; allowed_os; ++j) {
                        if (strcasestr(os_name, allowed_os) && strcasestr(os_version, allowed_ver)) {
                            retval = updates[i]->dist_tag_ref;
                            *agent_dist = updates[i]->dist_ref;
                            i = OS_SUPP_SIZE;
                            break;
                        }
                        allowed_os = updates[i]->allowed_os_name[j];
                        allowed_ver = updates[i]->allowed_os_ver[j];
                    }
                }
            } else {
                if (updates[i]->allowed_multios_src_name) {
                    for (j = 0; updates[i]->allowed_multios_src_name[j]; j++) {
                        for (k = 0; updates[i]->allowed_multios_src_name[j][k]; k++) {
                            if (strcasestr(os_name, updates[i]->allowed_multios_src_name[j][k]) &&
                                    strcasestr(os_version, updates[i]->allowed_multios_src_ver[j][k])) {
                                int dist_tag_ref;
                                int dist_ref;

                                if (wm_vuldet_get_dist_ref(updates[i]->allowed_multios_dst_name[j], updates[i]->allowed_multios_dst_ver[j],
                                        &dist_ref, &dist_tag_ref)) {
                                    mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_INVALID_TRANSLATION_OS, updates[i]->allowed_multios_dst_name[j], updates[i]->allowed_multios_dst_ver[j]);
                                    goto end;
                                }

                                retval = dist_tag_ref;
                                *agent_dist = dist_ref;
                                goto end;
                            }
                        }
                    }
                }
            }
        }
    }

end:
    return retval;
}

int wm_vuldet_check_update_period(update_node *upd) {
    return upd && (!upd->last_update || (upd->interval != WM_VULNDETECTOR_ONLY_ONE_UPD && (upd->last_update + (time_t) upd->interval) < time(NULL)));
}
int wm_vuldet_sync_feed(update_node *upd) {
    int need_update = 1;
    return wm_vuldet_fetch_feed(upd, &need_update) == OS_INVALID || (need_update && wm_vuldet_index_feed(upd));
}

int wm_vuldet_create_file(const char *path, const char *source) {
    const char *ROOT = "root";
    const char *sql;
    const char *tail;
    sqlite3 *db;
    sqlite3_stmt *stmt = NULL;
    int result;
    uid_t uid;
    gid_t gid;

    // Open the database file, or create it
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL)) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_CREATE_DB_ERROR);
        return wm_vuldet_sql_error(db, stmt);
    }

    // Load the tables schema
    for (sql = source; sql && *sql; sql = tail) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, &tail) != SQLITE_OK) {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_CREATE_DB_ERROR);
            return wm_vuldet_sql_error(db, stmt);
        }

        result = wm_vuldet_step(stmt);

        switch (result) {
        case SQLITE_MISUSE:
        case SQLITE_ROW:
        case SQLITE_DONE:
            break;
        default:
            return wm_vuldet_sql_error(db, stmt);
        }

        sqlite3_finalize(stmt);
    }

    // Add the database version
    if (sqlite3_prepare_v2(db, vu_queries[VU_INSERT_METADATA_DB], -1, &stmt, &tail) != SQLITE_OK) {
        return wm_vuldet_sql_error(db, stmt);
    }

    sqlite3_bind_text(stmt, 1, vu_get_version(), -1, NULL);

    // Insert the database version
    if (wm_vuldet_step(stmt) != SQLITE_DONE) {
        return wm_vuldet_sql_error(db, stmt);
    }
    sqlite3_finalize(stmt);

    sqlite3_close_v2(db);

    uid = Privsep_GetUser(ROOT);
    gid = Privsep_GetGroup(GROUPGLOBAL);

    if (uid == (uid_t) - 1 || gid == (gid_t) - 1) {
        mterror(WM_VULNDETECTOR_LOGTAG, USER_ERROR, ROOT, GROUPGLOBAL);
        return OS_INVALID;
    }

    if (chown(path, uid, gid) < 0) {
        mterror(WM_VULNDETECTOR_LOGTAG, CHOWN_ERROR, path, errno, strerror(errno));
        return OS_INVALID;
    }

    if (chmod(path, 0660) < 0) {
        mterror(WM_VULNDETECTOR_LOGTAG, CHMOD_ERROR, path, errno, strerror(errno));
        return OS_INVALID;
    }

    return 0;
}

int wm_vuldet_step(sqlite3_stmt *stmt) {
    int attempts;
    int result;
    for (attempts = 0; (result = sqlite3_step(stmt)) == SQLITE_BUSY; attempts++) {
        if (attempts == MAX_SQL_ATTEMPTS) {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_MAX_ACC_EXC);
            return OS_INVALID;
        }
    }
    return result;
}

int wm_checks_package_vulnerability(char *version, const char *operation, const char *operation_value) {
    int size;
    int v_result, r_result;
    int epoch, c_epoch;
    char version_cl[KEY_SIZE];
    char cversion_cl[KEY_SIZE];
    char *version_it, *release_it;
    char *cversion_it, *crelease_it;

    if (operation_value) {
        // Copy the original values
        if (size = snprintf(version_cl, KEY_SIZE, "%s", version), size >= KEY_SIZE) {
            return OS_INVALID;
        }
        if (size = snprintf(cversion_cl, KEY_SIZE, "%s", operation_value), size >= KEY_SIZE) {
            return OS_INVALID;
        }

        // Check EPOCH
        if (version_it = strchr(version_cl, ':'), version_it) {
            *(version_it++) = '\0';
            epoch = strtol(version_cl, NULL, 10);
        } else {
            version_it = version_cl;
            epoch = 0;
        }
        if (cversion_it = strchr(cversion_cl, ':'), cversion_it) {
            *(cversion_it++) = '\0';
            c_epoch = strtol(cversion_cl, NULL, 10);
        } else {
            cversion_it = cversion_cl;
            c_epoch = 0;
        }

        // Separate the version from the revision
        if (release_it = strchr(version_it, '-'), release_it) {
            if (*(release_it++) = '\0', *release_it == '\0') {
                release_it = NULL;
            }
        }

        if (crelease_it = strchr(cversion_it, '-'), crelease_it) {
            if (*(crelease_it++) = '\0', *crelease_it == '\0') {
                crelease_it = NULL;
            }
        }

        // Check version
        if (v_result = wm_vuldet_compare(version_it, cversion_it), v_result == VU_ERROR_CMP) {
            return VU_ERROR_CMP;
        }
        // Check release
        if (r_result = wm_vuldet_compare(release_it, crelease_it), r_result == VU_ERROR_CMP) {
            return VU_ERROR_CMP;
        }

        if (!strncmp(operation, vu_package_comp[VU_COMP_L], strlen(vu_package_comp[VU_COMP_L]))) {
            // Check for "less than" and "less than or equal"
            if (epoch > c_epoch) {
                return VU_NOT_VULNERABLE;
            } else if (epoch < c_epoch) {
                return VU_VULNERABLE;
            }

            if (v_result == VU_LESS) {
                return VU_VULNERABLE;
            } else if (v_result == VU_HIGHER) {
                return VU_NOT_VULNERABLE;
            }

            if (r_result == VU_LESS) {
                return VU_VULNERABLE;
            } else if (r_result == VU_EQUAL && !strcmp(operation, vu_package_comp[VU_COMP_LE])) {
                return VU_VULNERABLE;
            }
        } else if (!strncmp(operation, vu_package_comp[VU_COMP_G], strlen(vu_package_comp[VU_COMP_G]))) {
            // Check for "greater than" and "greater than or equal"
            if (epoch > c_epoch) {
                return VU_VULNERABLE;
            } else if (epoch < c_epoch) {
                return VU_NOT_VULNERABLE;
            }

            if (v_result == VU_LESS) {
                return VU_NOT_VULNERABLE;
            } else if (v_result == VU_HIGHER) {
                return VU_VULNERABLE;
            }

            if (r_result == VU_HIGHER) {
                return VU_VULNERABLE;
            } else if (r_result == VU_EQUAL && !strcmp(operation, vu_package_comp[VU_COMP_GE])) {
                return VU_VULNERABLE;
            }
        } else if (!strncmp(operation, vu_package_comp[VU_COMP_EQ], strlen(vu_package_comp[VU_COMP_EQ]))) {
            // Check for "equal"
            if (epoch != c_epoch) {
                return VU_NOT_VULNERABLE;
            }

            if (v_result != VU_EQUAL) {
                return VU_NOT_VULNERABLE;
            }

            if (r_result == VU_EQUAL) {
                return VU_VULNERABLE;
            }
        } else if (!strncmp(operation, vu_package_comp[VU_COMP_NEQ], strlen(vu_package_comp[VU_COMP_NEQ]))) {
            // Check for "not equal"
            if (epoch != c_epoch) {
                return VU_VULNERABLE;
            }

            if (v_result != VU_EQUAL) {
                return VU_VULNERABLE;
            }

            if (r_result == VU_EQUAL) {
                return VU_NOT_VULNERABLE;
            }
        } else if (!strcmp(operation, vu_package_comp[VU_COMP_EX])) {
            // Check for "exists"
            return VU_VULNERABLE;
        } else {
            mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_OPERATION_NOT_REC, operation);
            return VU_NOT_VULNERABLE;
        }

        return VU_NOT_VULNERABLE;
    }
    return VU_NOT_FIXED;
}

int wm_vuldet_compare(char *version_it, char *cversion_it) {
    char *found;
    int i, j, it;
    int version_found, cversion_found;
    int version_value, cversion_value;

    if (version_it && !cversion_it) {
        return VU_HIGHER;
    } else if (!version_it && cversion_it) {
        return VU_LESS;
    } else if (!version_it && !cversion_it) {
        return VU_EQUAL;
    }

    (found = strchr(version_it, '~')) ? *found = '\0' : 0;
    (found = strchr(version_it, '-')) ? *found = '\0' : 0;
    (found = strchr(version_it, '+')) ? *found = '\0' : 0;
    (found = strchr(cversion_it, '~')) ? *found = '\0' : 0;
    (found = strchr(cversion_it, '-')) ? *found = '\0' : 0;
    (found = strchr(cversion_it, '+')) ? *found = '\0' : 0;

    // For RedHat/CentOS packages
    (found = strstr(version_it, vu_package_dist[VU_RH_EXT_BASE])) ? *found = '\0' : 0;
    (found = strstr(cversion_it, vu_package_dist[VU_RH_EXT_BASE])) ? *found = '\0' : 0;

    // For Ubuntu packages
    (found = strstr(version_it, vu_package_dist[VU_UB_EXT]))? *found = '\0' : 0;
    (found = strstr(cversion_it, vu_package_dist[VU_UB_EXT]))? *found = '\0' : 0;

    // For Amazon Linux packages
    (found = strstr(version_it, vu_package_dist[VU_AMAZ_EXT])) ? *found = '\0' : 0;
    (found = strstr(cversion_it, vu_package_dist[VU_AMAZ_EXT])) ? *found = '\0' : 0;

    // Check version
    if (strcmp(version_it, cversion_it)) {
        for (it = 0, i = 0, j = 0, version_found = 0, cversion_found = 0; it < VU_MAX_VER_COMP_IT; it++) {
            if (!version_found) {
                if (version_it[i] == '\0') {
                    version_found = 3;
                } else if (!isdigit(version_it[i])) {
                    if (i) {
                        // There is a number to compare
                        version_found = 1;
                    } else {
                        if (isalpha(version_it[i]) && !isalpha(version_it[i + 1])) {
                            // There is an individual letter to compare
                            version_found = 2;
                        } else {
                            // Skip characters that are not comparable
                            for (; *version_it != '\0' && !isdigit(*version_it); version_it++);
                            i = 0;
                        }
                    }
                } else {
                    i++;
                }
            }

            if (!cversion_found) {
                if (cversion_it[j] == '\0') {
                    cversion_found = 3;
                } else if (!isdigit(cversion_it[j])) {
                    if (j) {
                        // There is a number to compare
                        cversion_found = 1;
                    } else {
                        if (isalpha(cversion_it[j]) && !isalpha(cversion_it[j + 1])) {
                            // There is an individual letter to compare
                            cversion_found = 2;
                        } else {
                            // Skip characters that are not comparable
                            for (; *cversion_it != '\0' && !isdigit(*cversion_it); cversion_it++);
                            j = 0;
                        }
                    }
                } else {
                    j++;
                }
            }

            if (version_found && cversion_found) {
                if (version_found == 2 && version_found == cversion_found) {
                    // Check version letter
                    version_value = *version_it;
                    cversion_value = *cversion_it;
                } else {
                    version_value = strtol(version_it, NULL, 10);
                    cversion_value = strtol(cversion_it, NULL, 10);
                }
                if (version_value > cversion_value) {
                    return VU_HIGHER;
                } else if (version_value < cversion_value) {
                    return VU_LESS;
                } else if (version_found != cversion_found) {
                    // The version with more digits is higher
                    if (version_found < cversion_found) {
                        return VU_HIGHER;
                    } else {
                        return VU_LESS;
                    }
                } else if (version_found > 2) {
                    // The version is over
                    break;
                }
                version_found = 0;
                cversion_found = 0;
                version_it = &version_it[i ? i : 1];
                cversion_it = &cversion_it[j ? j : 1];
                i = 0;
                j = 0;
            }
        }
        if (it == VU_MAX_VER_COMP_IT) {
            return VU_ERROR_CMP;
        }
    }

    return VU_EQUAL;
}

char *wm_vuldet_build_url(char *pattern, char *value) {
    size_t size;
    char *retval;

    os_calloc(VUL_BUILD_REF_MAX + 1, sizeof(char), retval);
    size = snprintf(retval, VUL_BUILD_REF_MAX, pattern, value);
    os_realloc(retval, size + 1, retval);

    return retval;
}

cJSON *wm_vuldet_decode_advisories(char *advisories) {
    cJSON *json_advisories = cJSON_CreateObject();
    char *found;
    char *str_base = advisories;

    while (str_base && *str_base) {
        char *patch_url;
        if (found = strchr(str_base, ','), found) {
            *(found++) = '\0';
        }

        patch_url = wm_vuldet_build_url(VU_BUILD_REF_RHSA, str_base);
        cJSON_AddItemToObject(json_advisories, str_base, cJSON_CreateString(patch_url));
        str_base = found;
        free(patch_url);
    }

    return json_advisories;
}

void wm_vuldet_free_report(vu_report *report) {
    free(report->cve);
    free(report->title);
    free(report->rationale);
    free(report->severity);
    free(report->published);
    free(report->updated);
    free(report->state);
    wm_vuldet_free_cv_scoring_system(report->cvss2);
    wm_vuldet_free_cv_scoring_system(report->cvss3);
    free(report->cwe);
    free(report->advisories);
    free(report->bugzilla_reference);
    free(report->reference);
    free(report->software);
    free(report->generated_cpe);
    free(report->version);
    free(report->operation);
    free(report->operation_value);
    free(report->agent_id);
    free(report->agent_name);
    free(report->agent_ip);
    free(report->ref_source);
    free(report->condition);
    free(report->arch);
}

int wm_vuldet_send_agent_report(vu_report *report) {
    cJSON *alert = NULL;
    cJSON *alert_cve = NULL;
    int retval = OS_INVALID;
    int send_queue;
    char *str_json = NULL;
    char header[OS_SIZE_256 + 1];
    char alert_msg[OS_MAXSTR + 1];
    // Define time to sleep between messages sent
    int usec = 1000000 / wm_max_eps;

    if (alert = cJSON_CreateObject(), !alert) {
        return retval;
    }

    if (alert_cve = cJSON_CreateObject(), alert_cve) {
        cJSON *j_software = NULL;
        cJSON *j_cvss = NULL;

        if (j_software = cJSON_CreateObject(), !j_software) {
            goto end;
        }

        if (report->cvss2 || report->cvss3) {
            cJSON *j_cvss_node;
            int i;

            if (j_cvss = cJSON_CreateObject(), !j_cvss) {
                goto end;
            }

            for (i = 0; i < 2; i++) {
                cv_scoring_system *cvss = !i ? report->cvss2 : report->cvss3;
                if (cvss) {
                    if (j_cvss_node = cJSON_CreateObject(), !j_cvss_node) {
                        goto end;
                    }
                    cJSON_AddItemToObject(j_cvss, !i ? "cvss2" : "cvss3", j_cvss_node);
                    if (cvss->vector_string) cJSON_AddItemToObject(j_cvss_node, "vector", wm_vuldet_get_cvss(cvss->vector_string));
                    if (cvss->base_score) cJSON_AddItemToObject(j_cvss_node, "base_score", cJSON_CreateNumber(cvss->base_score));
                    if (cvss->exploitability_score) cJSON_AddItemToObject(j_cvss_node, "exploitability_score", cJSON_CreateNumber(cvss->exploitability_score));
                    if (cvss->impact_score) cJSON_AddItemToObject(j_cvss_node, "impact_score", cJSON_CreateNumber(cvss->impact_score));
                }
            }
        }

        // Set the alert body
        cJSON_AddItemToObject(alert, "vulnerability", alert_cve);
        cJSON_AddStringToObject(alert_cve, "cve", report->cve);
        cJSON_AddStringToObject(alert_cve, "title", report->title && *report->title ? report->title : report->rationale);
        cJSON_AddStringToObject(alert_cve, "severity", wm_vuldet_get_unified_severity(report->severity));
        cJSON_AddStringToObject(alert_cve, "published", report->published);
        if (report->updated) cJSON_AddStringToObject(alert_cve, "updated", report->updated);
        if (*report->state) cJSON_AddStringToObject(alert_cve, "state", report->state);
        if (j_cvss) cJSON_AddItemToObject(alert_cve, "cvss", j_cvss);
        if (j_software) {
            cJSON_AddItemToObject(alert_cve, "software", j_software);
            cJSON_AddStringToObject(j_software, "name", report->software);
            if (report->version && *report->version) cJSON_AddStringToObject(j_software, "version", report->version);
            if (report->generated_cpe) cJSON_AddStringToObject(j_software, "generated_cpe", report->generated_cpe);
            if (report->arch && *report->arch) cJSON_AddStringToObject(j_software, "architecture", report->arch);
        }
        if (!report->pending) {
            if (report->condition && *report->condition != '\0') {
                cJSON_AddStringToObject(alert_cve, "condition", report->condition);
            } else if (report->operation_value) {
                free(report->condition);
                os_calloc(OS_SIZE_1024 + 1, sizeof(char), report->condition);
                snprintf(report->condition, OS_SIZE_1024, "Package %s %s", report->operation, report->operation_value);
                cJSON_AddStringToObject(alert_cve, "condition", report->condition);
            } else {
                 cJSON_AddStringToObject(alert_cve, "condition", report->operation);
             }
        }
        if (report->advisories && *report->advisories) {
            cJSON_AddStringToObject(alert_cve, "advisories", report->advisories);
        }
        if (report->cwe) cJSON_AddStringToObject(alert_cve, "cwe_reference", report->cwe);
        if (report->bugzilla_reference) cJSON_AddStringToObject(alert_cve, "bugzilla_reference", report->bugzilla_reference);
        if (report->reference) {
            cJSON_AddStringToObject(alert_cve, "reference", report->reference);
        } else {
            // Skip rationale if reference is provided
            cJSON_AddStringToObject(alert_cve, "rationale", report->rationale);
        }
    } else {
        goto end;
    }

    str_json = cJSON_PrintUnformatted(alert);

    // Send an alert as a manager if there is no IP assigned
    if (report->agent_ip) {
        snprintf(header, OS_SIZE_256, VU_ALERT_HEADER, atoi(report->agent_id), report->agent_name, report->agent_ip);
        snprintf(alert_msg, OS_MAXSTR, VU_ALERT_JSON, str_json);
        send_queue = SECURE_MQ;
    } else {
        snprintf(header, OS_SIZE_256, "%s", VU_WM_NAME);
        snprintf(alert_msg, OS_MAXSTR, "%s", str_json);
        send_queue = LOCALFILE_MQ;
    }

    if (wm_sendmsg(usec, *vu_queue, alert_msg, header, send_queue) < 0) {
        mterror(WM_VULNDETECTOR_LOGTAG, QUEUE_ERROR, DEFAULTQUEUE, strerror(errno));
        if ((*vu_queue = StartMQ(DEFAULTQUEUE, WRITE)) < 0) {
            mterror_exit(WM_VULNDETECTOR_LOGTAG, QUEUE_FATAL, DEFAULTQUEUE);
        }
    }

    retval = 0;
end:
    free(str_json);
    cJSON_Delete(alert);
    return retval;
}

char *wm_vuldet_report_agent_vulnerabilities(sqlite3 *db, agent_software *agents, wm_vuldet_flags *flags, int max) {
    sqlite3_stmt *stmt = NULL;
    char condition[OS_SIZE_1024 + 1];
    char state[50];
    const char *query;
    agent_software *agents_it;
    char *cve;
    char *title;
    char *severity;
    char *published;
    char *updated;
    char *reference;
    char *rationale;
    int i;
    int sql_result;
    vu_report *report;
    vu_processed_alerts *alerts_queue = NULL;

    for (agents_it = agents, i = 0; agents_it && i < max; agents_it = agents_it->prev, i++) {
        time_t start = time(NULL);

        if (!agents_it->info) {
            continue;
        }

        mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_START_AG_AN, agents_it->agent_id);

        if (agents_it->dist == FEED_WIN) {
            if (wm_vuldet_nvd_vulnerabilities(db, agents_it, flags)) {
                goto error;
            }
            mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_AGENT_FINISH, agents_it->agent_id);
            continue;
        }

        if (agents_it->dist != FEED_REDHAT) {
            query = vu_queries[VU_JOIN_QUERY];
        } else {
            query = vu_queries[VU_JOIN_RH_QUERY];
        }

        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
            goto error;
        }

        if (agents_it->dist != FEED_REDHAT) {
            sqlite3_bind_text(stmt, 1, vu_feed_tag[agents_it->dist_ver], -1, NULL);
            sqlite3_bind_int(stmt, 2,  strtol(agents_it->agent_id, NULL, 10));
        } else {
            sqlite3_bind_text(stmt, 1, vu_feed_tag[FEED_RHEL5], -1, NULL);
            sqlite3_bind_text(stmt, 2, vu_feed_tag[FEED_RHEL6], -1, NULL);
            sqlite3_bind_text(stmt, 3, vu_feed_tag[FEED_RHEL7], -1, NULL);
            sqlite3_bind_int(stmt, 4,  strtol(agents_it->agent_id, NULL, 10));
            sqlite3_bind_text(stmt, 5, vu_feed_tag[agents_it->dist_ver], -1, NULL);
        }

        while (sql_result = wm_vuldet_step(stmt), sql_result == SQLITE_ROW) {
            char *package;
            char *version;
            char *arch;
            char *operation;
            char *operation_value;
            int pending = 0;
            char *cvss;
            char *cvss3;
            char *cvss_vector;
            char *bugzilla_reference;
            char *cwe;
            char *advisories;
            int v_type;

            cve = (char *)sqlite3_column_text(stmt, 0);
            package = (char *)sqlite3_column_text(stmt, 1);
            title = (char *)sqlite3_column_text(stmt, 2);
            severity = (char *)sqlite3_column_text(stmt, 3);
            published = (char *)sqlite3_column_text(stmt, 4);
            updated = (char *)sqlite3_column_text(stmt, 5);
            reference = (char *)sqlite3_column_text(stmt, 6);
            rationale = (char *)sqlite3_column_text(stmt, 7);
            version = (char *)sqlite3_column_text(stmt, 8);
            arch = (char *)sqlite3_column_text(stmt, 9);
            operation = (char *)sqlite3_column_text(stmt, 10);
            operation_value = (char *)sqlite3_column_text(stmt, 11);
            pending = sqlite3_column_int(stmt, 12);
            cvss = (char *)sqlite3_column_text(stmt, 13);
            cvss3 = (char *)sqlite3_column_text(stmt, 14);
            cvss_vector = (char *)sqlite3_column_text(stmt, 15);
            bugzilla_reference = (char *)sqlite3_column_text(stmt, 16);
            cwe = (char *)sqlite3_column_text(stmt, 17);
            advisories = (char *)sqlite3_column_text(stmt, 18);

            *condition = '\0';
            *state = '\0';
            if (pending) {
                snprintf(state, 30, "Pending confirmation");
            } else {
                if (v_type = wm_checks_package_vulnerability(version, operation, operation_value), v_type == OS_INVALID) {
                    goto error;
                }
                if (v_type == VU_NOT_FIXED) {
                    snprintf(state, 15, "Unfixed");
                    mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_PACK_VULN, package, cve);
                } else if (v_type == VU_NOT_VULNERABLE) {
                    mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_NOT_VULN, package, agents_it->agent_id, cve, version, operation, operation_value);
                    continue;
                } else if (v_type == VU_ERROR_CMP) {
                    mtdebug1(WM_VULNDETECTOR_LOGTAG, "The '%s' and '%s' versions of '%s' package could not be compared. Possible false positive.", version, operation_value, package);
                    snprintf(condition, OS_SIZE_1024, "Could not compare package versions (%s %s).", operation, operation_value);
                } else {
                    snprintf(state, 15, "Fixed");
                    mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_PACK_VER_VULN, package, agents_it->agent_id, cve, version, operation, operation_value);
                }
            }

            // Fill the report data
            os_calloc(1, sizeof(vu_report), report);

            w_strdup(cve, report->cve);
            w_strdup(title, report->title);
            w_strdup(rationale, report->rationale);
            w_strdup(severity, report->severity);
            w_strdup(published, report->published);
            w_strdup(updated, report->updated);
            os_strdup(state, report->state);
            if (*condition) {
                os_strdup(condition, report->condition);
            }
            report->pending = pending;
            if (cvss) {
                os_calloc(1, sizeof(cv_scoring_system), report->cvss2);
                w_strdup(cvss_vector, report->cvss2->vector_string);
                if (cvss) sscanf(cvss, "%lf", &report->cvss2->base_score);
            }

            if (cvss3) {
                os_calloc(1, sizeof(cv_scoring_system), report->cvss3);
                if (cvss) sscanf(cvss3, "%lf", &report->cvss3->base_score);
            }

            w_strdup(cwe, report->cwe);
            w_strdup(advisories, report->advisories);
            w_strdup(bugzilla_reference, report->bugzilla_reference);
            w_strdup(reference, report->reference);
            w_strdup(package, report->software);
            w_strdup(version, report->version);
            w_strdup(arch, report->arch);
            w_strdup(operation, report->operation);
            w_strdup(operation_value, report->operation_value);
            w_strdup(agents_it->agent_id, report->agent_id);
            w_strdup(agents_it->agent_name, report->agent_name);
            w_strdup(agents_it->agent_ip, report->agent_ip);

            wm_vuldet_queue_report(&alerts_queue, cve, package, version, arch, operation_value, report);
        }

        if (alerts_queue) {
            wm_vuldet_queue_report_higher(&alerts_queue);
        }

        sqlite3_finalize(stmt);
        stmt = NULL;
        mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_AGENT_FINISH, agents_it->agent_id);
        mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_FUNCTION_TIME, time(NULL) - start, "find", agents_it->agent_id);
    }

    return NULL;
error:
    if (alerts_queue) {
        wm_vuldet_queue_report_clean(&alerts_queue);
    }
    wdb_finalize(stmt);

    return agents_it->agent_id;
}


int wm_vuldet_check_agent_vulnerabilities(agent_software *agents, wm_vuldet_flags *flags, OSHash *agents_triag, unsigned long ignore_time) {
    agent_software *agents_it;
    sqlite3 *db;
    sqlite3_stmt *stmt = NULL;
    char *agent_error;
    int result;
    int i;

    if (!agents) {
        mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_AG_NO_TARGET);
        return 0;
    } else if (wm_vuldet_check_db()) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_CHECK_DB_ERROR);
        return OS_INVALID;
    } else if (sqlite3_open_v2(CVE_DB, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        return wm_vuldet_sql_error(db, stmt);
    }

    wm_vuldet_reset_tables(db);

    for (i = 1, agents_it = agents;; i++) {
        mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_AGENT_START, agents_it->agent_id);
        if (result = wm_vuldet_get_software_info(agents_it, db, agents_triag, ignore_time), result == OS_INVALID) {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_GET_SOFTWARE_ERROR, agents_it->agent_id);
        }

        if (result != 2) {  // There exists packages for the agent
            if (VU_AGENT_REQUEST_LIMIT && i >= VU_AGENT_REQUEST_LIMIT) {
                // Report a group of agents
                if (agent_error = wm_vuldet_report_agent_vulnerabilities(db, agents_it, flags, i), agent_error) {
                    mterror(WM_VULNDETECTOR_LOGTAG, VU_REPORT_ERROR, agent_error, sqlite3_errmsg(db));
                }
                i = 0;
                // Reset the tables
                wm_vuldet_reset_tables(db);
            }
        }
        if (agents_it->next) {
            agents_it = agents_it->next;
        } else {
            break;
        }
    }

    // Check for unreported agents
    if (!VU_AGENT_REQUEST_LIMIT || (i != VU_AGENT_REQUEST_LIMIT)) {
        if (agent_error = wm_vuldet_report_agent_vulnerabilities(db, agents_it, flags, i), agent_error) {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_REPORT_ERROR, agent_error, sqlite3_errmsg(db));
        }
    }

    sqlite3_close_v2(db);
    return 0;
}

int wm_vuldet_sql_error(sqlite3 *db, sqlite3_stmt *stmt) {
    mterror(WM_VULNDETECTOR_LOGTAG, VU_SQL_ERROR, sqlite3_errmsg(db));
    wdb_finalize(stmt);
    sqlite3_close_v2(db);
    return OS_INVALID;
}

int wm_vuldet_remove_target_table(sqlite3 *db, char *TABLE, const char *target) {
    sqlite3_stmt *stmt = NULL;
    char sql[MAX_QUERY_SIZE];
    size_t size;

    if (size = snprintf(sql, MAX_QUERY_SIZE, vu_queries[VU_REMOVE_OS], TABLE), sql[size - 1] != ';') {
        sqlite3_close_v2(db);
        return OS_INVALID;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return wm_vuldet_sql_error(db, stmt);
    }

    sqlite3_bind_text(stmt, 1, target, -1, NULL);

    if (wm_vuldet_step(stmt) != SQLITE_DONE) {
        return wm_vuldet_sql_error(db, stmt);
    }
    sqlite3_finalize(stmt);

    return 0;
}

int wm_vuldet_update_feed(update_node *upd) {
    int result;
    int pages_fail = 0;
    char feed_updated = 0;

    if (!upd->multi_path && (upd->dist_ref == FEED_NVD || upd->dist_ref == FEED_REDHAT)) {
        int start;
        int end;

        if (!upd->multi_url) { // Online update for Redhat and NVD providers
            time_t n_date;
            struct tm *t_date;

            n_date = time(NULL);
            t_date = gmtime(&n_date);

            // Set the starting and ending point for each provider
            start = upd->dist_ref == FEED_NVD ? upd->update_from_year : 1;
            end = upd->dist_ref == FEED_NVD ? t_date->tm_year + 1900 : RED_HAT_REPO_MAX_REQ_ITS;
        } else { // Offline update from a custom repo for Redhat and NVD providers
            start = upd->multi_url_start;
            end = upd->multi_url_end;
        }

        // If it is a Red Hat update, we need to clean the database before we start
        if (upd->dist_ref == FEED_REDHAT) {
            wm_vuldet_clean_rh(NULL);
        }

        for (upd->update_it = start; upd->update_it <= end; (upd->update_it)++) {
            mtdebug1(WM_VULNDETECTOR_LOGTAG, upd->dist_ref == FEED_NVD ? VU_UPDATING_NVD_YEAR : VU_UPDATING_RH_YEAR, upd->update_it);
            if (result = wm_vuldet_sync_feed(upd), result) {
                // Feed synchronization failed
                if (upd->dist_ref == FEED_NVD) {
                    wm_vuldet_clean_nvd_year(NULL, upd->update_it);
                } else if (upd->dist_ref == FEED_REDHAT) {
                    wm_vuldet_clean_rh(NULL);
                }
                return OS_INVALID;
            } else {
                if (upd->dist_ref == FEED_REDHAT) {
                    switch (upd->update_state) {
                        case VU_FINISH_FETCH: // The last page has been reached
                            return 0;
                        case VU_TRY_NEXT_PAGE: // The maximum number of possible attempts for a page has been exhausted, the following page will be attempted
                            if (pages_fail == RED_HAT_REPO_MAX_FAIL_ITS) { // The allowed number of failed pages has been exhausted. The feed will not be updated.
                                mterror(WM_VULNDETECTOR_LOGTAG, VU_RH_REQ_FAIL_MAX, RED_HAT_REPO_MAX_FAIL_ITS);
                                wm_vuldet_clean_rh(NULL);
                                return OS_INVALID;
                            }
                            pages_fail++;
                        break;
                        case VU_INV_FEED:
                            wm_vuldet_clean_rh(NULL);
                            return OS_INVALID;
                        default:
                            mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_DOWNLOAD_PAGE_SUC, upd->update_it);
                            feed_updated = 1;
                    }
                }
            }
        }

        if (upd->dist_ref == FEED_REDHAT && !feed_updated) {
            return OS_INVALID;
        }
    } else {
        // single-providers -> offline update (multi_path only)
        // multi-providers -> online and offline update
        return wm_vuldet_sync_feed(upd);
    }
    return 0;
}

int wm_vuldet_insert(wm_vuldet_db *parsed_oval, update_node *update) {
    sqlite3 *db;
    sqlite3_stmt *stmt = NULL;
    int result;
    const char *query;
    oval_metadata *met_it = &parsed_oval->metadata;
    vulnerability *vul_it = parsed_oval->vulnerabilities;
    rh_vulnerability *rvul_it = parsed_oval->rh_vulnerabilities;
    info_state *state_it = parsed_oval->info_states;
    info_obj *obj_it = parsed_oval->info_objs;
    info_test *test_it = parsed_oval->info_tests;
    info_cve *info_it = parsed_oval->info_cves;
    cpe_list *cpes_it = parsed_oval->nvd_cpes;
    nvd_vulnerability *nvd_it = parsed_oval->nvd_vulnerabilities;
    vu_cpe_dic *w_cpes_it = parsed_oval->w_cpes;
    vu_msu_entry *msu_it = parsed_oval->msu_entries;
    variables *vars_it = parsed_oval->vars;

    if (sqlite3_open_v2(CVE_DB, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        return wm_vuldet_sql_error(db, stmt);
    }

    sqlite3_exec(db, vu_queries[BEGIN_T], NULL, NULL, NULL);

    switch (update->dist_ref) {
        case FEED_UBUNTU:
        case FEED_DEBIAN:
            if (wm_vuldet_remove_target_table(db, CVE_TABLE, parsed_oval->OS)      ||
                wm_vuldet_remove_target_table(db, METADATA_TABLE, parsed_oval->OS) ||
                wm_vuldet_remove_target_table(db, CVE_INFO_TABLE, parsed_oval->OS) ||
                wm_vuldet_remove_target_table(db, VARIABLES_TABLE, parsed_oval->OS)) {
                return OS_INVALID;
            }
        break;
        case FEED_CPEW:
            if (wm_vuldet_clean_wcpe(db)) {
                return OS_INVALID;
            }
        break;
        case FEED_NVD: case FEED_MSU: case FEED_REDHAT:
        break;
        default:
            sqlite3_close_v2(db);
            return OS_INVALID;
    }

    mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_UPDATE_VU);

    if (cpes_it) {
        mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_INS_CPES_SEC);

        if (wm_vuldet_insert_cpe_db(db, cpes_it, 1)){
            mterror(WM_VULNDETECTOR_LOGTAG, VU_CPES_INSERT_ERROR);
            return OS_INVALID;
        }
        free(cpes_it);
    }

    if (w_cpes_it) {
        mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_INS_CPES_DIC);

        if (wm_vuldet_insert_cpe_dic(db, w_cpes_it)){
            mterror(WM_VULNDETECTOR_LOGTAG, VU_CPES_INSERT_ERROR);
            return OS_INVALID;
        }
        free(w_cpes_it);
    }

    if (msu_it) {
        mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_INS_MSU);

        if (wm_vuldet_insert_MSU(db, msu_it)){
            return OS_INVALID;
        }
    }

    if (nvd_it) {
        mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_INS_NVD_SEC);
        if (wm_vuldet_index_nvd(db, update, nvd_it)) {
            return OS_INVALID;
        }
        parsed_oval->nvd_vulnerabilities = NULL;
    }

    // Adds the vulnerabilities
    while (vul_it) {
        // If you do not have this field, it has been discarded by the preparser and the OS is not affected
        if (vul_it->state_id) {
            if (sqlite3_prepare_v2(db, vu_queries[VU_INSERT_CVE], -1, &stmt, NULL) != SQLITE_OK) {
                return wm_vuldet_sql_error(db, stmt);
            }

            sqlite3_bind_text(stmt, 1, vul_it->cve_id, -1, NULL);
            sqlite3_bind_text(stmt, 2, parsed_oval->OS, -1, NULL);
            sqlite3_bind_text(stmt, 3, NULL, -1, NULL);
            sqlite3_bind_text(stmt, 4, vul_it->package_name ? vul_it->package_name : vul_it->state_id, -1, NULL);
            sqlite3_bind_int(stmt, 5, vul_it->pending);
            sqlite3_bind_text(stmt, 6, vul_it->state_id, -1, NULL);
            sqlite3_bind_text(stmt, 7, NULL, -1, NULL);
            sqlite3_bind_int(stmt, 8, 0);

            if (result = wm_vuldet_step(stmt), result != SQLITE_DONE && result != SQLITE_CONSTRAINT) {
                return wm_vuldet_sql_error(db, stmt);
            }
            sqlite3_finalize(stmt);
        }

        vulnerability *vul_aux = vul_it;
        vul_it = vul_it->prev;
        free(vul_aux->cve_id);
        free(vul_aux->state_id);
        free(vul_aux->package_name);
        free(vul_aux);
    }

    if (rvul_it) {
        mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_INS_RH_SEC);
        if (wm_vuldet_index_redhat(db, update, rvul_it)) {
            return OS_INVALID;
        }
        parsed_oval->rh_vulnerabilities = NULL;
    }

    if (test_it) {
        mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_INS_TEST_SEC);
    }

    // Links vulnerabilities to their conditions
    while (test_it) {
        if (sqlite3_prepare_v2(db, vu_queries[VU_UPDATE_CVE], -1, &stmt, NULL) != SQLITE_OK) {
            return wm_vuldet_sql_error(db, stmt);
        }

        sqlite3_bind_text(stmt, 1, test_it->obj, -1, NULL);
        sqlite3_bind_text(stmt, 2, test_it->state, -1, NULL);
        sqlite3_bind_text(stmt, 3, test_it->id, -1, NULL);

        if (result = wm_vuldet_step(stmt), result != SQLITE_DONE && result != SQLITE_CONSTRAINT) {
            return wm_vuldet_sql_error(db, stmt);
        }
        sqlite3_finalize(stmt);

        info_test *test_aux = test_it;
        test_it = test_it->prev;
        free(test_aux->id);
        free(test_aux->state);
        free(test_aux->obj);
        free(test_aux);
    }

    if (state_it) {
        mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_UPDATE_VU_CO);

        sqlite3_exec(db, vu_queries[VU_REMOVE_UNUSED_VULS], NULL, NULL, NULL);
    }

    // Sets the OVAL operators and values
    while (state_it) {
        query = vu_queries[VU_UPDATE_CVE_VAL];
        if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
            return wm_vuldet_sql_error(db, stmt);
        }
        sqlite3_bind_text(stmt, 1, state_it->operation, -1, NULL);
        sqlite3_bind_text(stmt, 2, state_it->operation_value, -1, NULL);
        sqlite3_bind_text(stmt, 3, state_it->id, -1, NULL);
        if (result = wm_vuldet_step(stmt), result != SQLITE_DONE && result != SQLITE_CONSTRAINT) {
            return wm_vuldet_sql_error(db, stmt);
        }
        sqlite3_finalize(stmt);

        info_state *state_aux = state_it;
        state_it = state_it->prev;
        free(state_aux->id);
        free(state_aux->operation);
        free(state_aux->operation_value);
        free(state_aux->arch_value);
        free(state_aux);
    }

    if (obj_it) {
        mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_UPDATE_PACK_NAME);
    }

    // Sets the OVAL package name
    while (obj_it) {
        query = vu_queries[VU_UPDATE_CVE_PACK];
        if (obj_it->obj) {
            if (result = sqlite3_prepare_v2(db, query, -1, &stmt, NULL), result != SQLITE_OK && result != SQLITE_CONSTRAINT) {
                return wm_vuldet_sql_error(db, stmt);
            }
            sqlite3_bind_text(stmt, 1, obj_it->obj, -1, NULL);
            sqlite3_bind_int(stmt, 2, obj_it->need_vars);
            sqlite3_bind_text(stmt, 3, obj_it->id, -1, NULL);
            if (result = wm_vuldet_step(stmt), result != SQLITE_DONE) {
                return wm_vuldet_sql_error(db, stmt);
            }
            sqlite3_finalize(stmt);
        }

        info_obj *obj_aux = obj_it;
        obj_it = obj_it->prev;
        free(obj_aux->id);
        free(obj_aux->obj);
        free(obj_aux);
    }

    if (vars_it) {
        mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_INS_VARIABLES);
    }

    // Sets the OVAL variables
    while (vars_it) {
        query = vu_queries[VU_INSERT_VARIABLES];
        if (vars_it->id) {
            int j;

            for (j = 0; vars_it->values[j]; j++) {
                if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
                    return wm_vuldet_sql_error(db, stmt);
                }
                sqlite3_bind_text(stmt, 1, vars_it->id, -1, NULL);
                sqlite3_bind_text(stmt, 2, vars_it->values[j], -1, NULL);
                sqlite3_bind_text(stmt, 3, parsed_oval->OS, -1, NULL);
                if (result = wm_vuldet_step(stmt), result != SQLITE_DONE && result != SQLITE_CONSTRAINT) {
                    return wm_vuldet_sql_error(db, stmt);
                }
                sqlite3_finalize(stmt);
            }
        }

        variables *var_aux = vars_it;
        vars_it = vars_it->prev;
        free(var_aux->id);
        w_FreeArray(var_aux->values);
        free(var_aux->values);
        free(var_aux);
    }

    if (info_it) {
        mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_UPDATE_VU_INFO);
    }

    while (info_it) {
        if (sqlite3_prepare_v2(db, vu_queries[VU_INSERT_CVE_INFO], -1, &stmt, NULL) != SQLITE_OK) {
            return wm_vuldet_sql_error(db, stmt);
        }

        sqlite3_bind_text(stmt, 1, info_it->cveid, -1, NULL);
        sqlite3_bind_text(stmt, 2, info_it->title, -1, NULL);
        sqlite3_bind_text(stmt, 3, (info_it->severity) ? info_it->severity : vu_severities[VU_UNKNOWN], -1, NULL);
        sqlite3_bind_text(stmt, 4, info_it->published, -1, NULL);
        sqlite3_bind_text(stmt, 5, info_it->updated, -1, NULL);
        sqlite3_bind_text(stmt, 6, info_it->reference, -1, NULL);
        sqlite3_bind_text(stmt, 7, parsed_oval->OS, -1, NULL);
        sqlite3_bind_text(stmt, 8, info_it->description, -1, NULL);
        sqlite3_bind_text(stmt, 9, info_it->cvss, -1, NULL);
        sqlite3_bind_text(stmt, 10, info_it->cvss_vector, -1, NULL);
        sqlite3_bind_text(stmt, 11, info_it->cvss3, -1, NULL);
        sqlite3_bind_text(stmt, 12, info_it->bugzilla_reference, -1, NULL);
        sqlite3_bind_text(stmt, 13, info_it->cwe, -1, NULL);
        sqlite3_bind_text(stmt, 14, info_it->advisories, -1, NULL);

        if (result = wm_vuldet_step(stmt), result != SQLITE_DONE && result != SQLITE_CONSTRAINT) {
            return wm_vuldet_sql_error(db, stmt);
        }
        sqlite3_finalize(stmt);
        info_cve *info_aux = info_it;
        info_it = info_it->prev;
        free(info_aux->cveid);
        free(info_aux->title);
        free(info_aux->severity);
        free(info_aux->published);
        free(info_aux->updated);
        free(info_aux->reference);
        free(info_aux->description);
        free(info_aux->cvss);
        free(info_aux->cvss_vector);
        free(info_aux->cvss3);
        free(info_aux->bugzilla_reference);
        free(info_aux->advisories);
        free(info_aux->cwe);
        free(info_aux);
    }

    if (sqlite3_prepare_v2(db, vu_queries[VU_INSERT_METADATA], -1, &stmt, NULL) != SQLITE_OK) {
        return wm_vuldet_sql_error(db, stmt);
    }
    sqlite3_bind_text(stmt, 1, parsed_oval->OS, -1, NULL);
    sqlite3_bind_text(stmt, 2, met_it->product_name, -1, NULL);
    sqlite3_bind_text(stmt, 3, met_it->product_version, -1, NULL);
    sqlite3_bind_text(stmt, 4, met_it->schema_version, -1, NULL);
    sqlite3_bind_text(stmt, 5, met_it->timestamp, -1, NULL);
    if (result = wm_vuldet_step(stmt), result != SQLITE_DONE && result != SQLITE_CONSTRAINT) {
        return wm_vuldet_sql_error(db, stmt);
    }
    sqlite3_finalize(stmt);

    free(met_it->product_name);
    free(met_it->product_version);
    free(met_it->schema_version);
    free(met_it->timestamp);

    sqlite3_exec(db, vu_queries[END_T], NULL, NULL, NULL);
    sqlite3_close_v2(db);
    return 0;
}

int wm_vuldet_check_db() {
    if (wm_vuldet_create_file(CVE_DB, schema_vuln_detector_sql)) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_INVALID_DB_INT);
        pthread_exit(NULL);
    }
    return 0;
}

void wm_vuldet_add_rvulnerability(wm_vuldet_db *ctrl_block) {
    rh_vulnerability *new;
    os_calloc(1, sizeof(rh_vulnerability), new);

    if (ctrl_block->rh_vulnerabilities) {
        new->prev = ctrl_block->rh_vulnerabilities;
    }
    ctrl_block->rh_vulnerabilities = new;
}

void wm_vuldet_add_vulnerability_info(wm_vuldet_db *ctrl_block) {
    info_cve *new;
    os_calloc(1, sizeof(info_cve), new);

    if (ctrl_block->info_cves) {
        new->prev = ctrl_block->info_cves;
    }
    ctrl_block->info_cves = new;
}

char * wm_vuldet_oval_xml_preparser(char *path, vu_feed dist) {
    FILE *input, *output = NULL;
    char buffer[OS_MAXSTR + 1];
    parser_state state = V_OVALDEFINITIONS;
    char *found;
    char *tmp_file;
    static const char *exclude_tags[] = {
        // Debian
        "oval:org.debian.oval:tst:1\"",
        "oval:org.debian.oval:tst:2\""
    };

    os_strdup(VU_FIT_TEMP_FILE, tmp_file);

    if (input = fopen(path, "r" ), !input) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_OPEN_FILE_ERROR, path);
        free(tmp_file);
        tmp_file = NULL;
        goto free_mem;
    } else if (output = fopen(tmp_file, "w" ), !output) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_OPEN_FILE_ERROR, tmp_file);
        free(tmp_file);
        tmp_file = NULL;
        goto free_mem;
    }

    while (fgets(buffer, OS_MAXSTR, input)) {
        if (dist == FEED_UBUNTU) { //5.11.1
            switch (state) {
                case V_OBJECTS:
                    if (found = strstr(buffer, "</objects>"), found) {
                        state = V_OVALDEFINITIONS;
                    }
                break;
                case V_DEFINITIONS:
                    if ((found = strstr(buffer, "is not affected")) &&
                              (found = strstr(buffer, "negate")) &&
                        strstr(found, "true")) {
                        continue;
                    } else if (strstr(buffer, "a decision has been made to ignore it")) {
                        continue;
                    } else if (found = strstr(buffer, "</definitions>"), found) {
                        state = V_OVALDEFINITIONS;
                        //continue;
                    }
                break;
                default:
                    if (strstr(buffer, "<objects>")) {
                        state = V_OBJECTS;
                    } else if (strstr(buffer, "<definitions>")) {
                      state = V_DEFINITIONS;
                      //continue;
                  }
            }
        } else if (dist == FEED_DEBIAN) { //5.3
            switch (state) {
                case V_OVALDEFINITIONS:
                    if (found = strstr(buffer, "?>"), found) {
                        state = V_STATES;
                    }
                    continue;
                break;
                case V_OBJECTS:
                    if (found = strstr(buffer, "</objects>"), found) {
                        state = V_STATES;
                    }
                break;
                case V_DEFINITIONS:
                    if (strstr(buffer, exclude_tags[0]) ||
                        strstr(buffer, exclude_tags[1])) {
                        continue;
                    } else if (found = strstr(buffer, "</definitions>"), found) {
                        state = V_STATES;
                    }
                break;
                default:
                    if (strstr(buffer, "<objects>")) {
                        state = V_OBJECTS;
                    } else if (strstr(buffer, "<definitions>")) {
                      state = V_DEFINITIONS;
                    } else if (strstr(buffer, "<tests>")) {
                      state = V_TESTS;
                    }
            }
        } else {
            free(tmp_file);
            tmp_file = NULL;
            goto free_mem;
        }
        fwrite(buffer, 1, strlen(buffer), output);
    }

free_mem:
    if (input) {
        fclose(input);
    }
    if (output) {
        fclose(output);
    }
    return tmp_file;
}

char *wm_vuldet_extract_advisories(cJSON *advisories) {
    char *advisories_str = NULL;
    char *str_it;
    size_t size;

    if (advisories) {
        for (; advisories && advisories->valuestring; advisories = advisories->next) {
            if (!advisories_str) {
                if (w_strdup(advisories->valuestring, advisories_str)) {
                    return NULL;
                }
                size = strlen(advisories_str);
            } else {
                os_realloc(advisories_str, size + strlen(advisories->valuestring) + 2, advisories_str);
                str_it = size + advisories_str;
                size = snprintf(str_it, strlen(advisories->valuestring) + 2, ",%s", advisories->valuestring) + size;
            }
        }
    }

    return advisories_str;
}

void wm_vuldet_adapt_title(char *title, char *cve) {
    // Remove unnecessary line jumps and  spaces
    size_t size;
    int offset;
    char *title_ofs;

    for (size = strlen(title) - 1; size > 0 && title[size] == ' '; size -= 1) {
        title[size] = '\0';
    }

    if (title[size] == '\n') {
        title[size--] = '\0';
    }

    offset = title[0] == '\n' ? 1 : 0;
    if(size > 1 && !strncmp(title + offset, cve, strlen(cve))) {
        offset += strlen(cve) + 1;
    }
    os_strdup(title + offset, title_ofs);
    strncpy(title, title_ofs, strlen(title_ofs) + 1);
    free(title_ofs);
}

int wm_vuldet_oval_xml_parser(OS_XML *xml, XML_NODE node, wm_vuldet_db *parsed_oval, update_node *update, vu_logic condition) {
    int i, j;
    int retval = 0;
    int check = 0;
    vulnerability *vuln;
    XML_NODE chld_node = NULL;
    vu_feed dist = update->dist_ref;
    static const char *XML_OVAL_DEFINITIONS = "oval_definitions";
    static const char *XML_GENERATOR = "generator";
    static const char *XML_DEFINITIONS = "definitions";
    static const char *XML_DEFINITION = "definition";
    static const char *XML_OBJECTS = "objects";
    static const char *XML_VARIABLES = "variables";
    static const char *XML_CONST_VAR = "constant_variable";
    static const char *XML_VALUE = "value";
    static const char *XML_TITLE = "title";
    static const char *XML_CLASS = "class";
    static const char *XML_VULNERABILITY = "vulnerability"; //ub 15.11.1
    static const char *XML_METADATA = "metadata";
    static const char *XML_OVAL_DEF_METADATA = "oval-def:metadata";
    static const char *XML_CRITERIA = "criteria";
    static const char *XML_REFERENCE = "reference";
    static const char *XML_REF_ID = "ref_id";
    static const char *XML_REF_URL = "ref_url";
    static const char *XML_OPERATOR = "operator";
    static const char *XML_OR = "OR";
    static const char *XML_AND = "AND";
    static const char *XML_COMMENT = "comment";
    static const char *XML_CRITERION = "criterion";
    static const char *XML_TEST_REF = "test_ref";
    static const char *XML_TESTS = "tests";
    static const char *XML_DPKG_LINUX_INFO_TEST = "linux-def:dpkginfo_test";
    static const char *XML_DPKG_LINUX_INFO_OBJ = "linux-def:dpkginfo_object";
    static const char *XML_DPKG_LINUX_INFO_DEB_OBJ = "dpkginfo_object";
    static const char *XML_DPKG_INFO_TEST = "dpkginfo_test";
    static const char *XML_ID = "id";
    static const char *XML_LINUX_STATE = "linux-def:state";
    static const char *XML_LINUX_NAME = "linux-def:name";
    static const char *XML_VAR_REF = "var_ref";
    static const char *XML_VAR_CHECK = "var_check";
    static const char *XML_LINUX_DEB_NAME = "name";
    static const char *XML_LINUX_OBJ = "linux-def:object";
    static const char *XML_LINUX_DEB_OBJ = "object";
    static const char *XML_STATE = "state";
    static const char *XML_STATE_REF = "state_ref";
    static const char *XML_OBJECT_REF = "object_ref";
    static const char *XML_STATES = "states";
    static const char *XML_DPKG_LINUX_INFO_STATE = "linux-def:dpkginfo_state";
    static const char *XML_DPKG_INFO_STATE = "dpkginfo_state";
    static const char *XML_LINUX_DEF_EVR = "linux-def:evr";
    static const char *XML_EVR = "evr";
    static const char *XML_OPERATION = "operation";
    static const char *XML_DATATYPE = "datatype";
    static const char *XML_OVAL_PRODUCT_NAME = "oval:product_name";
    static const char *XML_OVAL_PRODUCT_VERSION = "oval:product_version";
    static const char *XML_OVAL_SCHEMA_VERSION = "oval:schema_version";
    static const char *XML_OVAL_TIMESTAMP = "oval:timestamp";
    static const char *XML_ADVIDSORY = "advisory";
    static const char *XML_SEVERITY = "severity";
    static const char *XML_PUBLIC_DATE = "public_date";
    static const char *XML_UPDATED = "updated";
    static const char *XML_DESCRIPTION = "description";
    static const char *XML_DATE = "date";
    static const char *XML_DATES = "dates";
    static const char *XML_OVAL_DEF_DATES = "oval-def:dates";
    static const char *XML_DEBIAN = "debian";
    static const char *XML_OVAL_REPOSITORY = "oval_repository";
    static const char *XML_OVAL_DEF_OV_REPO = "oval-def:oval_repository";

    for (i = 0; node[i]; i++) {
        chld_node = NULL;
        if (!node[i]->element) {
            mterror(WM_VULNDETECTOR_LOGTAG, XML_ELEMNULL);
            return OS_INVALID;
        }

        if ((dist == FEED_UBUNTU && !strcmp(node[i]->element, XML_DPKG_LINUX_INFO_STATE)) ||
            (dist == FEED_DEBIAN && !strcmp(node[i]->element, XML_DPKG_INFO_STATE))) {
            if (chld_node = OS_GetElementsbyNode(xml, node[i]), !chld_node) {
                goto invalid_elem;
            }
            for (j = 0; node[i]->attributes[j]; j++) {
                if (!strcmp(node[i]->attributes[j], XML_ID)) {
                    info_state *infos;
                    os_calloc(1, sizeof(info_state), infos);
                    os_strdup(node[i]->values[j], infos->id);
                    infos->operation = infos->operation_value = NULL;
                    infos->prev = parsed_oval->info_states;
                    parsed_oval->info_states = infos;
                    if (wm_vuldet_oval_xml_parser(xml, chld_node, parsed_oval, update, condition) == OS_INVALID) {
                        goto end;
                    }
                }
            }
        } if (!strcmp(node[i]->element, XML_CONST_VAR)) {
            if (chld_node = OS_GetElementsbyNode(xml, node[i]), !chld_node) {
                goto invalid_elem;
            }
            if (node[i]->attributes && node[i]->values) {
                variables *vars = NULL;

                for (j = 0; node[i]->attributes[j] && node[i]->values[j]; j++) {
                    if (!strcmp(node[i]->attributes[j], XML_ID)) {
                        os_calloc(1, sizeof(variables), vars);
                        os_strdup(node[i]->values[j], vars->id);
                        vars->prev = parsed_oval->vars;
                        parsed_oval->vars =vars;

                        for (j = 0; chld_node[j] && chld_node[j]->element &&
                                    !strcmp(chld_node[j]->element, XML_VALUE) &&
                                    chld_node[j]->content; j++) {
                            os_realloc(vars->values, (j + 2) * sizeof(char *), vars->values);
                            os_strdup(chld_node[j]->content, vars->values[j]);
                            vars->values[j + 1] = NULL;
                        }

                        vars->elements = j;
                        break;
                    }
                }
            }
        } else if ((dist == FEED_UBUNTU && !strcmp(node[i]->element, XML_DPKG_LINUX_INFO_TEST)) ||
                   (dist == FEED_DEBIAN && !strcmp(node[i]->element, XML_DPKG_INFO_TEST))) {
            if (chld_node = OS_GetElementsbyNode(xml, node[i]), !chld_node) {
                goto invalid_elem;
            }
            info_test *infot;
            os_calloc(1, sizeof(info_test), infot);
            infot->state = NULL;
            infot->prev = parsed_oval->info_tests;
            parsed_oval->info_tests = infot;

            for (j = 0; node[i]->attributes[j]; j++) {
                if (!strcmp(node[i]->attributes[j], XML_ID)) {
                    os_strdup(node[i]->values[j], parsed_oval->info_tests->id);
                }
            }
            if (wm_vuldet_oval_xml_parser(xml, chld_node, parsed_oval, update, VU_PACKG) == OS_INVALID) {
                goto end;
            }
        } else if ((dist == FEED_UBUNTU && !strcmp(node[i]->element, XML_DPKG_LINUX_INFO_OBJ)) ||
                   (dist == FEED_DEBIAN && !strcmp(node[i]->element, XML_DPKG_LINUX_INFO_DEB_OBJ))) {
            if (chld_node = OS_GetElementsbyNode(xml, node[i]), !chld_node) {
                goto invalid_elem;
            }

            for (j = 0; node[i]->attributes[j]; j++) {
                if (!strcmp(node[i]->attributes[j], XML_ID)) {
                    info_obj *info_o;
                    os_calloc(1, sizeof(info_obj), info_o);
                    os_strdup(node[i]->values[j], info_o->id);
                    info_o->prev = parsed_oval->info_objs;
                    parsed_oval->info_objs = info_o;
                    if (wm_vuldet_xml_parser(xml, chld_node, parsed_oval, update, VU_OBJ) == OS_INVALID) {
                        goto end;
                    }
                }
            }
            if (wm_vuldet_oval_xml_parser(xml, chld_node, parsed_oval, update, VU_PACKG) == OS_INVALID) {
                goto end;
            }
        } else if (((dist == FEED_UBUNTU && !strcmp(node[i]->element, XML_LINUX_NAME)) ||
                   (dist == FEED_DEBIAN && !strcmp(node[i]->element, XML_LINUX_DEB_NAME)))) {
            w_strdup(node[i]->content, parsed_oval->info_objs->obj);
        } else if (condition == VU_OBJ && ((dist == DIS_UBUNTU && !strcmp(node[i]->element, XML_LINUX_NAME)) ||
                   (dist == DIS_DEBIAN && !strcmp(node[i]->element, XML_LINUX_DEB_NAME)))) {
            if (node[i]->content && *node[i]->content) {
                w_strdup(node[i]->content, parsed_oval->info_objs->obj);
            } else {
                if (node[i]->attributes && node[i]->values) {
                    int j;
                    char *var_check = NULL;
                    char *var_ref = NULL;

                    for (j = 0; node[i]->attributes[j] && node[i]->values[j]; j++) {
                        if (!strcmp(node[i]->attributes[j], XML_VAR_REF)) {
                            if (!var_check) {
                                os_strdup(node[i]->values[j], var_ref);
                            }
                        } else if (!strcmp(node[i]->attributes[j], XML_VAR_CHECK)) {
                            if (!var_check) {
                                os_strdup(node[i]->values[j], var_check);
                            }
                        }
                    }

                    if (!var_check || !var_ref) {
                        mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_OVAL_OBJ_INV, "Parameters 'var_check' and 'var_ref' were expected");
                    } else {
                        if (!strcmp(var_check, "at least one")) {
                            parsed_oval->info_objs->need_vars = 1;
                            parsed_oval->info_objs->obj = var_ref;
                            var_ref = NULL;
                        } else {
                            char error_msg[OS_SIZE_128];
                            snprintf(error_msg, OS_SIZE_128, "Unexpected var_check: '%s'", var_check);
                            mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_OVAL_OBJ_INV, error_msg);
                        }
                    }
                    free(var_ref);
                    free(var_check);
                } else {
                    mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_OVAL_OBJ_INV, "Empty object");
                }
            }
        } else if ((dist == FEED_UBUNTU && !strcmp(node[i]->element, XML_LINUX_DEF_EVR)) ||
                   (dist == FEED_DEBIAN && !strcmp(node[i]->element, XML_EVR))) {
            if (node[i]->attributes) {
                for (j = 0; node[i]->attributes[j]; j++) {
                    if (!strcmp(node[i]->attributes[j], XML_OPERATION)) {
                        os_strdup(node[i]->values[j], parsed_oval->info_states->operation);
                        os_strdup(node[i]->content, parsed_oval->info_states->operation_value);
                    }
                }
                if (!parsed_oval->info_states->operation && !strcmp(*node[i]->attributes, XML_DATATYPE) && !strcmp(*node[i]->values, "version")) {
                    os_strdup(vu_package_comp[VU_COMP_EQ], parsed_oval->info_states->operation);
                    os_strdup(node[i]->content, parsed_oval->info_states->operation_value);
                }

            }
        } else if ((condition == VU_PACKG) &&
                   ((dist == FEED_UBUNTU && !strcmp(node[i]->element, XML_LINUX_STATE)) ||
                   (dist == FEED_DEBIAN && !strcmp(node[i]->element, XML_STATE)))) {
            for (j = 0; node[i]->attributes[j]; j++) {
                if (!strcmp(node[i]->attributes[j], XML_STATE_REF)) {
                    if (!parsed_oval->info_tests->state) {
                        os_strdup(node[i]->values[j], parsed_oval->info_tests->state);
                    }
                }
            }
        } else if ((condition == VU_PACKG) &&
                   ((dist == FEED_UBUNTU && !strcmp(node[i]->element, XML_LINUX_OBJ)) ||
                   (dist == FEED_DEBIAN && !strcmp(node[i]->element, XML_LINUX_DEB_OBJ)))) {
            for (j = 0; node[i]->attributes[j]; j++) {
                if (!strcmp(node[i]->attributes[j], XML_OBJECT_REF)) {
                    if (!parsed_oval->info_tests->obj) {
                        os_strdup(node[i]->values[j], parsed_oval->info_tests->obj);
                    }
                }
            }
        } else if (!strcmp(node[i]->element, XML_DEFINITION)) {
            if (chld_node = OS_GetElementsbyNode(xml, node[i]), !chld_node) {
                goto invalid_elem;
            }
            for (j = 0; node[i]->attributes[j]; j++) {
                if (!strcmp(node[i]->attributes[j], XML_CLASS)) {
                    if (!strcmp(node[i]->values[j], XML_VULNERABILITY)) {
                        vulnerability *vuln;
                        info_cve *cves;
                        os_calloc(1, sizeof(vulnerability), vuln);
                        os_calloc(1, sizeof(info_cve), cves);

                        vuln->cve_id = NULL;
                        vuln->state_id = NULL;
                        vuln->pending = 0;
                        vuln->package_name = NULL;
                        vuln->prev = parsed_oval->vulnerabilities;
                        cves->cveid = NULL;
                        cves->title = NULL;
                        cves->severity = NULL;
                        cves->published = NULL;
                        cves->updated = NULL;
                        cves->reference = NULL;
                        cves->flags = 0;
                        cves->prev = parsed_oval->info_cves;

                        parsed_oval->vulnerabilities = vuln;
                        parsed_oval->info_cves = cves;

                        if (wm_vuldet_oval_xml_parser(xml, chld_node, parsed_oval, update, condition) == OS_INVALID) {
                            retval = OS_INVALID;
                            goto end;
                        }
                    }
                }
            }
        } else if (!strcmp(node[i]->element, XML_REFERENCE)) {
            for (j = 0; node[i]->attributes[j]; j++) {
                if (!parsed_oval->info_cves->reference && !strcmp(node[i]->attributes[j], XML_REF_URL)) {
                    os_strdup(node[i]->values[j], parsed_oval->info_cves->reference);
                } else if (!strcmp(node[i]->attributes[j], XML_REF_ID)){
                    if (!parsed_oval->info_cves->cveid) {
                        os_strdup(node[i]->values[j], parsed_oval->info_cves->cveid);
                    }
                    if (!parsed_oval->vulnerabilities->cve_id) {
                        os_strdup(node[i]->values[j], parsed_oval->vulnerabilities->cve_id);
                    }
                }
            }
        } else if (!strcmp(node[i]->element, XML_TITLE)) {
                os_strdup(node[i]->content, parsed_oval->info_cves->title);
                // Debian Wheezy OVAL has its CVE of the title
                if (dist == FEED_DEBIAN && !strcmp(parsed_oval->OS, vu_feed_tag[FEED_WHEEZY])) {
                    if (!parsed_oval->info_cves->cveid) {
                        os_strdup(node[i]->content, parsed_oval->info_cves->cveid);
                    }
                    if (!parsed_oval->vulnerabilities->cve_id) {
                        os_strdup(node[i]->content, parsed_oval->vulnerabilities->cve_id);
                    }
                }
        } else if (!strcmp(node[i]->element, XML_CRITERIA)) {
            if (!node[i]->attributes) {
                if (chld_node = OS_GetElementsbyNode(xml, node[i]), !chld_node) {
                    goto invalid_elem;
                }
                if (wm_vuldet_oval_xml_parser(xml, chld_node, parsed_oval, update, condition) == OS_INVALID) {
                    retval = OS_INVALID;
                    goto end;
                }
            } else {
                char operator_found = 0;
                for (j = 0; node[i]->attributes[j]; j++) {
                    if (!strcmp(node[i]->attributes[j], XML_OPERATOR)) {
                        int result = VU_TRUE;
                        operator_found = 1;
                        if (!strcmp(node[i]->values[j], XML_OR)) {
                            if (chld_node = OS_GetElementsbyNode(xml, node[i]), !chld_node) {
                                continue;
                            } else if (result = wm_vuldet_oval_xml_parser(xml, chld_node, parsed_oval, update, VU_OR), result == OS_INVALID) {
                                retval = OS_INVALID;
                                goto end;
                            }
                            if (result == VU_TRUE) {
                                retval = VU_TRUE;
                                check = 1;
                            }

                        } else if (!strcmp(node[i]->values[j], XML_AND)) {
                            if (chld_node = OS_GetElementsbyNode(xml, node[i]), !chld_node) {
                                continue;
                            } else if (result = wm_vuldet_oval_xml_parser(xml, chld_node, parsed_oval, update, VU_AND), result == OS_INVALID) {
                                retval = OS_INVALID;
                                goto end;
                            }
                        } else {
                            mterror(WM_VULNDETECTOR_LOGTAG, VU_INVALID_OPERATOR, node[i]->values[j]);
                            retval = OS_INVALID;
                            goto end;
                        }

                        if (result == VU_FALSE) {
                            if (condition == VU_AND) {
                                retval = VU_FALSE;
                                goto end;
                            } else if (condition == VU_OR && !check) {
                                retval = VU_FALSE;
                            }
                        }
                    }
                }
                // Checks for version comparasions without operators
                if (!operator_found && node[i]->attributes && node[i]->values &&
                    *node[i]->attributes && *node[i]->values &&
                    !strcmp(*node[i]->attributes, XML_COMMENT) &&
                    !strcmp(*node[i]->values, "file version")) {
                    if (chld_node = OS_GetElementsbyNode(xml, node[i]), !chld_node) {
                        continue;
                    } else if (wm_vuldet_oval_xml_parser(xml, chld_node, parsed_oval, update, VU_AND) == OS_INVALID) {
                        retval = OS_INVALID;
                        goto end;
                    }
                }
            }
        } else if (!strcmp(node[i]->element, XML_CRITERION)) {
            for (j = 0; node[i]->attributes[j]; j++) {
                if (!strcmp(node[i]->attributes[j], XML_TEST_REF)) {
                    static const char pending_state[] = "tst:10";

                    if (parsed_oval->vulnerabilities->state_id) {
                        os_calloc(1, sizeof(vulnerability), vuln);
                        os_strdup(parsed_oval->vulnerabilities->cve_id, vuln->cve_id);
                        vuln->prev = parsed_oval->vulnerabilities;
                        vuln->state_id = NULL;
                        vuln->package_name = NULL;
                        parsed_oval->vulnerabilities = vuln;

                        if (wstr_end(node[i]->values[j], pending_state)) {
                            vuln->pending = 1;
                        } else {
                            vuln->pending = 0;
                        }
                        os_strdup(node[i]->values[j], vuln->state_id);
                    } else {
                        if (wstr_end(node[i]->values[j], pending_state)) {
                            parsed_oval->vulnerabilities->pending = 1;
                        } else {
                            parsed_oval->vulnerabilities->pending = 0;
                        }
                        os_strdup(node[i]->values[j], parsed_oval->vulnerabilities->state_id);
                    }
                }
            }
        } else if (!strcmp(node[i]->element, XML_DESCRIPTION)) {
            os_strdup(node[i]->content, parsed_oval->info_cves->description);
        } else if (!strcmp(node[i]->element, XML_OVAL_PRODUCT_VERSION)) {
            os_strdup(node[i]->content, parsed_oval->metadata.product_version);
        } else if (!strcmp(node[i]->element, XML_OVAL_PRODUCT_NAME)) {
            os_strdup(node[i]->content, parsed_oval->metadata.product_name);
        } else if (!strcmp(node[i]->element, XML_DATE)) {
            os_strdup(node[i]->content, parsed_oval->info_cves->published);
        } else if (!strcmp(node[i]->element, XML_OVAL_TIMESTAMP)) {
            os_strdup(node[i]->content, parsed_oval->metadata.timestamp);
        } else if (!strcmp(node[i]->element, XML_OVAL_SCHEMA_VERSION)) {
            os_strdup(node[i]->content, parsed_oval->metadata.schema_version);
        } else if (!strcmp(node[i]->element, XML_SEVERITY)) {
            if (*node[i]->content != '\0') {
                os_strdup(node[i]->content, parsed_oval->info_cves->severity);
            } else {
                parsed_oval->info_cves->severity = NULL;
            }
        } else if (!strcmp(node[i]->element, XML_UPDATED)) {
            if (node[i]->attributes) {
                for (j = 0; node[i]->attributes[j]; j++) {
                    if (!strcmp(node[i]->attributes[j], XML_DATE)) {
                        os_strdup(node[i]->values[j], parsed_oval->info_cves->updated);
                    }
                }
            }
        } else if (dist == FEED_UBUNTU && !strcmp(node[i]->element, XML_PUBLIC_DATE)) {
            os_strdup(node[i]->content, parsed_oval->info_cves->published);
        } else if (!strcmp(node[i]->element, XML_OVAL_DEFINITIONS)  ||
                   !strcmp(node[i]->element, XML_DEFINITIONS)       ||
                   !strcmp(node[i]->element, XML_OBJECTS)           ||
                   !strcmp(node[i]->element, XML_VARIABLES)         ||
                   !strcmp(node[i]->element, XML_METADATA)          ||
                   !strcmp(node[i]->element, XML_OVAL_DEF_METADATA) ||
                   !strcmp(node[i]->element, XML_TESTS)             ||
                   !strcmp(node[i]->element, XML_STATES)            ||
                   !strcmp(node[i]->element, XML_ADVIDSORY)         ||
                   !strcmp(node[i]->element, XML_DEBIAN)            ||
                   !strcmp(node[i]->element, XML_GENERATOR)         ||
                   !strcmp(node[i]->element, XML_OVAL_REPOSITORY)   ||
                   !strcmp(node[i]->element, XML_OVAL_DEF_OV_REPO)  ||
                   !strcmp(node[i]->element, XML_DATES)             ||
                   !strcmp(node[i]->element, XML_OVAL_DEF_DATES)) {
            if (chld_node = OS_GetElementsbyNode(xml, node[i]), !chld_node) {
                goto invalid_elem;
            } else if (wm_vuldet_oval_xml_parser(xml, chld_node, parsed_oval, update, condition) == OS_INVALID) {
                retval = OS_INVALID;
                goto end;
            }
        }

        OS_ClearNode(chld_node);
        chld_node = NULL;
    }


end:
    OS_ClearNode(chld_node);
    return retval;

invalid_elem:
    mterror(WM_VULNDETECTOR_LOGTAG, XML_INVELEM, node[i]->element);
    return OS_INVALID;
}

int wm_vuldet_oval_process(update_node *update, char *path, wm_vuldet_db *parsed_vulnerabilities) {
    int success = 0;
    char *tmp_file;
    OS_XML xml;
    XML_NODE node = NULL;
    XML_NODE chld_node = NULL;

    memset(&xml, 0, sizeof(xml));

    mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_UPDATE_PRE);
    if (tmp_file = wm_vuldet_oval_xml_preparser(path, update->dist_ref), !tmp_file) {
        goto free_mem;
    }

    mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_UPDATE_PAR);
    if (OS_ReadXML(tmp_file, &xml) < 0) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_LOAD_CVE_ERROR, vu_feed_tag[update->dist_tag_ref], xml.err);
        goto free_mem;
    }

    if (node = OS_GetElementsbyNode(&xml, NULL), !node) {
        goto free_mem;
    };

    // Reduces a level of recurrence
    if (chld_node = OS_GetElementsbyNode(&xml, *node), !chld_node) {
        goto free_mem;
    }

    if (wm_vuldet_oval_xml_parser(&xml, chld_node, parsed_vulnerabilities, update, 0) == OS_INVALID) {
        goto free_mem;
    }

    success = 1;
free_mem:
    OS_ClearNode(node);
    OS_ClearNode(chld_node);
    OS_ClearXML(&xml);
    return !success;
}

int wm_vuldet_index_feed(update_node *update) {
    char *tmp_file = NULL;
    wm_vuldet_db parsed_vulnerabilities;
    const char *OS_VERSION;
    char *path;
    char success = 0;

    memset(&parsed_vulnerabilities, 0, sizeof(wm_vuldet_db));
    OS_VERSION = vu_feed_tag[update->dist_tag_ref];
    parsed_vulnerabilities.OS = OS_VERSION;

    if (update->json_format) {
        int result;
        // It is a feed in JSON format
        if (result = wm_vuldet_index_json(&parsed_vulnerabilities, update,
                                update->multi_path ? update->multi_path : VU_FIT_TEMP_FILE,
                                update->multi_path ? 1 : 0), result == OS_INVALID) {
            goto free_mem;
        } else if (result == VU_NOT_NEED_UPDATE) {
            success = 1;
            goto free_mem;
        }
    } else {
        path = update->path ? update->path : VU_TEMP_FILE;

        if (update->dist_ref == FEED_UBUNTU || update->dist_ref == FEED_DEBIAN) {
            if (wm_vuldet_oval_process(update, path, &parsed_vulnerabilities)) {
                goto free_mem;
            }
        } else if (update->dist_ref == FEED_CPED) {
            if (wm_vuldet_nvd_cpe_parser(path, &parsed_vulnerabilities)) {
                goto free_mem;
            }
        }
    }

    if (wm_vuldet_check_db()) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_CHECK_DB_ERROR);
        goto free_mem;
    }

    mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_START_REFRESH_DB, update->dist_ext);

    if (wm_vuldet_insert(&parsed_vulnerabilities, update)) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_REFRESH_DB_ERROR, OS_VERSION);
        goto free_mem;
    }
    mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_STOP_REFRESH_DB, update->dist_ext);

    success = 1;
free_mem:
    if (tmp_file) {
        free(tmp_file);
    }
    if (remove(VU_TEMP_FILE) < 0) {
        mtdebug2(WM_VULNDETECTOR_LOGTAG, "remove(%s): %s", VU_TEMP_FILE, strerror(errno));
    }
    if (remove(VU_FIT_TEMP_FILE) < 0) {
        mtdebug2(WM_VULNDETECTOR_LOGTAG, "remove(%s): %s", VU_FIT_TEMP_FILE, strerror(errno));
    }

    if (success) {
        return 0;
    }

    return OS_INVALID;
}

int wm_vuldet_fetch_oval(update_node *update, char *repo) {
    static const char *timestamp_tag = "timestamp>";
    char timestamp[OS_SIZE_256 + 1];
    char buffer[OS_MAXSTR + 1];
    FILE *fp = NULL;
    char *found;
    int attempts;
    vu_logic retval = VU_INV_FEED;

    for (attempts = 0;; attempts++) {
        if (!wurl_request(repo, VU_TEMP_FILE, NULL, NULL)) {
            break;
        } else if (attempts == WM_VULNDETECTOR_DOWN_ATTEMPTS) {
            goto end;
        }
        mdebug1(VU_DOWNLOAD_FAIL, attempts);
        sleep(attempts);
    }

    if (fp = fopen(VU_TEMP_FILE, "r"), !fp) {
        goto end;
    }

    while (fgets(buffer, OS_MAXSTR, fp)) {
        if (found = strstr(buffer, timestamp_tag), found) {
            char *close_tag;
            found+=strlen(timestamp_tag);

            if (close_tag = strstr(found, "<"), !close_tag) {
                goto end;
            }
            *close_tag = '\0';

            switch (wm_vuldet_check_timestamp(vu_feed_tag[update->dist_tag_ref], found, timestamp)) {
                case VU_TIMESTAMP_FAIL:
                    mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_DB_TIMESTAMP_FEED_ERROR, update->dist_ext);
                    goto end;
                break;
                case VU_TIMESTAMP_UPDATED:
                    mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_UPDATE_DATE, update->dist_ext, timestamp);
                    retval = VU_NOT_NEED_UPDATE;
                    goto end;
                break;
                case VU_TIMESTAMP_OUTDATED:
                    mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_DB_TIMESTAMP_FEED, update->dist_ext, "");
                break;
            }
            break;
        }
    }

    retval = VU_NEED_UPDATE;
end:
    if (fp) {
        w_fclose(fp);
    }

    return retval;
}

int wm_vuldet_fetch_redhat(update_node *update) {
    int attempt = 0;
    int retval = VU_TRY_NEXT_PAGE;
    FILE *fp = NULL;
    char *repo = NULL;
    char buffer[OS_SIZE_128 + 1];

    if (update->multi_url) {
        char tag[10 + 1];

        snprintf(tag, 10, "%d", update->update_it);
        repo = wstr_replace(update->multi_url, MULTI_URL_TAG, tag);
    } else {
        os_calloc(OS_SIZE_2048 + 1, sizeof(char), repo);
        snprintf(repo, OS_SIZE_2048, RED_HAT_REPO, update->update_from_year, RED_HAT_REPO_REQ_SIZE, update->update_it);
    }

    mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_DOWNLOAD_START, repo);
    while (1) {
        if (wurl_request(repo, VU_FIT_TEMP_FILE, NULL, NULL)) {
            if (attempt == RED_HAT_REPO_MAX_ATTEMPTS) {
                mtwarn(WM_VULNDETECTOR_LOGTAG, VU_API_REQ_INV_NEW, repo, RED_HAT_REPO_MAX_ATTEMPTS);
                update->update_state = VU_TRY_NEXT_PAGE;
                retval = 0;
                goto end;
            }

            attempt++;
            mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_API_REQ_INV, repo, attempt * DOWNLOAD_SLEEP_FACTOR);
            sleep(attempt * DOWNLOAD_SLEEP_FACTOR);
        } else {
            break;
        }
    }

    if (fp = fopen(VU_FIT_TEMP_FILE, "r"), !fp) {
        mtdebug2(WM_VULNDETECTOR_LOGTAG, "fopen(%s): %s", VU_FIT_TEMP_FILE, strerror(errno));
        retval = OS_INVALID;
        update->update_state = VU_INV_FEED;
        goto end;
    }

    if (!fgets(buffer, OS_SIZE_128, fp) || !strncmp(buffer, "[]", 2)) {
        if (remove(VU_FIT_TEMP_FILE) < 0) {
            mtdebug2(WM_VULNDETECTOR_LOGTAG, "remove(%s): %s", VU_FIT_TEMP_FILE, strerror(errno));
        }
        update->update_state = VU_FINISH_FETCH;
    } else {
        update->update_state = VU_SUCC_DOWN_PAGE;
    }

    retval = 0;
end:
    free(repo);
    if (fp) {
        fclose(fp);
    }
    return retval;
}

const char *wm_vuldet_decode_package_version(char *raw, const char **OS, char **OS_minor, char **package_name, char **package_version) {
    static OSRegex *reg = NULL;
    static char *package_regex = "(-\\d+:)|(-\\d+.)|(-\\d+\\w+)";
    const char *retv = NULL;
    char *found;

    if (!reg) {
        os_calloc(1, sizeof(OSRegex), reg);
        if(OSRegex_Compile(package_regex, reg, OS_RETURN_SUBSTRING) == 0) {
            return NULL;
        }
    }

    if (retv = OSRegex_Execute(raw, reg), retv) {
        if (found = strstr(raw, *reg->d_sub_strings), !found) {
            return NULL;
        }
        *found = '\0';
        w_strdup(found + 1, *package_version);
        w_strdup(raw, *package_name);

        if (found = strstr(*package_version, vu_package_dist[VU_RH_EXT_7]), found) {
            *OS = vu_feed_tag[FEED_RHEL7];
        } else if (found = strstr(*package_version, vu_package_dist[VU_RH_EXT_6]), found) {
            *OS = vu_feed_tag[FEED_RHEL6];
        } else if (found = strstr(*package_version, vu_package_dist[VU_RH_EXT_5]), found) {
            *OS = vu_feed_tag[FEED_RHEL5];
        }

        wm_vuldet_set_subversion(found, OS_minor);
    }

    return retv;
}

int wm_vuldet_check_timestamp(const char *target, char *timst, char *ret_timst) {
    int retval = VU_TIMESTAMP_FAIL;
    const char *stored_timestamp;
    sqlite3_stmt *stmt = NULL;
    sqlite3 *db = NULL;

    if (sqlite3_open_v2(CVE_DB, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
        goto end;
    } else {
        if (sqlite3_prepare_v2(db, vu_queries[TIMESTAMP_QUERY], -1, &stmt, NULL) != SQLITE_OK) {
            goto end;
        }
        sqlite3_bind_text(stmt, 1, target, -1, NULL);
        if (wm_vuldet_step(stmt) == SQLITE_ROW) {
            if (stored_timestamp = (const char *) sqlite3_column_text(stmt, 0), stored_timestamp) {
                if (!strcmp(stored_timestamp, timst)) {
                    retval = VU_TIMESTAMP_UPDATED;
                    if (ret_timst) {
                        snprintf(ret_timst, OS_SIZE_256, "%s", stored_timestamp);
                    }
                    goto end;
                }
            }
        }
        retval = VU_TIMESTAMP_OUTDATED;
    }
end:
    if (db) {
        sqlite3_close_v2(db);
    }
    wdb_finalize(stmt);
    return retval;
}

int wm_vuldet_fetch_feed(update_node *update, int *need_update) {
    char repo[OS_SIZE_2048 + 1] = { '\0' };
    int i;
    char *low_repo;
    unsigned char success = 0;
    int result;
    *need_update = 1;

    if (!update->url && (update->path || update->multi_path)) {
        mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_LOCAL_FETCH, update->path ? update->path : update->multi_path);
        return 0;
    }

    if (!update->url) {
        // Ubuntu and debian build their repos in a specific way
        if (update->dist_ref == FEED_UBUNTU) {
            os_strdup(update->version, low_repo);
            for(i = 0; low_repo[i] != '\0'; i++) {
                low_repo[i] = tolower(low_repo[i]);
            }
            snprintf(repo, OS_SIZE_2048, CANONICAL_REPO, low_repo);
            free(low_repo);
        } else if (update->dist_ref == FEED_DEBIAN) {
            os_strdup(update->version, low_repo);
            for(i = 0; low_repo[i] != '\0'; i++) {
                low_repo[i] = tolower(low_repo[i]);
            }
            snprintf(repo, OS_SIZE_2048, DEBIAN_REPO, low_repo);
            free(low_repo);
        } else if (update->dist_ref != FEED_REDHAT &&
                   update->dist_ref != FEED_CPED &&
                   update->dist_ref != FEED_NVD &&
                   update->dist_ref != FEED_CPEW &&
                   update->dist_ref != FEED_MSU) {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_OS_VERSION_ERROR);
            return OS_INVALID;
        }
    } else {
        snprintf(repo, OS_SIZE_2048, "%s", update->url);
    }

    // Call to the specific fetch function
    switch (update->dist_ref) {
        case FEED_REDHAT:
            update->update_state = 0;
            if (result = wm_vuldet_fetch_redhat(update), result != OS_INVALID) {
                success = 1;
                if (update->update_state == VU_FINISH_FETCH || update->update_state == VU_TRY_NEXT_PAGE || update->update_state == VU_INV_FEED) {
                    *need_update = 0;
                }
            }

            goto end;
        break;
        case FEED_CPED:
            if (wm_vuldet_fetch_nvd_cpe(repo)) {
                goto end;
            }
        break;
        case FEED_NVD:
            if (result = wm_vuldet_fetch_nvd_cve(update), result == VU_INV_FEED) {
                goto end;
            } else if (result == VU_NOT_NEED_UPDATE) {
                *need_update = 0;
            }
        break;
        case FEED_CPEW:
            if (result = wm_vuldet_fetch_wazuh_cpe(update), result == VU_INV_FEED) {
                goto end;
            } else if (result == VU_NOT_NEED_UPDATE) {
                *need_update = 0;
            }
        break;
        case FEED_MSU:
            if (result = wm_vuldet_fetch_MSU(), result == VU_INV_FEED) {
                goto end;
            } else if (result == VU_NOT_NEED_UPDATE) {
                *need_update = 0;
            }
        break;
        default:
            if (result = wm_vuldet_fetch_oval(update, repo), result == VU_INV_FEED) {
                goto end;
            } else if (result == VU_NOT_NEED_UPDATE) {
                *need_update = 0;
            }
    }

    success = 1;
end:
    if (success) {
        return 0;
    } else {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_FETCH_ERROR, update->dist_ext);
        return OS_INVALID;
    }
}

int wm_vuldet_run_update(update_node *upd, int *error_code) {
    if (wm_vuldet_check_update_period(upd)) {
        if (!wm_vuldet_silent_feed(upd->dist_tag_ref)) {
            mtinfo(WM_VULNDETECTOR_LOGTAG, VU_STARTING_UPDATE, vu_feed_ext[upd->dist_tag_ref]);
        } else {
            mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_STARTING_UPDATE, vu_feed_ext[upd->dist_tag_ref]);
        }
        if (wm_vuldet_update_feed(upd)) {
            if (!upd->attempted) {
                upd->last_update = time(NULL) - upd->interval + WM_VULNDETECTOR_RETRY_UPDATE;
                upd->attempted = 1;
                mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_UPDATE_RETRY, upd->dist, upd->version ? upd->version : "provider", (long unsigned)WM_VULNDETECTOR_RETRY_UPDATE);
            } else {
                upd->last_update = time(NULL);
                upd->attempted = 0;
                mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_UPDATE_RETRY, upd->dist, upd->version ? upd->version : "provider", upd->interval);
            }

            if (upd->dist_tag_ref == FEED_CPEW || upd->dist_tag_ref == FEED_MSU) {
                mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_NVD_UPD_CANCEL,
                        upd->dist_tag_ref == FEED_CPEW ? "Wazuh CPE Helper" : "MSU feed");
                *error_code = 1;
            }

            return OS_INVALID;
        } else {
            if (!wm_vuldet_silent_feed(upd->dist_tag_ref)) {
                mtinfo(WM_VULNDETECTOR_LOGTAG, VU_ENDING_UPDATE, upd->dist_ext);
            } else {
                mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_STARTING_UPDATE, vu_feed_ext[upd->dist_tag_ref]);
            }
            upd->last_update = time(NULL);
        }
    }
    return 0;
}


int wm_vuldet_updatedb(update_node **updates) {
    int error_code = 0;

    if (wm_vuldet_check_db()) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_CHECK_DB_ERROR);
        return OS_INVALID;
    }
        // Ubuntu
    if (wm_vuldet_run_update(updates[CVE_BIONIC], &error_code)   ||
        wm_vuldet_run_update(updates[CVE_XENIAL], &error_code)   ||
        wm_vuldet_run_update(updates[CVE_TRUSTY], &error_code)   ||
        wm_vuldet_run_update(updates[CVE_PRECISE], &error_code)  ||
        // Debian
        wm_vuldet_run_update(updates[CVE_STRETCH], &error_code)  ||
        wm_vuldet_run_update(updates[CVE_JESSIE], &error_code)   ||
        wm_vuldet_run_update(updates[CVE_WHEEZY], &error_code)   ||
        // RedHat
        wm_vuldet_run_update(updates[CVE_REDHAT], &error_code)   ||
        // Wazuh CPE feed
        wm_vuldet_run_update(updates[CPE_WDIC], &error_code)     ||
        //wm_vuldet_run_update(updates[CPE_NDIC], &error_code)   ||
        // Microsoft Security Update
        wm_vuldet_run_update(updates[CVE_MSU], &error_code)      ||
        // NVD
        wm_vuldet_run_update(updates[CVE_NVD], &error_code)) {

        if (error_code == 1) {
            // The CPE helper update failed
            wm_vuldet_free_update_node(updates[CPE_WDIC]);
            wm_vuldet_free_update_node(updates[CVE_MSU]);
            wm_vuldet_free_update_node(updates[CVE_NVD]);
            os_free(updates[CPE_WDIC]);
            os_free(updates[CVE_MSU]);
            os_free(updates[CVE_NVD]);
        }

        return OS_INVALID;
    }
    return 0;
}

int wm_vuldet_json_rh_parser(cJSON *json_feed, wm_vuldet_db *parsed_vulnerabilities) {
    cJSON *json_it;
    cJSON *cve_content;
    time_t l_time;
    struct tm *tm_time;
    // RH Security API - CVE LIST VALUES
    static char *JSON_CVE = "CVE";
    static char *JSON_SEVERITY = "severity";
    static char *JSON_PUBLIC_DATE = "public_date";
    static char *JSON_ADVISORIES = "advisories";
    static char *JSON_BUGZILLA = "bugzilla";
    static char *JSON_BUGZILLA_DESCRIPTION = "bugzilla_description";
    static char *JSON_CVSS_SCORE = "cvss_score";
    static char *JSON_CVSS_SCORING_VECTOR = "cvss_scoring_vector";
    static char *JSON_CWE = "CWE";
    static char *JSON_AFFECTED_PACKAGES = "affected_packages";
    static char *JSON_RESOURCE_URL = "resource_url";
    static char *JSON_CVSS3_SCORE = "cvss3_score";
    // Metadata values
    static char *rh_product_name = "Red Hat Security Data";
    static char *rh_product_version = "1.0";
    char *m_product_name = NULL;
    char *m_product_version = NULL;
    char *m_schema_version = NULL;
    char m_timestamp[27] = { '\0' };

    for (json_it  = json_feed->child; json_it; json_it = json_it->next) {
        char *tmp_cve = NULL;
        char *tmp_severity = NULL;
        char *tmp_public_date = NULL;
        cJSON *tmp_advisories = NULL;
        char *tmp_bugzilla = NULL;
        char *tmp_bugzilla_description = NULL;
        double tmp_cvss_score = -1;
        char *tmp_cvss_scoring_vector = NULL;
        char *tmp_cwe = NULL;
        cJSON *tmp_affected_packages = NULL;
        double tmp_cvss3_score = -1;

        time(&l_time);
        tm_time = localtime(&l_time);
        strftime(m_timestamp, 26, "%Y-%m-%d %H:%M:%S", tm_time);
        m_product_name = rh_product_name;
        m_product_version = rh_product_version;

        for (cve_content  = json_it->child; cve_content; cve_content = cve_content->next) {
            if (!strcmp(cve_content->string, JSON_CVE)) {
                tmp_cve = cve_content->valuestring;
            } else if (!strcmp(cve_content->string, JSON_SEVERITY)) {
                tmp_severity = cve_content->valuestring;
            } else if (!strcmp(cve_content->string, JSON_PUBLIC_DATE)) {
                tmp_public_date = cve_content->valuestring;
            } else if (!strcmp(cve_content->string, JSON_ADVISORIES)) {
                tmp_advisories = cve_content->child;
            } else if (!strcmp(cve_content->string, JSON_BUGZILLA)) {
                tmp_bugzilla = cve_content->valuestring;
            } else if (!strcmp(cve_content->string, JSON_BUGZILLA_DESCRIPTION)) {
                tmp_bugzilla_description = cve_content->valuestring;
            } else if (!strcmp(cve_content->string, JSON_CVSS_SCORE)) {
                if (cve_content->type != cJSON_NULL) {
                    tmp_cvss_score = cve_content->valuedouble;
                }
            } else if (!strcmp(cve_content->string, JSON_CVSS_SCORING_VECTOR)) {
                tmp_cvss_scoring_vector = cve_content->valuestring;
            } else if (!strcmp(cve_content->string, JSON_CWE)) {
                tmp_cwe = cve_content->valuestring;
            } else if (!strcmp(cve_content->string, JSON_AFFECTED_PACKAGES)) {
                tmp_affected_packages = cve_content->child;
            } else if (!strcmp(cve_content->string, JSON_RESOURCE_URL)) {
            } else if (!strcmp(cve_content->string, JSON_CVSS3_SCORE)) {
                if (cve_content->type != cJSON_NULL) {
                    tmp_cvss3_score = cve_content->valuedouble;
                }
            } else {
                mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_UNEXP_JSON_KEY, cve_content->string);
            }
        }

        if(!tmp_bugzilla_description || !tmp_cve) {
            mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_FEED_NODE_NULL_ELM);
            return OS_INVALID;
        }

        wm_vuldet_adapt_title(tmp_bugzilla_description, tmp_cve);

        if (tmp_affected_packages) {
            // Fill in vulnerability information
            wm_vuldet_add_vulnerability_info(parsed_vulnerabilities);
            if (w_strdup(tmp_cve, parsed_vulnerabilities->info_cves->cveid)) {
                continue;
            }
            w_strdup(tmp_severity, parsed_vulnerabilities->info_cves->severity);
            w_strdup(tmp_public_date, parsed_vulnerabilities->info_cves->published);
            parsed_vulnerabilities->info_cves->reference = wm_vuldet_build_url(VU_BUILD_REF_CVE_RH, tmp_cve);
            w_strdup(tmp_bugzilla_description, parsed_vulnerabilities->info_cves->description);
            parsed_vulnerabilities->info_cves->cvss = (tmp_cvss_score != -1) ? w_double_str(tmp_cvss_score) : NULL;
            parsed_vulnerabilities->info_cves->cvss3 = (tmp_cvss3_score != -1) ? w_double_str(tmp_cvss3_score) : NULL;
            w_strdup(tmp_cvss_scoring_vector, parsed_vulnerabilities->info_cves->cvss_vector);
            parsed_vulnerabilities->info_cves->bugzilla_reference = wm_vuldet_build_url(VU_BUILD_REF_BUGZ, tmp_bugzilla);
            parsed_vulnerabilities->info_cves->advisories = wm_vuldet_extract_advisories(tmp_advisories);
            w_strdup(tmp_cwe, parsed_vulnerabilities->info_cves->cwe);

            // Set the vulnerability - package relationship
            for (; tmp_affected_packages; tmp_affected_packages = tmp_affected_packages->next) {
            for (; tmp_affected_packages && tmp_affected_packages->valuestring; tmp_affected_packages = tmp_affected_packages->next) {
                wm_vuldet_add_rvulnerability(parsed_vulnerabilities);
                w_strdup(tmp_cve, parsed_vulnerabilities->rh_vulnerabilities->cve_id);
                if (!wm_vuldet_decode_package_version(tmp_affected_packages->valuestring,
                    &parsed_vulnerabilities->rh_vulnerabilities->OS,
                    &parsed_vulnerabilities->rh_vulnerabilities->OS_minor,
                    &parsed_vulnerabilities->rh_vulnerabilities->package_name,
                    &parsed_vulnerabilities->rh_vulnerabilities->package_version)) {
                    mterror(WM_VULNDETECTOR_LOGTAG, VU_VER_EXTRACT_ERROR, parsed_vulnerabilities->rh_vulnerabilities->package_name);
                    continue;
                } else if (!parsed_vulnerabilities->rh_vulnerabilities->OS) {
                    // The operating system of the package could not be specified. It will be added for all
                    char *pname = parsed_vulnerabilities->rh_vulnerabilities->package_name;
                    char *pversion = parsed_vulnerabilities->rh_vulnerabilities->package_version;
                    parsed_vulnerabilities->rh_vulnerabilities->OS = vu_feed_tag[FEED_RHEL7];

                    wm_vuldet_add_rvulnerability(parsed_vulnerabilities);
                    os_strdup(tmp_cve, parsed_vulnerabilities->rh_vulnerabilities->cve_id);
                    os_strdup(pname, parsed_vulnerabilities->rh_vulnerabilities->package_name);
                    os_strdup(pversion, parsed_vulnerabilities->rh_vulnerabilities->package_version);
                    parsed_vulnerabilities->rh_vulnerabilities->OS = vu_feed_tag[FEED_RHEL6];

                    wm_vuldet_add_rvulnerability(parsed_vulnerabilities);
                    os_strdup(tmp_cve, parsed_vulnerabilities->rh_vulnerabilities->cve_id);
                    os_strdup(pname, parsed_vulnerabilities->rh_vulnerabilities->package_name);
                    os_strdup(pversion, parsed_vulnerabilities->rh_vulnerabilities->package_version);
                    parsed_vulnerabilities->rh_vulnerabilities->OS = vu_feed_tag[FEED_RHEL5];
                }
            }
        }
    }

    // Insert metadata values
    os_free(parsed_vulnerabilities->metadata.product_name);
    w_strdup(m_product_name, parsed_vulnerabilities->metadata.product_name);
    os_free(parsed_vulnerabilities->metadata.product_version);
    w_strdup(m_product_version, parsed_vulnerabilities->metadata.product_version);
    os_free(parsed_vulnerabilities->metadata.schema_version);
    w_strdup(m_schema_version, parsed_vulnerabilities->metadata.schema_version);
    os_free(parsed_vulnerabilities->metadata.timestamp);
    os_strdup(m_timestamp, parsed_vulnerabilities->metadata.timestamp);

    return 0;
}

int wm_vuldet_json_parser(cJSON *json_feed, wm_vuldet_db *parsed_vulnerabilities, update_node *update) {
    switch ((int) update->dist_ref) {
        case FEED_REDHAT:
            return wm_vuldet_json_rh_parser(json_feed, parsed_vulnerabilities);
        break;
        case FEED_NVD:
            return wm_vuldet_json_nvd_parser(json_feed, parsed_vulnerabilities);
        break;
        case FEED_CPEW:
            return wm_vuldet_json_wcpe_parser(json_feed, parsed_vulnerabilities);
        break;
        case FEED_MSU:
            return wm_vuldet_json_msu_parser(json_feed, parsed_vulnerabilities);
        break;
    }

    return OS_INVALID;
}


int wm_vuldet_get_software_info(agent_software *agent, sqlite3 *db, OSHash *agents_triag, unsigned long ignore_time) {
    int sock = 0;
    unsigned int i;
    int size;
    char buffer[OS_SIZE_6144 + 1];
    char json_str[OS_SIZE_6144 + 1];
    char scan_id[OS_SIZE_1024];
    int request;
    char *found;
    int retval = OS_INVALID;
    cJSON *obj = NULL;
    cJSON *package_list = NULL;
    last_scan *scan;
    int min_cpe_index;
    cpe_list *node_list = NULL;

    mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_AGENT_SOFTWARE_REQ, agent->agent_id);

    for (i = 0; i < VU_MAX_WAZUH_DB_ATTEMPS && (sock = OS_ConnectUnixDomain(WDB_LOCAL_SOCK_PATH, SOCK_STREAM, OS_SIZE_6144)) < 0; i++) {
        mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_SOCKET_RETRY, WDB_LOCAL_SOCK_PATH, i);
        sleep(i);
    }

    if (i == VU_MAX_WAZUH_DB_ATTEMPS) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_SOCKET_RETRY_ERROR, WDB_LOCAL_SOCK_PATH);
        goto end;
    }

    // Request the ID of the last scan
    size = snprintf(buffer, OS_SIZE_6144, vu_queries[VU_SYSC_SCAN_REQUEST], agent->agent_id);
    if (OS_SendSecureTCP(sock, size + 1, buffer) || (size = OS_RecvSecureTCP(sock, buffer, OS_SIZE_6144)) < 1) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_SYSC_SCAN_REQUEST_ERROR, agent->agent_id);
        goto end;
    }

    buffer[size] = '\0';
    if (!strncmp(buffer, "ok", 2)) {
        buffer[0] = buffer[1] = ' ';
        // Check empty answers
        if ((found = strchr(buffer, '[')) && *(++found) != '\0' && *found == ']') {
            mtdebug1(WM_VULNDETECTOR_LOGTAG , VU_NO_SYSC_SCANS, agent->agent_id);
            retval = 2;
            goto end;
        }
        size = snprintf(json_str, OS_SIZE_6144, "{\"data\":%s}", buffer);
        json_str[size] = '\0';
    } else {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_INV_WAZUHDB_RES, buffer);
        retval = OS_INVALID;
        goto end;
    }

    if (obj = cJSON_Parse(json_str), obj && cJSON_IsObject(obj)) {
        cJSON_GetObjectItem(obj, "data");
    } else {
        retval = OS_INVALID;
        goto end;
    }

    size = snprintf(scan_id, OS_SIZE_1024, "%i", (int) cJSON_GetObjectItem(obj, "data")->child->child->valuedouble);
    scan_id[size] = '\0';

    cJSON_Delete(obj);
    obj = NULL;

    // Check to see if the scan has already been reported
    if (scan = OSHash_Get(agents_triag, agent->agent_id), scan) {
        if ((scan->last_scan_time + (time_t) ignore_time) < time(NULL)) {
            scan->last_scan_time = time(NULL);
        } else if (!strcmp(scan_id, scan->last_scan_id)) {
            // Nothing to do
            mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_SYS_CHECKED, agent->agent_id, scan_id);
            retval = 0;
            goto end;
        } else {
            free(scan->last_scan_id);
            os_strdup(scan_id, scan->last_scan_id);
        }
        request = VU_SOFTWARE_REQUEST;
    } else {
        if (agent->dist == FEED_WIN && wm_vuldet_clean_agent_cpes(sock, agent->agent_id)) {
            goto end;
        }

        os_calloc(1, sizeof(last_scan), scan);
        os_strdup(scan_id, scan->last_scan_id);
        scan->last_scan_time = time(NULL);
        OSHash_Add(agents_triag, agent->agent_id, scan);
        request = VU_SOFTWARE_FULL_REQ; // Check all at the first time
    }

    // Request and store packages
    i = 0;
    size = snprintf(buffer, OS_SIZE_6144, vu_queries[request], agent->agent_id, scan_id, VU_MAX_PACK_REQ, i);
    if (OS_SendSecureTCP(sock, size + 1, buffer)) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_SOFTWARE_REQUEST_ERROR, agent->agent_id);
        goto end;
    }

    while (size = OS_RecvSecureTCP(sock, buffer, OS_SIZE_6144), size) {
        if (size > 0) {
            if (size < 10) {
                break;
            }
            buffer[size] = '\0';
            if (!strncmp(buffer, "ok", 2)) {
                buffer[0] = buffer[1] = ' ';
                size = snprintf(json_str, OS_SIZE_6144, "{\"data\":%s}", buffer);
                json_str[size] = '\0';
            } else {
                goto end;
            }
            if (obj) {
                cJSON *new_obj;
                cJSON *data;
                if (new_obj = cJSON_Parse(json_str), !new_obj) {
                    goto end;
                } else if (!cJSON_IsObject(new_obj)) {
                    free(new_obj);
                    goto end;
                }
                data = cJSON_GetObjectItem(new_obj, "data");
                if (data) {
                    cJSON_AddItemToArray(package_list, data->child);
                    free(data->string);
                    free(data);
                }
                free(new_obj);
            } else if (obj = cJSON_Parse(json_str), obj && cJSON_IsObject(obj)) {
                package_list = cJSON_GetObjectItem(obj, "data");
                if (!package_list) {
                    goto end;
                }
            } else {
                goto end;
            }

            i += VU_MAX_PACK_REQ;
            size = snprintf(buffer, OS_SIZE_6144, vu_queries[request], agent->agent_id, scan_id, VU_MAX_PACK_REQ, i);
            if (OS_SendSecureTCP(sock, size + 1, buffer)) {
                mterror(WM_VULNDETECTOR_LOGTAG, VU_SOFTWARE_REQUEST_ERROR, agent->agent_id);
                goto end;
            }
        } else {
            goto end;
        }
    }

    // Avoid checking the same packages again
    size = snprintf(buffer, OS_SIZE_6144, vu_queries[VU_SYSC_UPDATE_SCAN], agent->agent_id, scan_id);
    if (OS_SendSecureTCP(sock, size + 1, buffer) || (size = OS_RecvSecureTCP(sock, buffer, OS_SIZE_6144)) < 1) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_SOFTWARE_REQUEST_ERROR, agent->agent_id);
        goto end;
    }

    if (agent->dist == FEED_WIN) {
        // Request the agent hotfixes
        if (wm_vuldet_request_hotfixes(db, sock, agent->agent_id)) {
            goto end;
        }

        // Get the os info
        if (wm_vuldet_get_oswindows_info(sock, agent)) {
            goto end;
        }

        // Generate the OS CPE
        if (!agent->os_release) {
            mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_OSINFO_DISABLED, agent->agent_id);
        } else {
            wm_vuldet_generate_os_cpe(agent);
        }
    }

    close(sock);
    sock = 0;

    if (wm_vuldet_get_min_cpe_index(db, &min_cpe_index)) {
        mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_CPE_INDEX_GET_ERROR);
        goto end;
    }

    if (package_list) {
        sqlite3_exec(db, vu_queries[BEGIN_T], NULL, NULL, NULL);
        if (agent->agent_os_cpe) {
            if (wm_vuldet_insert_agent_data(db, agent, &min_cpe_index, "",
                                        vu_feed_ext[agent->dist_ver], "", "",
                                        agent->agent_os_cpe, &node_list)) {
                mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_INSERT_DATA_ERR, agent->agent_os_cpe->raw);
                goto end;
            }
        }

        for (package_list = package_list->child; package_list; package_list = package_list->next) {
            char *name = (char *) wm_vuldet_get_json_value(package_list, "name");
            char *version = (char *) wm_vuldet_get_json_value(package_list, "version");
            char *architecture = (char *) wm_vuldet_get_json_value(package_list, "architecture");
            char *cpe_str = (char *) wm_vuldet_get_json_value(package_list, "cpe");
            char *vendor = (char *) wm_vuldet_get_json_value(package_list, "vendor");
            char *msu_name = (char *) wm_vuldet_get_json_value(package_list, "msu_name");

            if (name && version && architecture) {
                struct cpe *s_cpe = NULL;
                char success = 1;

                if (cpe_str) {
                    s_cpe = wm_vuldet_decode_cpe(cpe_str);
                    w_strdup(msu_name, s_cpe->msu_name);
                }

                if (wm_vuldet_insert_agent_data(db, agent, &min_cpe_index, vendor, name,
                                                version, architecture, s_cpe, &node_list)) {
                    mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_INSERT_DATA_ERR,
                            cpe_str ? cpe_str : name);
                    success = 0;
                }

                if (s_cpe) {
                    wm_vuldet_free_cpe(s_cpe);
                }

                if (!success) {
                    goto end;
                }
            }
        }
        sqlite3_exec(db, vu_queries[END_T], NULL, NULL, NULL);
        agent->info = 1;
    } else {
        mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_NO_SOFTWARE, agent->agent_id);
    }

    // Generate the agent's CPEs
    // At the moment, only for Windows agents
    if (agent->dist == FEED_WIN) {
        sqlite3_exec(db, vu_queries[BEGIN_T], NULL, NULL, NULL);
        // Add the cpes obtained from Wazuh-db
        if (node_list && wm_vuldet_insert_cpe_db(db, node_list, 0)) {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_WAZUH_DB_CPE_IN_ERROR, agent->agent_id);
        }

        // Attempts to generate the remaining CPEs
        if (wm_vuldet_generate_agent_cpes(db, agent, 1)) {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_AGENT_CPE_GEN_ERROR, agent->agent_id);
            goto end;
        }
        sqlite3_exec(db, vu_queries[END_T], NULL, NULL, NULL);
    }

    retval = 0;
end:
    free(node_list);
    if (obj) {
        cJSON_Delete(obj);
    }
    if (sock >= 0) {
        close(sock);
    }
    return retval;
}

void free_agents_triag(last_scan *data) {
    if (!data) return;
    if (data->last_scan_id) free(data->last_scan_id);
    free(data);
}

void * wm_vuldet_main(wm_vuldet_t * vuldet) {
    time_t time_sleep = 0;
    wm_vuldet_flags *flags = &vuldet->flags;
    update_node **updates = vuldet->updates;
    int i;

    if (!flags->enabled) {
        mtdebug1(WM_VULNDETECTOR_LOGTAG, "Module disabled. Exiting...");
        pthread_exit(NULL);
    }

    for (i = 0; vuldet->queue_fd = StartMQ(DEFAULTQPATH, WRITE), vuldet->queue_fd < 0 && i < WM_MAX_ATTEMPTS; i++) {
        sleep(WM_MAX_WAIT);
    }

    if (i == WM_MAX_ATTEMPTS) {
        mterror(WM_VULNDETECTOR_LOGTAG, "Can't connect to queue.");
        pthread_exit(NULL);
    }

    vu_queue = &vuldet->queue_fd;

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();


    for (i = 0; SSL_library_init() < 0 && i < WM_MAX_ATTEMPTS; i++) {
        sleep(WM_MAX_WAIT);
    }

    if (i == WM_MAX_ATTEMPTS) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_SSL_LIBRARY_ERROR);
        pthread_exit(NULL);
    }

    if (flags->run_on_start) {
        vuldet->last_detection = 0;
        for (i = 0; i < OS_SUPP_SIZE; i++) {
            if (updates[i]) {
                updates[i]->last_update = 0;
            }
        }
    } else {
        vuldet->last_detection = time(NULL);
        for (i = 0; i < OS_SUPP_SIZE; i++) {
            if (updates[i]) {
                updates[i]->last_update = time(NULL);
            }
        }
    }

    if (vuldet->agents_triag = OSHash_Create(), !vuldet->agents_triag) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_CREATE_HASH_ERRO, "agents");
        pthread_exit(NULL);
    }

    OSHash_SetFreeDataPointer(vuldet->agents_triag, (void (*)(void *))free_agents_triag);

    while (1) {
        // Update CVE databases
        if (flags->update &&
            wm_vuldet_updatedb(vuldet->updates)) {
                mterror(WM_VULNDETECTOR_LOGTAG, VU_OVAL_UPDATE_ERROR);
        }

        if ((vuldet->last_detection + (time_t) vuldet->detection_interval) < time(NULL)) {
            if (wm_vuldet_db_empty() == (int) VU_FALSE) {
                mtinfo(WM_VULNDETECTOR_LOGTAG, VU_START_SCAN);

                if (wm_vuldet_set_agents_info(&vuldet->agents_software, updates)) {
                    mterror(WM_VULNDETECTOR_LOGTAG, VU_NO_AGENT_ERROR);
                } else {
                    if (wm_vuldet_check_agent_vulnerabilities(vuldet->agents_software, &vuldet->flags, vuldet->agents_triag, vuldet->ignore_time)) {
                        mterror(WM_VULNDETECTOR_LOGTAG, VU_AG_CHECK_ERR);
                    } else {
                        mtinfo(WM_VULNDETECTOR_LOGTAG, VU_END_SCAN);
                    }
                }
                agent_software *agent;
                for (agent = vuldet->agents_software; agent;) {
                    agent_software *agent_aux = agent->next;

                    wm_vuldet_free_agent_software(agent);
                    if (agent_aux) {
                        agent = agent_aux;
                    } else {
                        break;
                    }
                }
                vuldet->agents_software = NULL;
            }

            vuldet->last_detection = time(NULL);
        }

        time_t t_now = time(NULL);
        time_sleep = (vuldet->last_detection + vuldet->detection_interval) - t_now;
        if (time_sleep <= 0) {
            time_sleep = 1;
            i = OS_SUPP_SIZE;
        } else {
            i = 0;
        }

        // Check the remaining time for all updates and adjust the sleep time
        for (; i < OS_SUPP_SIZE; i++) {
            if (updates[i]) {
                time_t t_diff = (updates[i]->last_update + updates[i]->interval) - t_now;
                // Stop checking if we have any pending updates
                if (t_diff <= 0) {
                    time_sleep = 1;
                    break;
                } else if (t_diff < time_sleep) {
                    time_sleep = t_diff;
                }
            }
        }

        mtdebug2(WM_VULNDETECTOR_LOGTAG, "Sleeping for %lu seconds...", time_sleep);
        sleep(time_sleep);
    }

}

int wm_vuldet_set_agents_info(agent_software **agents_software, update_node **updates) {
    agent_software *agents = NULL;
    agent_software *f_agent = NULL;
    char global_db[OS_FLSIZE + 1];
    sqlite3 *db;
    sqlite3_stmt *stmt = NULL;
    int dist_error;
    char *id;
    char *name;
    char *ip;
    char *register_ip;
    char *os_name;
    char *os_version;
    char *arch;
    int build;
    vu_feed agent_dist_ver;
    vu_feed agent_dist;

    snprintf(global_db, OS_FLSIZE, "%s%s/%s", isChroot() ? "/" : "", WDB_DIR, WDB_GLOB_NAME);

    if (sqlite3_open_v2(global_db, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_GLOBALDB_OPEN_ERROR);
        return wm_vuldet_sql_error(db, stmt);
    }

    sqlite3_busy_timeout(db, SQL_BUSY_SLEEP_MS);

    // Extracts the operating system of the agents
    if (wdb_prepare(db, vu_queries[VU_GLOBALDB_REQUEST], -1, &stmt, NULL)) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_GLOBALDB_ERROR);
        return wm_vuldet_sql_error(db, stmt);
    }

    sqlite3_bind_int(stmt, 1, DISCON_TIME);

    while (wm_vuldet_step(stmt) == SQLITE_ROW) {
        dist_error = -1;
        os_name = (char *) sqlite3_column_text(stmt, 0);
        os_version = (char *) sqlite3_column_text(stmt,1);
        name = (char *) sqlite3_column_text(stmt, 2);
        id = (char *) sqlite3_column_text(stmt, 3);
        build = sqlite3_column_int(stmt, 6);

        if (id == NULL) {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_NULL_AGENT_ID);
            continue;
        }

        if (!os_name) {
            if (name) {
                mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_AG_NEVER_CON, name);
            }
            continue;
        } else if (!os_version) {
            mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_AGENT_UNSOPPORTED, id, name);
            continue;
        }

        if (strcasestr(os_name, vu_feed_ext[FEED_UBUNTU])) {
            if (strstr(os_version, "18")) {
                agent_dist_ver = FEED_BIONIC;
            } else if (strstr(os_version, "16")) {
                agent_dist_ver = FEED_XENIAL;
            } else if (strstr(os_version, "14")) {
                agent_dist_ver = FEED_TRUSTY;
            } else if (strstr(os_version, "12")) {
                agent_dist_ver = FEED_PRECISE;
            } else {
                dist_error = FEED_UBUNTU;
            }
            agent_dist = FEED_UBUNTU;
        } else if (strcasestr(os_name, vu_feed_ext[FEED_DEBIAN])) {
            if (strstr(os_version, "7")) {
                agent_dist_ver = FEED_WHEEZY;
            } else if (strstr(os_version, "8")) {
                agent_dist_ver = FEED_JESSIE;
            } else if (strstr(os_version, "9")) {
                agent_dist_ver = FEED_STRETCH;
            } else {
                dist_error = FEED_DEBIAN;
            }
            agent_dist = FEED_DEBIAN;
        }  else if (strcasestr(os_name, vu_feed_ext[FEED_REDHAT])) {
            if (strstr(os_version, "7")) {
                agent_dist_ver = FEED_RHEL7;
            } else if (strstr(os_version, "6")) {
                agent_dist_ver = FEED_RHEL6;
            } else if (strstr(os_version, "5")) {
                agent_dist_ver = FEED_RHEL5;
            } else {
                dist_error = FEED_REDHAT;
            }
            agent_dist = FEED_REDHAT;
        } else if (strcasestr(os_name, vu_feed_ext[FEED_CENTOS])) {
            if (strstr(os_version, "7")) {
                agent_dist_ver = FEED_RHEL7;
            } else if (strstr(os_version, "6")) {
                agent_dist_ver = FEED_RHEL6;
            } else if (strstr(os_version, "5")) {
                agent_dist_ver = FEED_RHEL5;
            } else {
                dist_error = FEED_CENTOS;
            }
            agent_dist = FEED_REDHAT;
        } else if (strcasestr(os_name, vu_feed_ext[FEED_AMAZL])) {
            agent_dist_ver = FEED_RHEL7;
            agent_dist = FEED_REDHAT;
        } else if (strcasestr(os_name, vu_feed_ext[FEED_WIN])) {
            agent_dist_ver = wm_vuldet_decode_win_os(os_name);
            agent_dist = FEED_WIN;
        } else {
            // Operating system not supported in any of its versions
            dist_error = -2;
        }

        if (dist_error != -1) {
            // Check if the agent OS can be matched with a OVAL
            if (agent_dist_ver = wm_vuldet_set_allowed_feed(os_name, os_version, updates, &agent_dist), agent_dist_ver == FEED_UNKNOW) {
                if (dist_error == -2) {
                    mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_AGENT_UNSOPPORTED, id, name);
                    continue;
                } else {
                    mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_UNS_OS_VERSION, vu_feed_ext[dist_error], name);
                    continue;
                }
            }
        }

        ip = (char *) sqlite3_column_text(stmt, 4);
        register_ip = (char *) sqlite3_column_text(stmt, 5);

        if (!ip && !register_ip) {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_NULL_AGENT_IP, id);
            continue;
        }

        arch = (char *) sqlite3_column_text(stmt, 6);

        if (agents) {
            os_calloc(1, sizeof(agent_software), agents->next);
            agents->next->prev = agents;
            agents = agents->next;
        } else {
            os_calloc(1, sizeof(agent_software), agents);
            agents->prev = NULL;
            f_agent = agents;
        }

        agents->build = build;
        os_strdup(id, agents->agent_id);
        if (ip) {
            if (strcmp(ip, "127.0.0.1")) {
                os_strdup(ip, agents->agent_ip);
            } else {
                agents->agent_ip = NULL;
            }
        } else {
            os_strdup(register_ip, agents->agent_ip);
        }

        os_strdup(name, agents->agent_name);
        if (arch) {
            os_strdup(arch, agents->arch);
        }
        agents->dist = agent_dist;
        agents->dist_ver = agent_dist_ver;
        agents->info = 0;
        agents->next = NULL;
    }
    sqlite3_finalize(stmt);
    *agents_software = f_agent;
    sqlite3_close_v2(db);
    return 0;
}

void wm_vuldet_destroy(wm_vuldet_t * vuldet) {
    agent_software *agent;
    update_node **update;
    int i, j, k;

    if (vuldet->agents_triag) {
        OSHash_Free(vuldet->agents_triag);
    }

    for (i = 0, update = vuldet->updates; i < OS_SUPP_SIZE; i++) {
        if (update[i]) {
            free(update[i]->dist);
            free(update[i]->version);
            free(update[i]->url);
            free(update[i]->path);
            if (wm_vuldet_is_single_provider(update[i]->dist_ref)) {
                if (update[i]->allowed_os_name) {
                    for (j = 0; update[i]->allowed_os_name[j]; j++) {
                        free(update[i]->allowed_os_name[j]);
                        free(update[i]->allowed_os_ver[j]);
                    }
                    free(update[i]->allowed_os_name);
                    free(update[i]->allowed_os_ver);
                }
            } else {
                if (update[i]->allowed_multios_src_name) {
                    for (j = 0; update[i]->allowed_multios_src_name[j]; j++) {
                        for (k = 0; update[i]->allowed_multios_src_name[j][k]; k++) {
                            free(update[i]->allowed_multios_src_name[j][k]);
                            free(update[i]->allowed_multios_src_ver[j][k]);
                        }
                        free(update[i]->allowed_multios_src_name[j]);
                        free(update[i]->allowed_multios_src_ver[j]);
                    }
                    free(update[i]->allowed_multios_src_name);
                    free(update[i]->allowed_multios_src_ver);

                    for (j = 0; update[i]->allowed_os_name[j]; j++) {
                        free(update[i]->allowed_multios_dst_name[j]);
                        free(update[i]->allowed_multios_dst_ver[j]);
                    }
                    free(update[i]->allowed_multios_dst_name);
                    free(update[i]->allowed_multios_dst_ver);
                }
            }
            free(update[i]);
        }
    }

    for (agent = vuldet->agents_software; agent;) {
        agent_software *agent_aux = agent->next;
        wm_vuldet_free_agent_software(agent);
        if (agent_aux) {
            agent = agent_aux;
        } else {
            break;
        }
    }
    free(vuldet);
}


cJSON *wm_vuldet_dump(const wm_vuldet_t * vuldet){

    cJSON *root = cJSON_CreateObject();
    cJSON *wm_vd = cJSON_CreateObject();
    unsigned int i;

    if (vuldet->flags.enabled) cJSON_AddStringToObject(wm_vd,"disabled","no"); else cJSON_AddStringToObject(wm_vd,"disabled","yes");
    if (vuldet->flags.run_on_start) cJSON_AddStringToObject(wm_vd,"run_on_start","yes"); else cJSON_AddStringToObject(wm_vd,"run_on_start","no");
    cJSON_AddNumberToObject(wm_vd,"interval",vuldet->detection_interval);
    cJSON_AddNumberToObject(wm_vd,"ignore_time",vuldet->ignore_time);
    cJSON *feeds = cJSON_CreateArray();
    for (i = 0; i < OS_SUPP_SIZE; i++) {
        if (vuldet->updates[i]) {
            cJSON *feed = cJSON_CreateObject();
            if (vuldet->updates[i]->dist) cJSON_AddStringToObject(feed,"name",vuldet->updates[i]->dist);
            if (vuldet->updates[i]->version) cJSON_AddStringToObject(feed,"version",vuldet->updates[i]->version);
            if (vuldet->updates[i]->url) {
                cJSON *alt = cJSON_CreateObject();
                cJSON_AddStringToObject(alt,"url",vuldet->updates[i]->url);
                if (vuldet->updates[i]->port > 0)
                    cJSON_AddNumberToObject(alt,"port",vuldet->updates[i]->port);

                cJSON_AddItemToObject(feed,"alternative",alt);
            }
            cJSON_AddNumberToObject(feed,"interval",vuldet->updates[i]->interval);
            cJSON_AddItemToArray(feeds,feed);
        }
    }
    if (cJSON_GetArraySize(feeds) > 0) {
        cJSON_AddItemToObject(wm_vd,"feeds",feeds);
    } else {
        cJSON_free(feeds);
    }

    cJSON_AddItemToObject(root,"vulnerability-detector",wm_vd);

    return root;

}

char *vu_get_version() {
    static char version[10] = { '\0' };
    int major = 0;
    int minor = 0;
    int patch = 0;

    if (*version == '\0') {
        sscanf(__ossec_version, "v%d.%d.%d", &major, &minor, &patch);
        snprintf(version, 9, "%d%d%d", major, minor, patch);
    }

    return version;
}

int wm_vuldet_db_empty() {
    sqlite3 *db;
    sqlite3_stmt *stmt = NULL;
    int result;

    if (wm_vuldet_check_db()) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_CHECK_DB_ERROR);
        return OS_INVALID;
    }

    if (sqlite3_open_v2(CVE_DB, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        return wm_vuldet_sql_error(db, NULL);
    }

    if (sqlite3_prepare_v2(db, vu_queries[VU_CHECK_UPDATES], -1, &stmt, NULL) != SQLITE_OK) {
        return wm_vuldet_sql_error(db, stmt);
    }

    if (result = wm_vuldet_step(stmt), result == SQLITE_DONE ||
        (result == SQLITE_ROW && !sqlite3_column_int(stmt, 0))) {
        result = (int) VU_TRUE;
    } else {
        result = (int) VU_FALSE;
    }

    sqlite3_finalize(stmt);
    sqlite3_close_v2(db);

    return result;
}

const char *wm_vuldet_get_unified_severity(char *severity) {
    if (!severity || strcasestr(severity, vu_severities[VU_UNKNOWN]) || strcasestr(severity, vu_severities[VU_UNTR])) {
        return vu_severities[VU_UNDEFINED_SEV];
    } else if (strcasestr(severity, vu_severities[VU_LOW]) || strcasestr(severity, vu_severities[VU_NEGL])) {
        return vu_severities[VU_LOW];
    } else if (strcasestr(severity, vu_severities[VU_MEDIUM]) || strcasestr(severity, vu_severities[VU_MODERATE])) {
        return vu_severities[VU_MEDIUM];
    } else if (strcasestr(severity, vu_severities[VU_HIGH]) || strcasestr(severity, vu_severities[VU_IMPORTANT])) {
        return vu_severities[VU_HIGH];
    } else if (strcasestr(severity, vu_severities[VU_CRITICAL])) {
        return vu_severities[VU_CRITICAL];
    } else if (strcasestr(severity, vu_severities[VU_NONE])) {
        return vu_severities[VU_NONE];
    } else {
        static OSHash *unc_severities = NULL;
        static int sev_count = 0;

        if (!unc_severities) {
            if (unc_severities = OSHash_Create(), !unc_severities) {
                mterror(WM_VULNDETECTOR_LOGTAG, VU_CREATE_HASH_ERRO, "unknown_severities");
                pthread_exit(NULL);
            }
        }

        // In case the feed has been corrupted, we will not insert more than 20 severities
        if (sev_count < 20 && OSHash_Add(unc_severities, severity, NULL) == 2) {
            mtwarn(WM_VULNDETECTOR_LOGTAG, VU_UNC_SEVERITY, severity);
            sev_count++;
        }
    }
    return vu_severities[VU_UNDEFINED_SEV];
}

void wm_vuldet_get_package_os(const char *version, const char **os_major, char **os_minor)  {
    char *v_it;

    if (v_it = strstr(version, vu_package_dist[VU_RH_EXT_7]), v_it) {
        *os_major = vu_feed_tag[FEED_RHEL7];
    } else if (v_it = strstr(version, vu_package_dist[VU_RH_EXT_6]), v_it) {
        *os_major = vu_feed_tag[FEED_RHEL6];
    } else if (v_it = strstr(version, vu_package_dist[VU_RH_EXT_5]), v_it) {
        *os_major = vu_feed_tag[FEED_RHEL5];
    }

    wm_vuldet_set_subversion(v_it, os_minor);
}

void wm_vuldet_set_subversion(char *version, char **os_minor) {
    if (version && (version = strchr(version, '_')) && *(++version) != '\0') {
        size_t minor_size = strlen(version) + 1;
        os_calloc(minor_size + 1, sizeof(char), *os_minor);
        snprintf(*os_minor, minor_size, "%ld", strtol(version, NULL, 10));
    }
}

void wm_vuldet_queue_report(vu_processed_alerts **alerts_queue, char *cve, char *package, char *package_version, char *package_arch, char *version_compare, vu_report *report) {

    if (*alerts_queue && (strcmp((*alerts_queue)->cve, cve) || strcmp((*alerts_queue)->package, package) || strcmp((*alerts_queue)->package_version, package_version) || strcmp((*alerts_queue)->package_arch, package_arch))) {
        wm_vuldet_queue_report_higher(alerts_queue);
    }

    if (!*alerts_queue) {
        os_calloc(1, sizeof(vu_processed_alerts), *alerts_queue);
        os_strdup(cve, (*alerts_queue)->cve);
        os_strdup(package, (*alerts_queue)->package);
        os_strdup(package_version, (*alerts_queue)->package_version);
        os_strdup(package_arch, (*alerts_queue)->package_arch);
    }

    wm_vuldet_queue_report_add(alerts_queue, version_compare, report);
}

void wm_vuldet_queue_report_add(vu_processed_alerts **alerts_queue, char *version_compare, vu_report *report) {
    vu_alerts_node *alert_node;

    os_calloc(1, sizeof(vu_alerts_node), alert_node);
    w_strdup(version_compare, alert_node->version_compare);
    alert_node->report = report;

    alert_node->next = (*alerts_queue)->report_queue;
    (*alerts_queue)->report_queue = alert_node;
}

void wm_vuldet_queue_report_higher(vu_processed_alerts **alerts_queue) {
    vu_alerts_node *node_it;
    vu_alerts_node *node_higher = NULL;
    int result;

    mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_ANAL_ACC_REPORTS, (*alerts_queue)->package, (*alerts_queue)->cve);


    for (node_it = (*alerts_queue)->report_queue; node_it; node_it = node_it->next) {
        if (!node_higher) {
            node_higher = node_it;
            if (!node_higher->version_compare) {
                mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_NO_VER_REPORT, (*alerts_queue)->cve);
                break;
            }
        } else {
            if (result = wm_checks_package_vulnerability(node_higher->version_compare, "less than", node_it->version_compare), result == VU_VULNERABLE) {
                mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_ADD_ACC_REPORTS, (*alerts_queue)->cve, node_it->version_compare, node_higher->version_compare);
                node_higher = node_it;
            } else {
                mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_DISC_ACC_REPORTS, (*alerts_queue)->cve, node_it->version_compare, node_higher->version_compare);
            }
        }
    }

    if (node_higher && wm_vuldet_send_agent_report(node_higher->report)) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_SEND_AGENT_REPORT_ERROR, node_higher->report->cve, node_higher->report->software, node_higher->report->agent_id);
    }

    wm_vuldet_queue_report_clean(alerts_queue);
}

void wm_vuldet_queue_report_clean(vu_processed_alerts **alerts_queue) {
    vu_alerts_node *node_it;
    vu_alerts_node *n_node;

    for (node_it = (*alerts_queue)->report_queue; node_it; node_it = n_node) {
        n_node = node_it->next;
        free(node_it->version_compare);
        wm_vuldet_free_report(node_it->report);
        free(node_it);
    }

    free((*alerts_queue)->cve);
    free((*alerts_queue)->package);
    free((*alerts_queue)->package_version);
    free((*alerts_queue)->package_arch);
    free(*alerts_queue);
    *alerts_queue = NULL;
}

void wm_vuldet_generate_os_cpe(agent_software *agent) {
    if (agent->dist_ver >= FEED_WS2003 && agent->dist_ver <= FEED_WS2019) {
        // It is a supported Windows agent
        wm_vuldet_generate_win_cpe(agent);
    }
}

void wm_vuldet_generate_win_cpe(agent_software *agent) {
    static const char *PR_WINXP = "windows_xp";
    static const char *PR_WINVISTA = "windows_vista";
    static const char *PR_WIN7 = "windows_7";
    static const char *PR_WIN8 = "windows_8";
    static const char *PR_WIN81 = "windows_8.1";
    static const char *PR_WIN10 = "windows_10";
    static const char *PR_WIN2003 = "windows_server_2003";
    static const char *PR_WIN2008 = "windows_server_2008";
    static const char *PR_WIN2012 = "windows_server_2012";
    static const char *PR_WIN2016 = "windows_server_2016";
    static const char *PR_WIN2019 = "windows_server_2019";
    static const char *VER_R2 = "r2";
    const char *win_pr = NULL;
    const char *win_ver = NULL;
    const char *win_upd = NULL;

    switch (agent->dist_ver) {
        case FEED_WS2003:
            win_pr = PR_WIN2003;
            win_ver = agent->os_release;
        break;
        case FEED_WS2003R2:
            win_pr = PR_WIN2003;
            win_ver = VER_R2;
            win_upd = agent->os_release;
        break;
        case FEED_WXP:
            win_pr = PR_WINXP;
            win_ver = agent->os_release;
        break;
        case FEED_WVISTA:
            win_pr = PR_WINVISTA;
            win_ver = agent->os_release;
        break;
        case FEED_W7:
            win_pr = PR_WIN7;
            win_ver = agent->os_release;
        break;
        case FEED_W8:
            win_pr = PR_WIN8;
            win_ver = agent->os_release;
        break;
        case FEED_W81:
            win_pr = PR_WIN81;
            win_ver = agent->os_release;
        break;
        case FEED_W10:
            win_pr = PR_WIN10;
            win_ver = agent->os_release;
        break;
        case FEED_WS2008:
            win_pr = PR_WIN2008;
            win_ver = agent->os_release;
        break;
        case FEED_WS2008R2:
            win_pr = PR_WIN2008;
            win_ver = VER_R2;
            win_upd = agent->os_release;
        break;
        case FEED_WS2012:
            win_pr = PR_WIN2012;
            win_ver = agent->os_release;
        break;
        case FEED_WS2012R2:
            win_pr = PR_WIN2012;
            win_ver = VER_R2;
            win_upd = agent->os_release;
        break;
        case FEED_WS2016:
            win_pr = PR_WIN2016;
            win_ver = agent->os_release;
        break;
        case FEED_WS2019:
            win_pr = PR_WIN2019;
            win_ver = agent->os_release;
        break;
        default:
            return;
    }

    agent->agent_os_cpe = wm_vuldet_generate_cpe("o",
                                                "microsoft",
                                                win_pr,
                                                win_ver,
                                                win_upd,
                                                NULL,
                                                NULL,
                                                NULL,
                                                NULL,
                                                agent->arch,
                                                NULL,
                                                0,
                                                vu_feed_ext[agent->dist_ver]);
}

vu_feed wm_vuldet_decode_win_os(char *os_raw) {
    if (strcasestr(os_raw, vu_feed_ext[FEED_WS2003R2])) {
        return FEED_WS2003R2;
    } else if (strcasestr(os_raw, vu_feed_ext[FEED_WS2003])) {
        return FEED_WS2003;
    } else if (strcasestr(os_raw, vu_feed_ext[FEED_WXP])) {
        return FEED_WXP;
    } else if (strcasestr(os_raw, vu_feed_ext[FEED_WVISTA])) {
        return FEED_WVISTA;
    } else if (strcasestr(os_raw, vu_feed_ext[FEED_W7])) {
        return FEED_W7;
    } else if (strcasestr(os_raw, vu_feed_ext[FEED_W8])) {
        return FEED_W8;
    } else if (strcasestr(os_raw, vu_feed_ext[FEED_W81])) {
        return FEED_W81;
    } else if (strcasestr(os_raw, vu_feed_ext[FEED_W10])) {
        return FEED_W10;
    } else if (strcasestr(os_raw, vu_feed_ext[FEED_WS2008R2])) {
        return FEED_WS2008R2;
    } else if (strcasestr(os_raw, vu_feed_ext[FEED_WS2008])) {
        return FEED_WS2008;
    } else if (strcasestr(os_raw, vu_feed_ext[FEED_WS2012R2])) {
        return FEED_WS2012R2;
    } else if (strcasestr(os_raw, vu_feed_ext[FEED_WS2012])) {
        return FEED_WS2012;
    } else if (strcasestr(os_raw, vu_feed_ext[FEED_WS2016])) {
        return FEED_WS2016;
    } else if (strcasestr(os_raw, vu_feed_ext[FEED_WS2019])) {
        return FEED_WS2019;
    } else {
        static OSHash *unk_os = NULL;
        static int os_count = 0;

        if (!os_count) {
            if (unk_os = OSHash_Create(), !unk_os) {
                mterror(WM_VULNDETECTOR_LOGTAG, VU_CREATE_HASH_ERRO, "unknown_systems");
                pthread_exit(NULL);
            }
        }

        if (os_count < 20 && OSHash_Add(unk_os, os_raw, NULL) == 2) {
            mtwarn(WM_VULNDETECTOR_LOGTAG, VU_UNC_SYSTEM, os_raw);
            os_count++;
        }
        return FEED_WIN;
    }
}

int wm_vuldet_insert_agent_data(sqlite3 *db, agent_software *agent, int *cpe_index, const char *vendor, const char *product, const char *version, const char *arch, cpe *ag_cpe, cpe_list **node_list) {
    sqlite3_stmt *stmt = NULL;
    int retval = OS_INVALID;
    int result;
    const char *os_major = NULL;
    char *os_minor = NULL;

    if (agent->dist == FEED_REDHAT) {
        wm_vuldet_get_package_os(version, &os_major, &os_minor);
    }

    if (sqlite3_prepare_v2(db, vu_queries[VU_INSERT_AGENTS], -1, &stmt, NULL) != SQLITE_OK) {
        wm_vuldet_sql_error(db, stmt);
        goto end;
    }

    sqlite3_bind_text(stmt, 1, agent->agent_id, -1, NULL);
    sqlite3_bind_text(stmt, 2, os_major, -1, NULL);
    sqlite3_bind_text(stmt, 3, os_minor, -1, NULL);
    sqlite3_bind_int(stmt, 4, ag_cpe ? *cpe_index : 0);
    sqlite3_bind_text(stmt, 5, vendor, -1, NULL);
    sqlite3_bind_text(stmt, 6, product, -1, NULL);
    sqlite3_bind_text(stmt, 7, version, -1, NULL);
    sqlite3_bind_text(stmt, 8, arch, -1, NULL);

    if (ag_cpe) {
        if (!*node_list) {
            os_calloc(1, sizeof(cpe_list), *node_list);
        }

        if (wm_vuldet_add_cpe(ag_cpe, NULL, *node_list, *cpe_index)) {
            mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_AGENT_CPE_PARSE_ERROR, ag_cpe->raw, atoi(agent->agent_id));
        } else {
            mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_AGENT_CPE_RECV, ag_cpe->raw, atoi(agent->agent_id));
            *cpe_index = *cpe_index - 1;
        }
    }

    if (result = wm_vuldet_step(stmt), result != SQLITE_DONE && result != SQLITE_CONSTRAINT) {
        wm_vuldet_sql_error(db, stmt);
        goto end;
    }

    retval = 0;
end:
    free(os_minor);
    wdb_finalize(stmt);
    return retval;
}

void wm_vuldet_free_agent_software(agent_software *agent) {
    if (agent) {
        free(agent->agent_id);
        free(agent->agent_name);
        free(agent->arch);
        free(agent->agent_ip);
        if (agent->agent_os_cpe) {
            wm_vuldet_free_cpe(agent->agent_os_cpe);
        }
        free(agent);
    }
}

int wm_vuldet_fetch_MSU() {
    int retval = VU_INV_FEED;
    char buffer[OS_MAXSTR + 1];
    FILE *fp = NULL;
    FILE *fp_out;

    if (fp = fopen(VU_MSU_FILE, "r"), !fp) {
        goto end;
    }
    if (fp_out = fopen(VU_FIT_TEMP_FILE, "w"), !fp_out) {
        goto end;
    }

    while (fgets(buffer, OS_MAXSTR, fp)) {
        fwrite(buffer, 1, strlen(buffer), fp_out);
    }
    w_fclose(fp);
    fclose(fp_out);

    retval = 0;
end:
    if (fp) {
        fclose(fp);
    }

    return retval;
}


int wm_vuldet_fetch_wazuh_cpe(update_node *update) {
    int retval = VU_INV_FEED;
    char buffer[OS_MAXSTR + 1];
    FILE *fp = NULL;
    FILE *fp_out;
    static const char *UPDATE_DATE_TAG = "update_date";

    if (fp = fopen(VU_CPE_HELPER_FILE, "r"), !fp) {
        goto end;
    }
    if (fp_out = fopen(VU_FIT_TEMP_FILE, "w"), !fp_out) {
        goto end;
    }

    while (fgets(buffer, OS_MAXSTR, fp)) {
        if (*buffer == '=') {
            continue;
        }
        fwrite(buffer, 1, strlen(buffer), fp_out);
    }
    fclose(fp);
    fclose(fp_out);

    if (fp = fopen(VU_FIT_TEMP_FILE, "r"), !fp) {
        goto end;
    }

    // Check the timestamp
    while (fgets(buffer, OS_MAXSTR, fp)) {
        char *timestamp_base;
        char *timestamp_end;

        if ((timestamp_base = strstr(buffer, UPDATE_DATE_TAG)) &&
            (timestamp_base = strchr(timestamp_base, ':')) &&
            (timestamp_base = strchr(timestamp_base, '\"')) &&
            (timestamp_end = strchr(++timestamp_base, '\"'))) {
            char timestamp[OS_SIZE_256 + 1];

            *timestamp_end = '\0';
            switch (wm_vuldet_check_timestamp(vu_feed_tag[update->dist_tag_ref], timestamp_base, timestamp)) {
                case VU_TIMESTAMP_FAIL:
                    mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_DB_TIMESTAMP_FEED_ERROR, update->dist_ext);
                    goto end;
                break;
                case VU_TIMESTAMP_UPDATED:
                    mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_UPDATE_DATE, update->dist_ext, timestamp);
                    retval = VU_NOT_NEED_UPDATE;
                    goto end;
                break;
                case VU_TIMESTAMP_OUTDATED:
                    mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_DB_TIMESTAMP_FEED, update->dist_ext, "");
                break;
            }
            break;
        }
    }

    retval = 0;
end:
    if (fp) {
        fclose(fp);
    }

    return retval;
}

int wm_vuldet_json_wcpe_parser(cJSON *json_feed, wm_vuldet_db *parsed_vulnerabilities) {
    cJSON *json_it;
    vu_cpe_dic *wcpes_it = NULL;
    static char *JSON_WCPE_FORMAT_VERSION = "format_version";
    static char *JSON_WCPE_UPDATE_DATE = "update_date";
    static char *JSON_WCPE_DICTIONARY = "dictionary";
    char *timestamp;
    char *format_version_major;
    int major_dec;

    // Check the timestamp
    if (json_it = cJSON_GetObjectItem(json_feed, JSON_WCPE_UPDATE_DATE), !json_it) {
        return OS_INVALID;
    }

    if (timestamp = json_it->valuestring, !timestamp) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_WCPE_GET_TIMEST_ERROR);
        return OS_INVALID;
    }

    switch (wm_vuldet_check_timestamp(vu_feed_tag[FEED_CPEW], timestamp, NULL)) {
        case VU_TIMESTAMP_FAIL:
            mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_DB_TIMESTAMP_FEED_ERROR, vu_feed_ext[FEED_CPEW]);
            return OS_INVALID;
        break;
        case VU_TIMESTAMP_UPDATED:
            mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_UPDATE_DATE, vu_feed_ext[FEED_CPEW], timestamp);
            return VU_NOT_NEED_UPDATE;
        break;
        case VU_TIMESTAMP_OUTDATED:
            mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_DB_TIMESTAMP_FEED, vu_feed_ext[FEED_CPEW], "");
        break;
    }

    // Check the version format
    if (json_it = cJSON_GetObjectItem(json_feed, JSON_WCPE_FORMAT_VERSION), !json_it) {
        return OS_INVALID;
    }

    if (format_version_major = json_it->valuestring, !format_version_major) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_WCPE_GET_VERFORMAT_ERROR);
        return OS_INVALID;
    }

    if (major_dec = strtol(format_version_major, NULL, 10), major_dec < 1 || major_dec > VU_CPEHELPER_FORMAT_VERSION) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_WCPE_INV_VERFORMAT, format_version_major);
        return OS_INVALID;
    }

    os_calloc(1, sizeof(vu_cpe_dic), wcpes_it);

    for (json_it = json_feed->child; json_it; json_it = json_it->next) {
        if (!strcmp(json_it->string, vu_cpe_tags[CPE_VERSION])) {
            os_strdup(json_it->valuestring, wcpes_it->version);
        } else if (!strcmp(json_it->string, JSON_WCPE_FORMAT_VERSION)) {
            os_strdup(json_it->valuestring, wcpes_it->format_version);
        } else if (!strcmp(json_it->string, JSON_WCPE_UPDATE_DATE)) {
            os_strdup(json_it->valuestring, wcpes_it->update_date);
        } else if (!strcmp(json_it->string, JSON_WCPE_DICTIONARY)) {
            wm_vuldet_json_dic_parser(json_it->child, wcpes_it);
        }
    }
    parsed_vulnerabilities->w_cpes = wcpes_it;

    return VU_NEED_UPDATE;
}

int wm_vuldet_json_dic_parser(cJSON *json_feed, vu_cpe_dic *wcpes) {
    cJSON *dic_node;
    char need_check_hotfix = 0;
    int i;
    static char *JSON_WCPE_ACTION = "action";
    static char *JSON_WCPE_TARGET = "target";
    static char *JSON_WCPE_SOURCE = "source";
    static char *JSON_WCPE_TRANSLATION = "translation";

    for (; json_feed; json_feed = json_feed->next) {
        vu_cpe_dic_node *node;
        os_calloc(1, sizeof(vu_cpe_dic_node), node);
        for (dic_node = json_feed->child; dic_node; dic_node = dic_node->next) {
            if (!strcmp(dic_node->string, JSON_WCPE_TARGET)) {
                vu_cpe_dic_node *node_ref = NULL;

                for (i = 0; wcpes->targets_name && wcpes->targets_name[i]; i++) {
                    if (!strcmp(wcpes->targets_name[i], dic_node->valuestring)) {
                        node_ref = wcpes->targets[i];
                        break;
                    }
                }

                if (!node_ref) {
                    os_realloc(wcpes->targets_name, (i + 2) * sizeof(char *), wcpes->targets_name);
                    os_realloc(wcpes->targets, (i + 2) * sizeof(vu_cpe_dic_node *), wcpes->targets);
                    os_strdup(dic_node->valuestring, wcpes->targets_name[i]);
                    wcpes->targets[i + 1] = NULL;
                    wcpes->targets_name[i + 1] = NULL;
                } else {
                    node->prev = wcpes->targets[i];
                    wcpes->targets[i]->next = node;
                }

                wcpes->targets[i] = node;
            } else if (!strcmp(dic_node->string, JSON_WCPE_ACTION)) {
                int msu_mask = CPE_DIC_REP_MSUN_IF_VER_MATCHES | CPE_DIC_REP_MSUN;

                node->action = wm_vuldet_fill_action_mask(dic_node->child);
                if ((node->action & msu_mask && !(node->action & CPE_DIC_CHECK_HOTFIX)) ||
                    (node->action & CPE_DIC_CHECK_HOTFIX && !(node->action & msu_mask))) {
                    need_check_hotfix = 1;
                }
            } else if (!strcmp(dic_node->string, JSON_WCPE_SOURCE)) {
                wm_vuldet_fill_dic_section(dic_node->child, &node->source);
            } else if (!strcmp(dic_node->string, JSON_WCPE_TRANSLATION)) {
                wm_vuldet_fill_dic_section(dic_node->child, &node->translation);
            }
        }
    }

    if (need_check_hotfix) {
        mtwarn(WM_VULNDETECTOR_LOGTAG, VU_CPEHELPER_INVALID_ACTION, vu_cpe_dic_option[VU_REP_MSUN_IF_VER_MATCHES], vu_cpe_dic_option[VU_CHECK_HOTFIX]);
    }

    return 0;
}

int wm_vuldet_fill_dic_section(cJSON *json_feed, vu_cpe_dic_section **wcpes) {
    cJSON *json_array;

    if (!*wcpes) {
        os_calloc(1, sizeof(vu_cpe_dic_section), *wcpes);
    }

    for (; json_feed; json_feed = json_feed->next) {
        char ****array;
        int i;

        if (!strcmp(json_feed->string, vu_cpe_tags[CPE_VENDOR])) {
            array = &(*wcpes)->vendor;
            if (!json_feed->child) {
                os_realloc(*array, 2 * sizeof(char **), *array);
                (*array)[1] = NULL;
                os_calloc(2, sizeof(char *), (*array)[0]);
                os_strdup("", ***array);
            }
        } else if (!strcmp(json_feed->string, vu_cpe_tags[CPE_PRODUCT])) {
            array = &(*wcpes)->product;
        } else if (!strcmp(json_feed->string, vu_cpe_tags[CPE_VERSION])) {
            array = &(*wcpes)->version;
        } else if (!strcmp(json_feed->string, vu_cpe_tags[CPE_SW_EDITION])) {
            array = &(*wcpes)->sw_edition;
        } else if (!strcmp(json_feed->string, vu_cpe_tags[CPE_MSU_NAME])) {
            array = &(*wcpes)->msu_name;
        } else {
            mtwarn(WM_VULNDETECTOR_LOGTAG, VU_INVALID_DIC_TAG, json_feed->string);
            continue;
        }

        for (i = 0, json_array = json_feed->child; json_array; json_array = json_array->next, i++) {
            os_realloc(*array, (i + 2) * sizeof(char **), *array);
            memset(&(*array)[i], 0, 2 * sizeof(char **));

            if (json_array->type == cJSON_String) {
                os_calloc(2, sizeof(char *), (*array)[i]);
                os_strdup(json_array->valuestring, (*array)[i][0]);
            } else if (json_array->type == cJSON_Array) {
                cJSON *json_sub_array = json_array->child;
                int j = 0;

                for (; json_sub_array; json_sub_array = json_sub_array->next, j++) {
                    if (json_sub_array->type != cJSON_String) {
                        mterror(WM_VULNDETECTOR_LOGTAG, VU_INV_ENTRY_TYPE, -1 * json_array->type);
                        j--;
                    } else {
                        os_realloc((*array)[i], (j + 2) * sizeof(char *), (*array)[i]);
                        os_strdup(json_sub_array->valuestring, (*array)[i][j]);
                        (*array)[i][j + 1] = NULL;
                    }
                }
            } else {
                mterror(WM_VULNDETECTOR_LOGTAG, VU_INV_ENTRY_TYPE, json_array->type);
                i--;
            }
        }
    }

    return 0;
}

unsigned int wm_vuldet_fill_action_mask(cJSON *json_feed) {
    unsigned int mask = 0;

    for (; json_feed; json_feed = json_feed->next) {
        if (!strcmp(json_feed->valuestring, vu_cpe_dic_option[VU_REP_VEN])) {
            mask |= CPE_DIC_REP_VEN;
        } else if (!strcmp(json_feed->valuestring, vu_cpe_dic_option[VU_REP_PROD])) {
            mask |= CPE_DIC_REP_PROD;
        } else if (!strcmp(json_feed->valuestring, vu_cpe_dic_option[VU_REP_VEN_IF_MATCH])) {
            mask |= CPE_DIC_REP_VEN_IF_MATCH;
        } else if (!strcmp(json_feed->valuestring, vu_cpe_dic_option[VU_REP_PROD_IF_MATCH])) {
            mask |= CPE_DIC_REP_PROD_IF_MATCH;
        } else if (!strcmp(json_feed->valuestring, vu_cpe_dic_option[VU_SET_VER_IF_MATCHES])) {
            mask |= CPE_DIC_SET_VER_IF_MATCH;
        } else if (!strcmp(json_feed->valuestring, vu_cpe_dic_option[VU_REP_SWED_VER_IF_PR_MATCHES])) {
            mask |= CPE_DIC_REP_SWED_IF_PROD_MATCHES;
        } else if (!strcmp(json_feed->valuestring, vu_cpe_dic_option[VU_REP_MSUN_IF_VER_MATCHES])) {
            mask |= CPE_DIC_REP_MSUN_IF_VER_MATCHES;
        } else if (!strcmp(json_feed->valuestring, vu_cpe_dic_option[VU_IGNORE])) {
            mask |= CPE_DIC_IGNORE;
        } else if (!strcmp(json_feed->valuestring, vu_cpe_dic_option[VU_CHECK_HOTFIX])) {
            mask |= CPE_DIC_CHECK_HOTFIX;
        } else if (!strcmp(json_feed->valuestring, vu_cpe_dic_option[VU_REP_MSUN])) {
            mask |= CPE_DIC_REP_MSUN;
        } else if (!strcmp(json_feed->valuestring, vu_cpe_dic_option[VU_SET_VERSION_IF_PROD_MATCHES])) {
            mask |= CPE_DIC_SET_VER_IF_PROD_MATCHES;
        } else if (!strcmp(json_feed->valuestring, vu_cpe_dic_option[VU_REP_ARCH_IF_PROD_MATCHES])) {
            mask |= CPE_DIC_REP_ARCH_IF_PRODUCT_MATCHES;
        } else if (!strcmp(json_feed->valuestring, vu_cpe_dic_option[VU_SET_VER_ONLY_IF_PROD_MATCHES])) {
            mask |= CPE_DIC_SET_VER_ONLY_IF_PROD_MATCHES;
        } else if (!strcmp(json_feed->valuestring, vu_cpe_dic_option[VU_REP_ARCH_ONLY_IF_PROD_MATCHES])) {
            mask |= CPE_DIC_REP_ARCH_ONLY_IF_PRODUCT_MATCHES;
        } else {
            mtwarn(WM_VULNDETECTOR_LOGTAG, VU_INVALID_DIC_TAG, json_feed->valuestring);
        }
    }

    return mask;
}

int wm_vuldet_json_msu_parser(cJSON *json_feed, wm_vuldet_db *parsed_vulnerabilities) {
    vu_msu_entry *msu = NULL;
    cJSON *vulnerability;
    cJSON *vuln_node;
    static char *JSON_MSU_PATCH = "patch";
    static char *JSON_MSU_PRODUCT = "product";
    static char *JSON_MSU_RESTART = "restart_required";
    static char *JSON_MSU_SUBTYPE = "subtype";
    static char *JSON_MSU_TITLE = "title";
    static char *JSON_MSU_URL = "url";

    for (json_feed = json_feed->child; json_feed; json_feed = json_feed->next) {
        for (vuln_node = json_feed->child; vuln_node; vuln_node = vuln_node->next) {
            if (msu) {
                os_calloc(1, sizeof(vu_msu_entry), msu->next);
                msu->next->prev = msu;
                msu = msu->next;
            } else {
                os_calloc(1, sizeof(vu_msu_entry), msu);
            }

            os_strdup(json_feed->string, msu->cveid);
            for (vulnerability = vuln_node->child; vulnerability; vulnerability = vulnerability->next) {
                if (!strcmp(vulnerability->string, JSON_MSU_PATCH)) {
                    os_strdup(vulnerability->valuestring, msu->patch);
                } else if (!strcmp(vulnerability->string, JSON_MSU_RESTART)) {
                    os_strdup(vulnerability->valuestring, msu->restart_required);
                } else if (!strcmp(vulnerability->string, JSON_MSU_PRODUCT)) {
                    os_strdup(vulnerability->valuestring, msu->product);
                } else if (!strcmp(vulnerability->string, JSON_MSU_SUBTYPE)) {
                    os_strdup(vulnerability->valuestring, msu->subtype);
                } else if (!strcmp(vulnerability->string, JSON_MSU_TITLE)) {
                    os_strdup(vulnerability->valuestring, msu->title);
                } else if (!strcmp(vulnerability->string, JSON_MSU_URL)) {
                    os_strdup(vulnerability->valuestring, msu->url);
                }
            }
        }
    }
    parsed_vulnerabilities->msu_entries = msu;

    return 0;
}

int wm_vuldet_insert_cpe_dic(sqlite3 *db, vu_cpe_dic *w_cpes) {
    int i;
    int entry_id;
    vu_cpe_dic_node *node_it;
    sqlite3_stmt *stmt;

    sqlite3_exec(db, vu_queries[VU_REMOVE_CPE_DIC], NULL, NULL, NULL);

    for (i = 0, entry_id = 0; w_cpes->targets_name[i]; i++) {
        for (node_it = w_cpes->targets[i]; node_it; node_it = wm_vuldet_free_dic_node(node_it), entry_id++) {

            if (sqlite3_prepare_v2(db, vu_queries[VU_INSERT_CPE_HELPER], -1, &stmt, NULL) != SQLITE_OK) {
                return wm_vuldet_sql_error(db, stmt);
            }
            sqlite3_bind_int(stmt, 1, entry_id);
            sqlite3_bind_text(stmt, 2, w_cpes->targets_name[i], -1, NULL);
            sqlite3_bind_int(stmt, 3, node_it->action);
            if (wm_vuldet_step(stmt) != SQLITE_DONE) {
                return wm_vuldet_sql_error(db, stmt);
            }
            sqlite3_finalize(stmt);

            if (wm_vuldet_insert_cpe_dic_array(db, VU_INSERT_CPE_SOURCE, entry_id, node_it->source)) {
                return OS_INVALID;
            }
            if (wm_vuldet_insert_cpe_dic_array(db, VU_INSERT_CPE_TRANSLATION, entry_id, node_it->translation)) {
                return OS_INVALID;
            }
        }
        free(w_cpes->targets_name[i]);
    }
    os_free(w_cpes->targets_name);
    os_free(w_cpes->targets);

    if (sqlite3_prepare_v2(db, vu_queries[VU_INSERT_METADATA], -1, &stmt, NULL) != SQLITE_OK) {
        return wm_vuldet_sql_error(db, stmt);
    }

    sqlite3_bind_text(stmt, 1, vu_feed_tag[FEED_CPEW], -1, NULL);
    sqlite3_bind_text(stmt, 2, vu_feed_ext[FEED_CPEW], -1, NULL);
    sqlite3_bind_text(stmt, 3, w_cpes->version, -1, NULL);
    sqlite3_bind_text(stmt, 4, w_cpes->format_version, -1, NULL);
    sqlite3_bind_text(stmt, 5, w_cpes->update_date, -1, NULL);

    if (wm_vuldet_step(stmt) != SQLITE_DONE) {
        return wm_vuldet_sql_error(db, stmt);
    }
    sqlite3_finalize(stmt);

    os_free(w_cpes->version);
    os_free(w_cpes->format_version);
    os_free(w_cpes->update_date);

    return 0;
}

int wm_vuldet_insert_cpe_dic_array(sqlite3 *db, vu_query query, int id, vu_cpe_dic_section *section) {
    sqlite3_stmt *stmt;
    int type_value;
    int pattern_it, subpattern_it;
    char ***section_it;
    const char *type;

    for (type_value = 0; type_value < 5; type_value++) {
        switch (type_value) {
            case 0:
                section_it = section->vendor;
                type = vu_cpe_tags[CPE_VENDOR];
            break;
            case 1:
                section_it = section->product;
                type = vu_cpe_tags[CPE_PRODUCT];
            break;
            case 2:
                section_it = section->version;
                type = vu_cpe_tags[CPE_VERSION];
            break;
            case 3:
                section_it = section->sw_edition;
                type = vu_cpe_tags[CPE_SW_EDITION];
            break;
            case 4:
                section_it = section->msu_name;
                type = vu_cpe_tags[CPE_MSU_NAME];
            break;
        }

        for (pattern_it = 0; section_it && section_it[pattern_it]; pattern_it++) {
            for (subpattern_it = 0; section_it[pattern_it][subpattern_it]; subpattern_it++) {
                char *term = NULL;
                char *cterm;
                char *comp_field = NULL;
                char *condition = NULL;

                if (sqlite3_prepare_v2(db, vu_queries[query], -1, &stmt, NULL) != SQLITE_OK) {
                    return wm_vuldet_sql_error(db, stmt);
                }

                cterm = !*section_it[pattern_it][subpattern_it] &&
                                            query == VU_INSERT_CPE_SOURCE &&
                                            !type_value ?
                                            NULL : section_it[pattern_it][subpattern_it];
                w_strdup(cterm, term);

                sqlite3_bind_int(stmt, 1, id);
                sqlite3_bind_int(stmt, 2, pattern_it);
                sqlite3_bind_text(stmt, 3, type, -1, NULL);

                if (query == VU_INSERT_CPE_TRANSLATION) {
                    char *exact_term;
                    if (wm_vuldet_get_term_condition(term, &exact_term, &comp_field, &condition)) {
                        free(term);
                        term = exact_term;
                        sqlite3_bind_text(stmt, 5, comp_field, -1, NULL);
                        sqlite3_bind_text(stmt, 6, condition, -1, NULL);
                    } else {
                        sqlite3_bind_null(stmt, 5);
                        sqlite3_bind_null(stmt, 6);
                    }
                }

                sqlite3_bind_text(stmt, 4, term, -1, NULL);
                if (wm_vuldet_step(stmt) != SQLITE_DONE) {
                    return wm_vuldet_sql_error(db, stmt);
                }

                sqlite3_finalize(stmt);
                free(term);
                free(comp_field);
                free(condition);
            }
        }
    }

    return 0;
}

vu_cpe_dic_node *wm_vuldet_free_dic_node(vu_cpe_dic_node *dic_node) {
    int i;
    vu_cpe_dic_node *prev = dic_node->prev;
    vu_cpe_dic_section *section;

    for (i = 0; i < 2; i++) {
        if (!i) {
            section = dic_node->source;
        } else if (i == 1) {
            section = dic_node->translation;
        } else {
            continue;
        }

        w_FreeDoubleArray(section->vendor);
        w_FreeDoubleArray(section->product);
        w_FreeDoubleArray(section->version);
        w_FreeDoubleArray(section->sw_edition);
        w_FreeDoubleArray(section->msu_name);
        free(section->vendor);
        free(section->product);
        free(section->version);
        free(section->sw_edition);
        free(section->msu_name);
        free(section);
    }

    free(dic_node);

    return prev;
}

vu_msu_entry *wm_vuldet_free_msu_node(vu_msu_entry *node) {
    vu_msu_entry *prev = node->prev;
    free(node->cveid);
    free(node->product);
    free(node->patch);
    free(node->title);
    free(node->url);
    free(node->subtype);
    free(node->restart_required);
    free(node);
    return prev;
}

int wm_vuldet_insert_MSU(sqlite3 *db, vu_msu_entry *msu) {
    sqlite3_stmt *stmt = NULL;
    int result;

    sqlite3_exec(db, vu_queries[VU_REMOVE_MSU], NULL, NULL, NULL);

    for (; msu; msu = wm_vuldet_free_msu_node(msu)) {
        if (sqlite3_prepare_v2(db, vu_queries[VU_INSERT_MSU], -1, &stmt, NULL) != SQLITE_OK) {
            return wm_vuldet_sql_error(db, stmt);
        }

        sqlite3_bind_text(stmt, 1, msu->cveid, -1, NULL);
        sqlite3_bind_text(stmt, 2, msu->product, -1, NULL);
        sqlite3_bind_text(stmt, 3, msu->patch, -1, NULL);
        sqlite3_bind_text(stmt, 4, msu->title, -1, NULL);
        sqlite3_bind_text(stmt, 5, msu->url, -1, NULL);
        sqlite3_bind_text(stmt, 6, msu->subtype, -1, NULL);
        sqlite3_bind_text(stmt, 7, msu->restart_required, -1, NULL);

        if (result = wm_vuldet_step(stmt), result != SQLITE_DONE && result != SQLITE_CONSTRAINT) {
            return wm_vuldet_sql_error(db, stmt);
        }
        sqlite3_finalize(stmt);
    }

    return 0;
}

int wm_vuldet_clean_agent_cpes(int sock, char *agent_id) {
    int retval = OS_INVALID;
    char request[OS_SIZE_128 + 1];
    size_t size;

    size = snprintf(request, OS_SIZE_128, vu_queries[VU_SYSC_CLEAN_CPES], agent_id);

    if (OS_SendSecureTCP(sock, size + 1, request) || OS_RecvSecureTCP(sock, request, size) < 3){
        goto end;
    }

    if (strncmp(request, "ok", 2)) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_INV_WAZUHDB_RES, request);
        goto end;
    }
    retval = 0;
end:
    if (retval) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_CPE_CLEAN_REQUEST_ERROR, agent_id);
    }

    return retval;
}

int wm_vuldet_request_hotfixes(sqlite3 *db, int sock, char *agent_id) {
    int retval = OS_INVALID;
    char request[OS_MAXSTR + 1];
    size_t size;
    char *found;
    cJSON *obj = NULL;
    cJSON *obj_it;
    sqlite3_stmt *stmt = NULL;

    size = snprintf(request, OS_SIZE_128, vu_queries[VU_HOTFIXES_REQUEST], agent_id);

    if (OS_SendSecureTCP(sock, size + 1, request) || (size = OS_RecvSecureTCP(sock, request, OS_MAXSTR)) < 3) {
        goto end;
    }

    if (strncmp(request, "ok", 2)) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_INV_WAZUHDB_RES, request);
        goto end;
    }

    request[size] = '\0';
    request[0] = request[1] = ' ';

    // Check empty answers
    if ((found = strchr(request, '[')) && *(++found) != '\0' && *found == ']') {
       retval = 0;
       goto end;
    }

    if (obj = cJSON_Parse(request), !obj) {
        goto end;
    }

    sqlite3_exec(db, vu_queries[BEGIN_T], NULL, NULL, NULL);
    for (obj_it = obj->child; obj_it; obj_it = obj_it->next) {
        if (sqlite3_prepare_v2(db, vu_queries[VU_INSERT_AGENT_HOTFIXES], -1, &stmt, NULL) != SQLITE_OK) {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_SQL_ERROR, sqlite3_errmsg(db));
            goto end;
        }

        sqlite3_bind_text(stmt, 1, agent_id, -1, NULL);
        sqlite3_bind_text(stmt, 2, obj_it->child->valuestring, -1, NULL);

        if (wm_vuldet_step(stmt) != SQLITE_DONE) {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_SQL_ERROR, sqlite3_errmsg(db));
            goto end;
        }
        wdb_finalize(stmt);
    }

    sqlite3_exec(db, vu_queries[END_T], NULL, NULL, NULL);

    retval = 0;
end:
    wdb_finalize(stmt);
    if (obj) {
        cJSON_Delete(obj);
    }

    if (retval) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_HOTFIX_REQUEST_ERROR, agent_id);
    }

    return retval;
}

void wm_vuldet_reset_tables(sqlite3 *db) {
    sqlite3_exec(db, vu_queries[VU_REMOVE_AGENTS_TABLE], NULL, NULL, NULL);
    sqlite3_exec(db, vu_queries[VU_REMOVE_AGENT_CPE], NULL, NULL, NULL);
    sqlite3_exec(db, vu_queries[VU_REMOVE_HOTFIXES_TABLE], NULL, NULL, NULL);
}

int wm_vuldet_get_oswindows_info(int sock, agent_software *agent) {
    int retval = OS_INVALID;
    char request[OS_SIZE_128 + 1];
    size_t size;
    char *found;
    cJSON *obj = NULL;
    cJSON *obj_it;

    size = snprintf(request, OS_SIZE_128, vu_queries[VU_SYSC_OSWIN_INFO], agent->agent_id);

    if (OS_SendSecureTCP(sock, size + 1, request) || OS_RecvSecureTCP(sock, request, size) < 3){
        goto end;
    }

    if (strncmp(request, "ok", 2)) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_INV_WAZUHDB_RES, request);
        goto end;
    }

    request[size] = '\0';
    request[0] = request[1] = ' ';

    // Check empty answers
    if ((found = strchr(request, '[')) && *(++found) != '\0' && *found == ']') {
       retval = 0;
       goto end;
    }

    if (obj = cJSON_Parse(request), !obj) {
        goto end;
    }

    for (obj_it = obj->child; obj_it; obj_it = obj_it->child) {
        if(obj_it->string && strcmp(obj_it->string, "os_release") == 0) {
            os_strdup(obj_it->valuestring, agent->os_release);
            break;
        }
    }

    retval = 0;
end:
    if (obj) {
        cJSON_Delete(obj);
    }
    if (retval) {
        mterror(WM_VULNDETECTOR_LOGTAG, VU_CPE_CLEAN_REQUEST_ERROR, agent->agent_id);
    }

    return retval;
}

int wm_vuldet_index_json(wm_vuldet_db *parsed_vulnerabilities, update_node *update, char *path, char multi_path) {
    cJSON *json_feed = NULL;
    int retval = OS_INVALID;
    char *directory;
    DIR *upd_dir = NULL;
    struct dirent *entry;
    char *path_cpy = NULL;

    if (multi_path) {
        char generated_path[PATH_MAX + 1];
        char *root_path = "/";
        char *pattern;
        regex_t path_pattern;
        regmatch_t match;
        update->update_it = 0;
        os_strdup(path, path_cpy);

        if (!(directory = strrchr(path_cpy, '/')) || *directory != '/') {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_MULTIPATH_ERROR, path_cpy, "it is not an absolute path");
            goto end;
        }

        if (path_cpy == directory) {
            path_cpy = root_path;
        } else {
            *directory = '\0';
        }
        pattern = directory + 1;

        if (upd_dir = opendir(path_cpy), !upd_dir) {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_MULTIPATH_ERROR, path_cpy, strerror(errno));
            goto end;
        }

        if (regcomp(&path_pattern, pattern, 0)) {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_REGEX_COMPILE_ERROR, pattern);
            goto end;
        }

        while (entry = readdir(upd_dir), entry) {
            if ((strcmp(entry->d_name, ".") == 0) ||
                (strcmp(entry->d_name, "..") == 0)) {
                continue;
            }
            snprintf(generated_path, PATH_MAX, "%s/%s", path_cpy, entry->d_name);

            if (w_is_file(generated_path)) {
                if(!regexec(&path_pattern, entry->d_name, 1, &match, 0)) {
                    if (json_feed = wm_vuldet_json_fread(generated_path), !json_feed) {
                        mterror(WM_VULNDETECTOR_LOGTAG, VU_PARSED_FEED_ERROR, update->dist_ext, generated_path);
                        regfree(&path_pattern);
                        goto end;
                    }
                    mtdebug1(WM_VULNDETECTOR_LOGTAG, VU_UPDATE_JSON_FEED, generated_path);
                    if (wm_vuldet_json_parser(json_feed, parsed_vulnerabilities, update) == OS_INVALID) {
                        regfree(&path_pattern);
                        goto end;
                    }
                    cJSON_Delete(json_feed);
                    json_feed = NULL;
                }
            }
        }
        regfree(&path_pattern);
    } else {
        int result;

        if (json_feed = json_fread(path, 0), !json_feed) {
            mterror(WM_VULNDETECTOR_LOGTAG, VU_PARSED_FEED_ERROR, update->dist_ext, path);
            goto end;
        }
        if (result = wm_vuldet_json_parser(json_feed, parsed_vulnerabilities, update), result == OS_INVALID) {
            goto end;
        } else if (result == VU_NOT_NEED_UPDATE) {
            retval = VU_NOT_NEED_UPDATE;
            goto end;
        }
    }

    retval = 0;
end:
    free(path_cpy);

    if (upd_dir) {
        closedir(upd_dir);
    }

    if (json_feed) {
        cJSON_Delete(json_feed);
    }

    return retval;
}

int wm_vuldet_remove_sequence(sqlite3 *db, char *table) {
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db, vu_queries[VU_REMOVE_SQUENCE], -1, &stmt, NULL) != SQLITE_OK) {
        return wm_vuldet_sql_error(db, stmt);
    }

    sqlite3_bind_text(stmt, 1, table, -1, NULL);

    if (wm_vuldet_step(stmt) != SQLITE_DONE) {
        return wm_vuldet_sql_error(db, stmt);
    }
    sqlite3_finalize(stmt);

    return 0;
}

int wm_vuldet_remove_table(sqlite3 *db, char *table) {
    sqlite3_stmt *stmt = NULL;
    char query[OS_SIZE_128 + 1];

    snprintf(query, OS_SIZE_128, vu_queries[DELETE_QUERY], table);
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        return wm_vuldet_sql_error(db, stmt);
    }

    sqlite3_bind_text(stmt, 1, table, -1, NULL);

    if (wm_vuldet_step(stmt) != SQLITE_DONE) {
        return wm_vuldet_sql_error(db, stmt);
    }
    sqlite3_finalize(stmt);

    return wm_vuldet_remove_sequence(db, table);
}

int wm_vuldet_index_nvd(sqlite3 *db, update_node *upd, nvd_vulnerability *nvd_it) {
    time_t index_time = time(NULL);
    int result;
    int cve_count = 0;

    // Clean everything only if there are still residues of an offline update
    if (result = wm_vuldet_has_offline_update(db), result == OS_INVALID) {
        goto error;
    } else if (result == 1 && wm_vuldet_clean_nvd(db)) {
        goto error;
    } else if (!upd->multi_path && !upd->multi_url && wm_vuldet_clean_nvd_year(db, upd->update_it)) {
        goto error;
    }

    while (nvd_it) {
        nvd_vulnerability *r_node = nvd_it;

        if (wm_vuldet_insert_nvd_cve(db, nvd_it, upd->update_it)) {
            goto error;
        }
        nvd_it = nvd_it->next;
        wm_vuldet_free_nvd_node(r_node);
        cve_count++;
    }

    if (wm_vuldet_index_nvd_metadata(db, upd->update_it, cve_count, upd->multi_path || upd->multi_url)) {
        goto error;
    }

    mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_INDEX_TIME, time(NULL) - index_time, "index the NVD");

    return 0;
error:
    while (nvd_it) {
        nvd_vulnerability *r_node = nvd_it;
        nvd_it = nvd_it->next;
        wm_vuldet_free_nvd_node(r_node);
    }

    return OS_INVALID;
}

int wm_vuldet_index_redhat(sqlite3 *db, update_node *upd, rh_vulnerability *r_it) {
    time_t index_time = time(NULL);
    sqlite3_stmt *stmt = NULL;
    int result;

    if (upd->multi_path) {
        // Clean everything at this point because all the insertions will be done at once
        if (wm_vuldet_clean_rh(db)) {
            return OS_INVALID;
        }
    }

    while (r_it) {
        if (sqlite3_prepare_v2(db, vu_queries[VU_INSERT_CVE], -1, &stmt, NULL) != SQLITE_OK) {
            return wm_vuldet_sql_error(db, stmt);
        }

        sqlite3_bind_text(stmt, 1, r_it->cve_id, -1, NULL);
        sqlite3_bind_text(stmt, 2, r_it->OS, -1, NULL);
        sqlite3_bind_text(stmt, 3, r_it->OS_minor, -1, NULL);
        sqlite3_bind_text(stmt, 4, r_it->package_name, -1, NULL);
        sqlite3_bind_int(stmt, 5, 0);
        sqlite3_bind_text(stmt, 6, vu_package_comp[VU_COMP_L], -1, NULL);
        sqlite3_bind_text(stmt, 7, r_it->package_version, -1, NULL);
        sqlite3_bind_text(stmt, 8, NULL, -1, NULL);
        sqlite3_bind_text(stmt, 9, NULL, -1, NULL);

        if (result = wm_vuldet_step(stmt), result != SQLITE_DONE && result != SQLITE_CONSTRAINT) {
            return wm_vuldet_sql_error(db, stmt);
        }
        sqlite3_finalize(stmt);

        rh_vulnerability *rvul_aux = r_it;
        r_it = r_it->prev;
        free(rvul_aux->cve_id);
        free(rvul_aux->package_name);
        free(rvul_aux->package_version);
        free(rvul_aux->OS_minor);
        free(rvul_aux);
    }

    mtdebug2(WM_VULNDETECTOR_LOGTAG, VU_INDEX_TIME, time(NULL) - index_time, "index the Red Hat feed");

    return 0;
}

int wm_vuldet_clean_rh(sqlite3 *db) {
    char open_db = 0;

    if (!db) {
        if (sqlite3_open_v2(CVE_DB, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
            return wm_vuldet_sql_error(db, NULL);
        }

        open_db = 1;
    }

    if (wm_vuldet_remove_target_table(db, CVE_TABLE, vu_feed_tag[FEED_RHEL5])        ||
        wm_vuldet_remove_target_table(db, CVE_TABLE, vu_feed_tag[FEED_RHEL6])        ||
        wm_vuldet_remove_target_table(db, CVE_TABLE, vu_feed_tag[FEED_RHEL7])        ||
        wm_vuldet_remove_target_table(db, METADATA_TABLE, vu_feed_tag[FEED_REDHAT])  ||
        wm_vuldet_remove_target_table(db, CVE_INFO_TABLE, vu_feed_tag[FEED_REDHAT])) {
        return OS_INVALID;
    }
    if (open_db) {
        sqlite3_close_v2(db);
    }
    return 0;
}

int wm_vuldet_clean_wcpe(sqlite3 *db) {
    char open_db = 0;

    if (!db) {
        if (sqlite3_open_v2(CVE_DB, &db, SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK) {
            return wm_vuldet_sql_error(db, NULL);
        }

        open_db = 1;
    }

    if (wm_vuldet_remove_target_table(db, METADATA_TABLE, vu_feed_tag[FEED_CPEW])        ||
        wm_vuldet_remove_table(db, CPEH_SOURCE_TABLE) ||
        wm_vuldet_remove_table(db, CPET_TRANSL_TABLE) ||
        wm_vuldet_remove_table(db, CPE_HELPER_TABLE)) {
        return OS_INVALID;
    }
    if (open_db) {
        sqlite3_close_v2(db);
    }
    return 0;
}

char *wm_vuldet_cpe_str(cpe *cpe_s) {
    char *generated_cpe;

    os_calloc(OS_SIZE_256 + 1, sizeof(char), generated_cpe);

    snprintf(generated_cpe, OS_SIZE_256, "%c:%s:%s:%s:%s:%s:%s:%s:%s:%s:%s",
        *cpe_s->part,
        cpe_s->vendor,
        cpe_s->product,
        cpe_s->version ? cpe_s->version : "",
        cpe_s->update ? cpe_s->update : "",
        cpe_s->edition ? cpe_s->edition : "",
        cpe_s->language ? cpe_s->language : "",
        cpe_s->sw_edition ? cpe_s->sw_edition : "",
        cpe_s->target_sw ? cpe_s->target_sw : "",
        cpe_s->target_hw ? cpe_s->target_hw : "",
        cpe_s->other ? cpe_s->other : "");

    return generated_cpe;
}

cJSON *wm_vuldet_json_fread(char *json_file) {
    cJSON *feed;
    char compress;

    if (wstr_end(json_file, ".gz")) {
        compress = 1;

        if (w_uncompress_gzfile(json_file, VU_TEMP_FILE)) {
            return NULL;
        }
    } else {
        compress = 0;
    }

    feed = json_fread(compress ? VU_TEMP_FILE : json_file, 0);

    if (compress && remove(VU_TEMP_FILE) < 0) {
        mtdebug2(WM_VULNDETECTOR_LOGTAG, "remove(%s): %s", VU_TEMP_FILE, strerror(errno));
    }

    return feed;
}

cJSON *wm_vuldet_get_cvss(const char *scoring_vector) {
    cJSON *cvss_json = NULL;
    char impact;
    const char *vector_it;
    static char *cvss_attack_vector = "AV:";
    static char *cvss_access_complexity = "AC:";
    static char *cvss_authentication = "Au:"; // CVSS2 only
    static char *cvss_confidentiality_impact = "C:";
    static char *cvss_integrity_impact = "I:";
    static char *cvss_availability = "A:";
    static char *cvss_privileges_required = "PR:"; // CVSS3 only
    static char *cvss_user_interaction = "UI:"; // CVSS3 only
    static char *cvss_scope = "S:"; // CVSS3 only

    if (cvss_json = cJSON_CreateObject(), cvss_json) {
        // Attack vector
        if (vector_it = strstr(scoring_vector, cvss_attack_vector), vector_it) {
            impact = *(vector_it + strlen(cvss_attack_vector));
            if (impact == 'L') {
                cJSON_AddStringToObject(cvss_json, "attack_vector", "local");
            } else if (impact == 'A') {
                cJSON_AddStringToObject(cvss_json, "attack_vector", "adjacent_network ");
            } else if (impact == 'N') {
                cJSON_AddStringToObject(cvss_json, "attack_vector", "network");
            } else if (impact == 'P') {
                cJSON_AddStringToObject(cvss_json, "attack_vector", "physical");
            }
        }
        // Access complexity
        if (vector_it = strstr(scoring_vector, cvss_access_complexity), vector_it) {
            impact = *(vector_it + strlen(cvss_access_complexity));
            if (impact == 'L') {
                cJSON_AddStringToObject(cvss_json, "access_complexity", "low");
            } else if (impact == 'M') {
                cJSON_AddStringToObject(cvss_json, "access_complexity", "medium ");
            } else if (impact == 'H') {
                cJSON_AddStringToObject(cvss_json, "access_complexity", "high");
            }
        }
        // Authentication
        if (vector_it = strstr(scoring_vector, cvss_authentication), vector_it) {
            impact = *(vector_it + strlen(cvss_authentication));
            if (impact == 'M') {
                cJSON_AddStringToObject(cvss_json, "authentication", "multiple");
            } else if (impact == 'S') {
                cJSON_AddStringToObject(cvss_json, "authentication", "single ");
            } else if (impact == 'N') {
                cJSON_AddStringToObject(cvss_json, "authentication", "none");
            }
        }
        // Confidentiality impact
        if (vector_it = strstr(scoring_vector, cvss_confidentiality_impact), vector_it) {
            impact = *(vector_it + strlen(cvss_confidentiality_impact));
            if (impact == 'N') {
                cJSON_AddStringToObject(cvss_json, "confidentiality_impact", "none");
            } else if (impact == 'P') {
                cJSON_AddStringToObject(cvss_json, "confidentiality_impact", "partial ");
            } else if (impact == 'C') {
                cJSON_AddStringToObject(cvss_json, "confidentiality_impact", "complete");
            } else if (impact == 'L') {
                cJSON_AddStringToObject(cvss_json, "confidentiality_impact", "low");
            } else if (impact == 'H') {
                cJSON_AddStringToObject(cvss_json, "confidentiality_impact", "high");
            }
        }
        // Integrity impact
        if (vector_it = strstr(scoring_vector, cvss_integrity_impact), vector_it) {
            impact = *(vector_it + strlen(cvss_integrity_impact));
            if (impact == 'N') {
                cJSON_AddStringToObject(cvss_json, "integrity_impact", "none");
            } else if (impact == 'P') {
                cJSON_AddStringToObject(cvss_json, "integrity_impact", "partial ");
            } else if (impact == 'C') {
                cJSON_AddStringToObject(cvss_json, "integrity_impact", "complete");
            } else if (impact == 'L') {
                cJSON_AddStringToObject(cvss_json, "integrity_impact", "low");
            } else if (impact == 'H') {
                cJSON_AddStringToObject(cvss_json, "integrity_impact", "high");
            }
        }
        // Availability
        if (vector_it = strstr(scoring_vector, cvss_availability), vector_it) {
            impact = *(vector_it + strlen(cvss_availability));
            if (impact == 'N') {
                cJSON_AddStringToObject(cvss_json, "availability", "none");
            } else if (impact == 'P') {
                cJSON_AddStringToObject(cvss_json, "availability", "partial ");
            } else if (impact == 'C') {
                cJSON_AddStringToObject(cvss_json, "availability", "complete");
            } else if (impact == 'L') {
                cJSON_AddStringToObject(cvss_json, "availability", "low");
            } else if (impact == 'H') {
                cJSON_AddStringToObject(cvss_json, "availability", "high");
            }
        }
        // Privileges required
        if (vector_it = strstr(scoring_vector, cvss_privileges_required), vector_it) {
            impact = *(vector_it + strlen(cvss_privileges_required));
            if (impact == 'N') {
                cJSON_AddStringToObject(cvss_json, "privileges_required", "none");
            } else if (impact == 'P') {
                cJSON_AddStringToObject(cvss_json, "privileges_required", "partial ");
            } else if (impact == 'C') {
                cJSON_AddStringToObject(cvss_json, "privileges_required", "complete");
            } else if (impact == 'L') {
                cJSON_AddStringToObject(cvss_json, "privileges_required", "low");
            } else if (impact == 'H') {
                cJSON_AddStringToObject(cvss_json, "privileges_required", "high");
            }
        }
        // User interaction
        if (vector_it = strstr(scoring_vector, cvss_user_interaction), vector_it) {
            impact = *(vector_it + strlen(cvss_user_interaction));
            if (impact == 'N') {
                cJSON_AddStringToObject(cvss_json, "user_interaction", "none");
            } else if (impact == 'R') {
                cJSON_AddStringToObject(cvss_json, "user_interaction", "required ");
            }
        }
        // Scope
        if (vector_it = strstr(scoring_vector, cvss_scope), vector_it) {
            impact = *(vector_it + strlen(cvss_scope));
            if (impact == 'U') {
                cJSON_AddStringToObject(cvss_json, "scope", "unchanged");
            } else if (impact == 'C') {
                cJSON_AddStringToObject(cvss_json, "scope", "changed ");
            }
        }
    }

    return cvss_json;
}

int wm_vuldet_get_dist_ref(const char *dist_name, const char *dist_ver, int *dist_ref, int *dist_ver_ref) {
    if (strcasestr(dist_name, vu_feed_tag[FEED_REDHAT]) || strcasestr(dist_name, "red hat")) {
        switch (*dist_ver) {
            case '5':
                *dist_ver_ref = FEED_RHEL5;
                break;
            case '6':
                *dist_ver_ref = FEED_RHEL6;
                break;
            case '7':
                *dist_ver_ref = FEED_RHEL7;
                break;
            default:
                return OS_INVALID;
        }
        *dist_ref = FEED_REDHAT;
        return 0;
    }

    return OS_INVALID;;
}

int wm_vuldet_get_term_condition(char *i_term, char **term, char **comp_field, char **condition) {
    static regex_t *regex = NULL;
    regmatch_t matches[5];

    if (!i_term) {
        return 0;
    }

    if (!regex) {
        const char *pattern = "^([^ ]+)[ ]+\\([ ]*([^ ]+)[ ]+([^ ]+)[ ]+([^ ]+)\\)";
        os_calloc(1, sizeof(regex_t), regex);

        if (regcomp(regex, pattern, REG_EXTENDED)) {
            mterror_exit(WM_VULNDETECTOR_LOGTAG, "Unexpected error when compiling '%s'.", pattern);
        }
    }

    if (!regexec(regex, i_term, 5, matches, 0)) {
        int i;
        char **target;
        char *part = NULL;
        char *op = NULL;
        char *check = NULL;
        char *op_value = NULL;
        size_t size;
        char success = 0;

        for (i = 1; i < 5; i++) {
            int start = (&matches[i])->rm_so;
            int end = (&matches[i])->rm_eo;

            switch (i) {
                case 1:
                    target = &part;
                break;
                case 2:
                    target = &op;
                break;
                case 3:
                    target = &check;
                break;
                case 4:
                    target = &op_value;
                break;
                default:
                    goto free_mem;
            }

            if (!(start >= 0 && end >= 0 && end > start && (unsigned int) (end - start) <= strlen(i_term))) {
                goto free_mem;
            }

            os_strdup(i_term + start, *target);
            (*target)[end - start] = '\0';
        }

        size = strlen(check) + strlen(op_value) + 3;
        os_calloc(size, sizeof(char), *condition);
        snprintf(*condition, size, "%s %s", check, op_value);
        os_strdup(part, *term);
        os_strdup(op, *comp_field);

        success = 1;
free_mem:
        free(part);
        free(op);
        free(check);
        free(op_value);
        return success;
    }

    return 0;
}

cpe *wm_vuldet_generate_cpe(const char *part, const char *vendor, const char *product, const char *version, const char *update, const char *edition, const char *language, const char *sw_edition, const char *target_sw, const char *target_hw, const char *other, const int id, const char *msu_name) {
    cpe *new_cpe;
    os_calloc(1, sizeof(cpe), new_cpe);

    // CPE fields
    w_strdup(part, new_cpe->part);
    w_strdup(vendor, new_cpe->vendor);
    w_strdup(product, new_cpe->product);
    w_strdup(version, new_cpe->version);
    w_strdup(update, new_cpe->update);
    w_strdup(edition, new_cpe->edition);
    w_strdup(language, new_cpe->language);
    w_strdup(sw_edition, new_cpe->sw_edition);
    w_strdup(target_sw, new_cpe->target_sw);
    w_strdup(target_hw, new_cpe->target_hw);
    w_strdup(other, new_cpe->other);

    // Extra fields
    new_cpe->id = id;
    w_strdup(msu_name, new_cpe->msu_name);
    new_cpe->check_hotfix = new_cpe->msu_name ? 1 : 0;
    new_cpe->raw = wm_vuldet_cpe_str(new_cpe);

    return new_cpe;
}

#endif
