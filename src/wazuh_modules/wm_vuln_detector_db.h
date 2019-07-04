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

#ifndef WM_VUNALIZER_DB
#define WM_VUNALIZER_DB

#define CVE_DBS_PATH            "queue/vulnerabilities/"
#define CVE_DB CVE_DBS_PATH     "cve.db"

#define AGENTS_TABLE            "AGENTS"
#define CVE_TABLE               "VULNERABILITIES"
#define CVE_INFO_TABLE          "VULNERABILITIES_INFO"
#define INFO_STATE_TABLE        "INFO_STATE"
#define METADATA_TABLE          "METADATA"
#define METADATADB_TABLE        "DB_METADATA"
#define CPEH_SOURCE_TABLE       "CPE_HELPER_SOURCE"
#define CPET_TRANSL_TABLE       "CPE_HELPER_TRANSLATION"
#define CPE_HELPER_TABLE        "CPE_HELPER"
#define VARIABLES_TABLE         "VARIABLES"
#define MAX_QUERY_SIZE          OS_SIZE_1024
#define MAX_SQL_ATTEMPTS        1000
#define SQL_BUSY_SLEEP_MS       1
#define VU_MAX_PACK_REQ         20

typedef enum vu_query {
    SELECT_QUERY,
    DELETE_QUERY,
    VU_CHECK_UPDATES,
    TIMESTAMP_QUERY,
    VU_INSERT_QUERY,
    VU_INSERT_CVE,
    VU_INSERT_CVE_INFO,
    VU_INSERT_METADATA,
    VU_INSERT_METADATA_DB,
    VU_INSERT_AGENTS,
    VU_INSERT_AGENT_HOTFIXES,
    VU_INSERT_VARIABLES,
    VU_UPDATE_CVE,
    VU_UPDATE_CVE_VAL,
    VU_UPDATE_CVE_PACK,
    VU_JOIN_QUERY,
    VU_JOIN_RH_QUERY,
    VU_REMOVE_OS,
    VU_REMOVE_AGENTS_TABLE,
    VU_REMOVE_HOTFIXES_TABLE,
    VU_REMOVE_PATCH,
    VU_GLOBALDB_REQUEST,
    VU_REMOVE_UNUSED_VULS,
    // WAZUH DB REQUESTS
    VU_HOTFIXES_REQUEST,
    VU_SOFTWARE_REQUEST,
    VU_SOFTWARE_FULL_REQ,
    VU_SYSC_SCAN_REQUEST,
    VU_SYSC_UPDATE_SCAN,
    VU_SYSC_UPDATE_CPE,
    VU_SYSC_CLEAN_CPES,
    VU_SYSC_OSWIN_INFO,
    // CPE INDEX
    VU_INSERT_CPE,
    VU_REMOVE_CPE,
    VU_REMOVE_AGENT_CPE,
    VU_SEARCH_AGENT_CPE,
    VU_MIN_CPEINDEX,
    VU_GET_PACK_WITHOUT_CPE,
    VU_GET_AGENT_CPES,
    VU_UPDATE_AGENT_CPE,
    // NVD
    VU_GET_NVD_LASTMOD,
    VU_REP_NVD_METADATA,
    VU_INSERT_NVD_CVE,
    VU_GET_MAX_NVD_CVE_ID,
    VU_INSERT_NVD_METRIC_CVSS,
    VU_INSERT_NVD_REFERENCE,
    VU_INSERT_NVD_CVE_CONFIGURATION,
    VU_GET_MAX_CONFIGURATION_ID,
    VU_INSERT_NVD_CVE_MATCHES,
    VU_INSERT_NVD_CPE,
    VU_GET_AN_CPE_ID,
    VU_GET_MAX_NVD_CPE_ID,
    VU_GET_OFFLINE_UPDATE,
    // NVD CLEAN
    VU_REMOVE_NVD_METADATA,
    VU_GET_NVD_CVE_YEAR,
    VU_REMOVE_CVE_TABLE_REFS,
    VU_REMOVE_CVE_MATCHES,
    VU_REMOVE_NVD_CPE,
    VU_REMOVE_NVD_CVE,
    VU_GET_NVD_CONFIG,
    // NVD REPORT
    VU_GET_CVE_INFO,
    VU_GET_CVE,
    VU_GET_REFERENCE,
    VU_GET_SCORING,
    // NVD VULNERABILITY CHECK
    VU_GET_DICT_CPE,
    VU_GET_NVD_CPE,
    VU_GET_NVD_MATCHES,
    VU_GET_CONF,
    VU_GET_CONF_AND,
    VU_GET_MATCHES_AND,
    VU_GET_CPE_AND,
    VU_GET_AGENTCPE_AND,
    // SQL OPERATIONS
    VU_REMOVE_SQUENCE,
    // WAZUH CPE DICTIONARY
    VU_REMOVE_CPE_DIC,
    VU_INSERT_CPE_HELPER,
    VU_INSERT_CPE_SOURCE,
    VU_INSERT_CPE_TRANSLATION,
    VU_GET_DIC_MATCHES,
    VU_GET_EXACT_TERM,
    VU_GET_TRANSLATION_TERM,
    // MSU
    VU_REMOVE_MSU,
    VU_INSERT_MSU,
    VU_NECESSARY_HOTFX,
    VU_NECESSARY_HOTFX_WITHOUT_R2,
    VU_CHECK_AGENT_HOTFIX,
    // TRANSACTIONS
    BEGIN_T,
    END_T
} vu_query;

static const char *vu_queries[] = {
    "SELECT %s FROM %s WHERE %s;",
    "DELETE FROM %s;",
    "SELECT COUNT(*) FROM METADATA;",
    "SELECT TIMESTAMP FROM " METADATA_TABLE " WHERE TARGET = ?;",
    "INSERT INTO ",
    "INSERT INTO " CVE_TABLE " VALUES(?,?,?,?,?,?,?,?);",
    "INSERT INTO " CVE_INFO_TABLE " VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
    "INSERT INTO " METADATA_TABLE " VALUES(?,?,?,?,?);",
    "INSERT INTO " METADATADB_TABLE " VALUES(?);",
    "INSERT INTO " AGENTS_TABLE " VALUES(?,?,?,?,?,?,?,?);",
    "INSERT INTO AGENT_HOTFIXES VALUES(?,?);",
    "INSERT INTO " VARIABLES_TABLE " VALUES(?,?,?);",
    "UPDATE " CVE_TABLE " SET PACKAGE = ?, OPERATION = ? WHERE OPERATION = ?;",
    "UPDATE " CVE_TABLE " SET OPERATION = ?, OPERATION_VALUE = ? WHERE OPERATION = ?;",
    "UPDATE " CVE_TABLE " SET PACKAGE = ?, CHECK_VARS = ? WHERE PACKAGE = ?;",
    "SELECT ID, (CASE WHEN VALUE IS NOT NULL THEN VALUE ELSE PACKAGE_NAME END), TITLE, SEVERITY, PUBLISHED, UPDATED, REFERENCE, RATIONALE, VERSION, ARCH, OPERATION, OPERATION_VALUE, PENDING, CVSS, CVSS3 FROM " CVE_INFO_TABLE " INNER JOIN " CVE_TABLE " ON ID = CVEID AND " CVE_INFO_TABLE ".TARGET = " CVE_TABLE ".TARGET LEFT JOIN VARIABLES ON VARIABLES.ID = VULNERABILITIES.PACKAGE AND VARIABLES.OS = VULNERABILITIES.OS INNER JOIN " AGENTS_TABLE " ON PACKAGE_NAME = (CASE WHEN VALUE IS NOT NULL THEN VALUE ELSE PACKAGE END) WHERE VULNERABILITIES_INFO.TARGET = ? AND AGENT_ID = ? ORDER BY ID, PACKAGE_NAME, VERSION, ARCH ORDER BY CVEID, PACKAGE_NAME, VERSION, ARCH;",
    "SELECT ID, PACKAGE_NAME, TITLE, SEVERITY, PUBLISHED, UPDATED, REFERENCE, RATIONALE, VERSION, ARCH, OPERATION, OPERATION_VALUE, PENDING, CVSS, CVSS3, CVSS_VECTOR, BUGZILLA_REFERENCE, CWE, ADVISORIES FROM VULNERABILITIES_INFO INNER JOIN VULNERABILITIES ON ID = CVEID AND VULNERABILITIES_INFO.TARGET = 'REDHAT' AND (VULNERABILITIES.TARGET = ? OR VULNERABILITIES.TARGET = ? OR VULNERABILITIES.TARGET = ?) INNER JOIN AGENTS ON AGENT_ID = ? AND PACKAGE_NAME = PACKAGE AND (VULNERABILITIES.TARGET = TARGET_MAJOR OR VULNERABILITIES.TARGET IS NULL OR (TARGET_MAJOR IS NULL AND VULNERABILITIES.TARGET IS ?)) AND (VULNERABILITIES.TARGET_MINOR = AGENTS.TARGET_MINOR OR VULNERABILITIES.TARGET_MINOR IS NULL OR AGENTS.TARGET_MINOR IS NULL) ORDER BY ID, PACKAGE_NAME, VERSION, ARCH;",
    "DELETE FROM %s WHERE TARGET = ?;",
    "DELETE FROM " AGENTS_TABLE ";",
    "DELETE FROM AGENT_HOTFIXES;",
    "DELETE FROM " CVE_TABLE " WHERE CVEID = ?;",
    "SELECT OS_NAME, OS_MAJOR, NAME, ID, IP, REGISTER_IP, OS_ARCH, OS_BUILD FROM AGENT WHERE (STRFTIME('%s', 'NOW', 'LOCALTIME') - STRFTIME('%s', LAST_KEEPALIVE)) < ?;",
    "DELETE FROM " CVE_TABLE " WHERE PACKAGE LIKE '%:tst:%';",
    // WAZUH DB REQUESTS
    "agent %s sql SELECT HOTFIX FROM SYS_HOTFIXES;",
    "agent %s sql SELECT DISTINCT NAME, VERSION, ARCHITECTURE, VENDOR, CPE, MSU_NAME FROM SYS_PROGRAMS WHERE TRIAGED != 1 AND SCAN_ID = '%s' LIMIT %i OFFSET %i;",
    "agent %s sql SELECT DISTINCT NAME, VERSION, ARCHITECTURE, VENDOR, CPE, MSU_NAME FROM SYS_PROGRAMS WHERE SCAN_ID = '%s' LIMIT %i OFFSET %i;",
    "agent %s sql SELECT SCAN_ID FROM SYS_PROGRAMS WHERE SCAN_TIME = (SELECT SCAN_TIME FROM SYS_PROGRAMS S1 WHERE NOT EXISTS (SELECT SCAN_TIME FROM SYS_PROGRAMS S2 WHERE S2.SCAN_TIME > S1.SCAN_TIME)) LIMIT 1;",
    "agent %s sql UPDATE SYS_PROGRAMS SET TRIAGED = 1 WHERE SCAN_ID = '%s';",
    "agent %s sql UPDATE SYS_PROGRAMS SET CPE = '%s:%s:%s:%s:%s:%s:%s:%s:%s:%s:%s', MSU_NAME = '%s' WHERE VENDOR = '%s' AND NAME = '%s' AND VERSION = '%s' AND ARCHITECTURE = '%s';",
    "agent %s sql UPDATE SYS_PROGRAMS SET CPE = NULL, MSU_NAME = NULL;",
    "agent %s sql SELECT OS_RELEASE FROM SYS_OSINFO;",
    // CPE INDEX
    "INSERT INTO CPE_INDEX VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
    "DELETE FROM CPE_INDEX;",
    "DELETE FROM CPE_INDEX WHERE ID < 0;",
    "SELECT DISTINCT PART FROM CPE_INDEX WHERE PART = 'a' AND VENDOR = ? AND PRODUCT = ?;",
    "SELECT MIN(ID) FROM CPE_INDEX;",
    "SELECT VENDOR, PACKAGE_NAME, VERSION, ARCH FROM AGENTS WHERE AGENT_ID = ? AND CPE_INDEX_ID = 0;",
    "SELECT PART, CPE_INDEX.VENDOR, PRODUCT, CPE_INDEX.VERSION, UPDATEV, EDITION, LANGUAGE, SW_EDITION, TARGET_SW, TARGET_HW, OTHER, MSU_NAME, AGENTS.VENDOR, PACKAGE_NAME, AGENTS.VERSION, ARCH FROM AGENTS JOIN CPE_INDEX ON CPE_INDEX_ID = ID WHERE AGENT_ID = ?;",
    "UPDATE AGENTS SET CPE_INDEX_ID = ? WHERE AGENT_ID = ? AND VENDOR IS ? AND PACKAGE_NAME = ? AND VERSION = ? AND ARCH = ?;",
    // NVD
    "SELECT LAST_MODIFIED FROM NVD_METADATA WHERE YEAR = ?;",
    "REPLACE INTO NVD_METADATA VALUES(?,?,?,?,?,?,?,?);",
    "INSERT INTO NVD_CVE VALUES(NULL,?,?,?,?,?,?);",
    "SELECT MAX(ID) FROM NVD_CVE;",
    "INSERT INTO NVD_METRIC_CVSS VALUES(NULL,?,?,?,?,?,?);",
    "INSERT INTO NVD_REFERENCE VALUES(NULL,?,?,?);",
    "INSERT INTO NVD_CVE_CONFIGURATION VALUES(NULL,?,?,?);",
    "SELECT MAX(ID) FROM NVD_CVE_CONFIGURATION;",
    "INSERT INTO NVD_CVE_MATCH VALUES(NULL,?,?,?,?,?,?,?,?);",
    "INSERT INTO NVD_CPE VALUES(?,?,?,?,?,?,?,?,?,?,?,?);",
    "SELECT ID FROM NVD_CPE WHERE PART = ? AND VENDOR = ? AND PRODUCT = ? AND VERSION = ? AND UPDATED = ? AND EDITION = ? AND LANGUAGE = ? AND SW_EDITION = ? AND TARGET_SW = ? AND TARGET_HW = ? AND OTHER = ?;",
    "SELECT MAX(ID) FROM NVD_CPE;",
    "SELECT * FROM NVD_METADATA WHERE ALTERNATIVE = 1;",
    // NVD CLEAN
    "DELETE FROM NVD_METADATA WHERE YEAR = ?;",
    "SELECT ID FROM NVD_CVE WHERE NVD_METADATA_YEAR = ?;",
    "DELETE FROM NVD_METRIC_CVSS WHERE NVD_CVE_ID = ?; DELETE FROM NVD_REFERENCE WHERE NVD_CVE_ID = ?; DELETE FROM NVD_CVE_CONFIGURATION WHERE NVD_CVE_ID = ?;",
    "DELETE FROM NVD_CVE_MATCH WHERE NVD_CVE_CONFIGURATION_ID = ?;",
    "DELETE FROM NVD_CPE WHERE ID NOT IN (SELECT DISTINCT ID_CPE FROM NVD_CVE_MATCH);",
    "DELETE FROM NVD_CVE WHERE NVD_METADATA_YEAR = ?;",
    "SELECT ID FROM NVD_CVE_CONFIGURATION WHERE NVD_CVE_ID = ?;",
    // NVD REPORT
    "SELECT CVE_ID, CWE_ID, DESCRIPTION, PUBLISHED, LAST_MODIFIED FROM NVD_CVE WHERE ID = ?;",
    "SELECT CVE_ID FROM NVD_CVE WHERE ID = ?;",
    "SELECT URL, REF_SOURCE FROM NVD_REFERENCE WHERE NVD_CVE_ID = ? LIMIT 1;",
    "SELECT VECTOR_STRING, BASE_SCORE, EXPLOITABILITY_SCORE, IMPACT_SCORE, VERSION FROM NVD_METRIC_CVSS WHERE NVD_CVE_ID = ?;",
    // NVD VULNERABILITY CHECK
    "SELECT CPE_INDEX.PART, CPE_INDEX.VENDOR, CPE_INDEX.PRODUCT, CPE_INDEX.VERSION, CPE_INDEX.UPDATEV, CPE_INDEX.EDITION, CPE_INDEX.LANGUAGE, CPE_INDEX.SW_EDITION, CPE_INDEX.TARGET_SW, CPE_INDEX.TARGET_HW, CPE_INDEX.MSU_NAME, AGENTS.PACKAGE_NAME, AGENTS.VERSION, AGENTS.ARCH FROM AGENTS INNER JOIN CPE_INDEX ON AGENTS.AGENT_ID = ? AND AGENTS.CPE_INDEX_ID < 0 AND CPE_INDEX.ID = AGENTS.CPE_INDEX_ID;",
    "SELECT NVD_CPE.ID FROM NVD_CPE WHERE NVD_CPE.PART = ? AND NVD_CPE.VENDOR = ? AND NVD_CPE.PRODUCT = ? AND (NVD_CPE.VERSION = ? OR NVD_CPE.VERSION = '*' OR NVD_CPE.VERSION = '-') AND (NVD_CPE.UPDATED = '*' OR NVD_CPE.UPDATED = ?) AND (NVD_CPE.EDITION = '*' OR NVD_CPE.EDITION = ? OR NVD_CPE.EDITION = '') AND (NVD_CPE.LANGUAGE = '*' OR NVD_CPE.LANGUAGE = ? OR NVD_CPE.LANGUAGE = '') AND (NVD_CPE.SW_EDITION = '*' OR NVD_CPE.SW_EDITION = ? OR NVD_CPE.SW_EDITION = '') AND (NVD_CPE.TARGET_SW = '*' OR NVD_CPE.TARGET_SW = '-' OR NVD_CPE.TARGET_SW = '') AND (NVD_CPE.TARGET_HW = '*' OR NVD_CPE.TARGET_HW = '-' OR NVD_CPE.TARGET_HW = ?);",
    "SELECT NVD_CVE_MATCH.NVD_CVE_CONFIGURATION_ID, NVD_CVE_MATCH.URI, NVD_CVE_MATCH.VULNERABLE, NVD_CVE_MATCH.VERSION_START_INCLUDING, NVD_CVE_MATCH.VERSION_START_EXCLUDING, NVD_CVE_MATCH.VERSION_END_INCLUDING, NVD_CVE_MATCH.VERSION_END_EXCLUDING FROM NVD_CVE_MATCH WHERE NVD_CVE_MATCH.ID_CPE = ?;",
    "SELECT NVD_CVE_CONFIGURATION.NVD_CVE_ID, NVD_CVE_CONFIGURATION.OPERATOR, NVD_CVE_CONFIGURATION.PARENT FROM NVD_CVE_CONFIGURATION WHERE NVD_CVE_CONFIGURATION.ID = ?;",
    "SELECT ID FROM NVD_CVE_CONFIGURATION WHERE PARENT=(SELECT ID FROM NVD_CVE_CONFIGURATION WHERE ID=? AND OPERATOR='AND') AND ID!=?;",
    "SELECT ID FROM NVD_CVE_MATCH WHERE NVD_CVE_CONFIGURATION_ID = ?;",
    "SELECT VENDOR, PRODUCT FROM NVD_CPE WHERE ID = ?;",
    "SELECT CPEI.ID INNER JOIN CPE_INDEX CPEI ON CPEI.VENDOR=? AND CPEI.PRODUCT=? AND CPEI.ID<0 INNER JOIN AGENTS AG ON AG.CPE_INDEX_ID = CPEI.ID AND AG.AGENT_ID=?;",
    // SQL OPERATIONS
    "DELETE FROM SQLITE_SEQUENCE WHERE NAME = ?;",
    // WAZUH CPE DICTIONARY
    "DELETE FROM CPE_HELPER; DELETE FROM CPE_HELPER_SOURCE; DELETE FROM CPE_HELPER_TRANSLATION;",
    "INSERT INTO CPE_HELPER VALUES(?,?,?);",
    "INSERT INTO CPE_HELPER_SOURCE VALUES(?,?,?,?);",
    "INSERT INTO CPE_HELPER_TRANSLATION VALUES(?,?,?,?,?,?);",
    "SELECT DISTINCT VENDOR, PACKAGE_NAME, VERSION, ARCH, ID, ACTION FROM AGENTS INNER JOIN CPE_HELPER_SOURCE T1 ON AGENT_ID = ? AND T1.TYPE = 'vendor' AND (VENDOR REGEXP T1.TERM OR (T1.TERM IS NULL AND VENDOR IS NULL)) INNER JOIN CPE_HELPER ON ID = T1.ID_HELPER INNER JOIN CPE_HELPER_SOURCE T2 ON ID = T2.ID_HELPER AND T2.TYPE = 'product' AND PACKAGE_NAME REGEXP T2.TERM;",
    "SELECT TERM, CORRELATION_ID FROM CPE_HELPER_SOURCE WHERE ID_HELPER = ? AND TYPE = ? AND CASE WHEN ? = '' THEN TERM ELSE ? END REGEXP TERM ORDER BY CORRELATION_ID;",
    "SELECT TERM, COMPARE_FIELD, CONDITION FROM CPE_HELPER_TRANSLATION WHERE ID_HELPER = ? AND TYPE = ? AND CORRELATION_ID = ?;",
    // MSU
    "DELETE FROM MSU;",
    "INSERT INTO MSU VALUES(?,?,?,?,?,?,?);",
    "SELECT DISTINCT PATCH FROM MSU WHERE CVEID = ? AND PRODUCT REGEXP ?;",
    "SELECT DISTINCT PATCH FROM MSU WHERE CVEID = ? AND PRODUCT REGEXP ? AND NOT PRODUCT REGEXP ' R2 ';",
    "SELECT HOTFIX FROM AGENT_HOTFIXES WHERE AGENT_ID = ? AND HOTFIX REGEXP ?;",
    // TRANSACTIONS
    "BEGIN TRANSACTION;",
    "END TRANSACTION;"
};

extern char *schema_vuln_detector_sql;

#endif
