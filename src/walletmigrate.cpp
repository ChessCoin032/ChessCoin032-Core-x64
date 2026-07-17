// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletmigrate.h"
#include "util.h"

#include <set>
#include <vector>
#include <string>
#include <stdio.h>

#include <db_cxx.h>      // Berkeley DB (still linked; used to read the legacy file)
#include <sqlite3/sqlite3.h>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

using namespace std;
namespace fs = boost::filesystem;

typedef std::vector<unsigned char> blob;

// "SQLite format 3\000" is the 16-byte magic at the start of every SQLite file.
static bool FileIsSQLite(const fs::path& p)
{
    FILE* f = fopen(p.string().c_str(), "rb");
    if (!f)
        return false;
    char hdr[16] = {0};
    size_t n = fread(hdr, 1, 16, f);
    fclose(f);
    static const char magic[16] = {'S','Q','L','i','t','e',' ','f','o','r','m','a','t',' ','3','\0'};
    return (n == 16 && memcmp(hdr, magic, 16) == 0);
}

// Read every (key,value) record from the legacy BDB wallet into vRecords.
// Opens inside a recovered environment so committed data is fully visible.
static bool ReadAllBdbRecords(const std::string& strWalletFile, std::vector<std::pair<blob,blob> >& vRecords, std::string& strError)
{
    fs::path pathEnv = GetDataDir();

    DbEnv dbenv(DB_CXX_NO_EXCEPTIONS);
    u_int32_t envFlags = DB_CREATE | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL |
                         DB_INIT_TXN | DB_THREAD | DB_RECOVER;
    int ret = dbenv.open(pathEnv.string().c_str(), envFlags, S_IRUSR | S_IWUSR);
    if (ret != 0) {
        strError = strprintf("migration: cannot open BDB environment (%d)", ret);
        return false;
    }

    Db db(&dbenv, 0);
    // ChessCoin/Bitcoin store every wallet record in a BDB sub-database named
    // "main" (see CDB::Open in db.cpp). Opening with a NULL sub-db name returns
    // only the sub-database directory (one entry), NOT the records -- which
    // silently produced an empty migrated wallet. Must match the app: "main".
    ret = db.open(NULL, strWalletFile.c_str(), "main", DB_BTREE, DB_RDONLY | DB_THREAD, 0);
    if (ret != 0) {
        strError = strprintf("migration: cannot open legacy wallet %s as BDB (%d)", strWalletFile.c_str(), ret);
        db.close(0);
        dbenv.close(0);
        return false;
    }

    Dbc* pcursor = NULL;
    if (db.cursor(NULL, &pcursor, 0) != 0 || pcursor == NULL) {
        strError = "migration: cannot create BDB cursor";
        db.close(0);
        dbenv.close(0);
        return false;
    }

    bool fOk = true;
    while (true) {
        Dbt datKey, datValue;
        datKey.set_flags(DB_DBT_MALLOC);
        datValue.set_flags(DB_DBT_MALLOC);
        int cret = pcursor->get(&datKey, &datValue, DB_NEXT);
        if (cret == DB_NOTFOUND)
            break;
        if (cret != 0 || datKey.get_data() == NULL || datValue.get_data() == NULL) {
            strError = strprintf("migration: BDB cursor read error (%d)", cret);
            fOk = false;
            if (datKey.get_data())   free(datKey.get_data());
            if (datValue.get_data()) free(datValue.get_data());
            break;
        }
        const unsigned char* kp = (const unsigned char*)datKey.get_data();
        const unsigned char* vp = (const unsigned char*)datValue.get_data();
        vRecords.push_back(std::make_pair(blob(kp, kp + datKey.get_size()),
                                          blob(vp, vp + datValue.get_size())));
        free(datKey.get_data());
        free(datValue.get_data());
    }

    pcursor->close();
    db.close(0);
    dbenv.close(0);
    return fOk;
}

// Write all records into a fresh SQLite file at pathTmp (absolute path).
static bool WriteSQLite(const fs::path& pathTmp, const std::vector<std::pair<blob,blob> >& vRecords, std::string& strError)
{
    sqlite3* db = NULL;
    if (sqlite3_open_v2(pathTmp.string().c_str(), &db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) != SQLITE_OK || !db) {
        strError = "migration: cannot create temp SQLite file";
        if (db) sqlite3_close(db);
        return false;
    }

    bool fOk = true;
    char* err = NULL;
    // v1.5.4 #3 fix: a one-shot bulk migration has no need for WAL, and WAL
    // creates -wal/-shm sidecar files that get orphaned when we rename the temp
    // file into place. Use the default rollback journal so the temp DB is a
    // single self-contained file; synchronous=FULL keeps the write durable.
    sqlite3_exec(db, "PRAGMA journal_mode=DELETE;", NULL, NULL, &err); if (err) { sqlite3_free(err); err = NULL; }
    sqlite3_exec(db, "PRAGMA synchronous=FULL;",    NULL, NULL, &err); if (err) { sqlite3_free(err); err = NULL; }

    if (sqlite3_exec(db, "CREATE TABLE main(key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL);", NULL, NULL, &err) != SQLITE_OK) {
        strError = strprintf("migration: create table failed: %s", err ? err : "?");
        if (err) sqlite3_free(err);
        sqlite3_close(db);
        return false;
    }

    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(db, "INSERT INTO main(key,value) VALUES(?,?);", -1, &stmt, NULL) != SQLITE_OK) {
        strError = "migration: prepare insert failed";
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        sqlite3_close(db);
        return false;
    }

    for (size_t i = 0; i < vRecords.size() && fOk; i++) {
        const blob& k = vRecords[i].first;
        const blob& v = vRecords[i].second;
        sqlite3_reset(stmt);
        sqlite3_bind_blob(stmt, 1, k.empty() ? (const void*)"" : (const void*)&k[0], (int)k.size(), SQLITE_STATIC);
        sqlite3_bind_blob(stmt, 2, v.empty() ? (const void*)"" : (const void*)&v[0], (int)v.size(), SQLITE_STATIC);
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            // A duplicate key in the source would land here; that should never
            // happen for a healthy BDB btree, so treat it as fatal.
            strError = strprintf("migration: insert failed at record %u (rc=%d)", (unsigned)i, rc);
            fOk = false;
        }
    }
    sqlite3_finalize(stmt);

    if (fOk) {
        if (sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK) {
            strError = "migration: COMMIT failed";
            fOk = false;
        }
    } else {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
    }

    // Rollback-journal mode: COMMIT has already flushed everything into the
    // single temp file. No WAL checkpoint needed, and no sidecars exist.
    sqlite3_close(db);
    return fOk;
}

// Verify the temp SQLite file: integrity_check passes, record count matches,
// and the set of keys is identical to the source.
static bool VerifySQLite(const fs::path& pathTmp, const std::vector<std::pair<blob,blob> >& vRecords, std::string& strError)
{
    sqlite3* db = NULL;
    if (sqlite3_open_v2(pathTmp.string().c_str(), &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK || !db) {
        strError = "migration: cannot reopen temp SQLite for verify";
        if (db) sqlite3_close(db);
        return false;
    }

    bool fOk = true;
    sqlite3_stmt* stmt = NULL;

    // integrity_check
    if (sqlite3_prepare_v2(db, "PRAGMA integrity_check;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* res = sqlite3_column_text(stmt, 0);
            if (!res || std::string((const char*)res) != "ok") {
                strError = "migration: SQLite integrity_check failed";
                fOk = false;
            }
        } else {
            strError = "migration: integrity_check returned no row";
            fOk = false;
        }
    }
    if (stmt) { sqlite3_finalize(stmt); stmt = NULL; }

    // key set + count
    if (fOk) {
        std::set<blob> srcKeys;
        for (size_t i = 0; i < vRecords.size(); i++)
            srcKeys.insert(vRecords[i].first);

        std::set<blob> dstKeys;
        if (sqlite3_prepare_v2(db, "SELECT key FROM main;", -1, &stmt, NULL) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const void* kp = sqlite3_column_blob(stmt, 0);
                int klen = sqlite3_column_bytes(stmt, 0);
                dstKeys.insert(blob((const unsigned char*)kp, (const unsigned char*)kp + klen));
            }
        }
        if (stmt) { sqlite3_finalize(stmt); stmt = NULL; }

        if (dstKeys.size() != srcKeys.size()) {
            strError = strprintf("migration: key count mismatch (src=%u dst=%u)",
                                 (unsigned)srcKeys.size(), (unsigned)dstKeys.size());
            fOk = false;
        } else if (dstKeys != srcKeys) {
            strError = "migration: key set mismatch between source and destination";
            fOk = false;
        }
    }

    sqlite3_close(db);
    return fOk;
}

WalletMigrateResult MigrateWalletToSQLite(const std::string& strWalletFile, std::string& strError)
{
    strError.clear();
    fs::path pathWallet = GetDataDir() / strWalletFile;

    // (1) Nothing to migrate.
    if (!fs::exists(pathWallet)) {
        return WMIGRATE_NONE;                 // fresh start; SQLite wallet created on first open
    }
    if (FileIsSQLite(pathWallet)) {
        // Already migrated. Sweep up any orphaned temp sidecars a prior
        // (pre-fix) migration run may have left behind; they are harmless junk
        // named after the temp file, not the live wallet, but should not linger.
        {
            boost::system::error_code ec;
            fs::path pathTmpOld = GetDataDir() / (strWalletFile + ".sqlite.tmp");
            fs::remove(pathTmpOld, ec);
            fs::remove(fs::path(pathTmpOld.string() + "-wal"), ec);
            fs::remove(fs::path(pathTmpOld.string() + "-shm"), ec);
        }
        return WMIGRATE_NONE;                 // already migrated
    }

    printf("Wallet migration: legacy Berkeley DB wallet detected, converting %s to SQLite...\n", strWalletFile.c_str());

    // (2) Read every record from the legacy BDB wallet.
    std::vector<std::pair<blob,blob> > vRecords;
    if (!ReadAllBdbRecords(strWalletFile, vRecords, strError))
        return WMIGRATE_ERROR;
    if (vRecords.empty()) {
        strError = "migration: legacy wallet contained no records (refusing to proceed)";
        return WMIGRATE_ERROR;
    }
    printf("Wallet migration: read %u records from legacy wallet.\n", (unsigned)vRecords.size());

    // (3) Write to a temp SQLite file.
    fs::path pathTmp = GetDataDir() / (strWalletFile + ".sqlite.tmp");
    if (fs::exists(pathTmp)) {
        boost::system::error_code ec;
        fs::remove(pathTmp, ec);
        // also clear any stray WAL/SHM siblings
        fs::remove(fs::path(pathTmp.string() + "-wal"), ec);
        fs::remove(fs::path(pathTmp.string() + "-shm"), ec);
    }
    if (!WriteSQLite(pathTmp, vRecords, strError)) {
        boost::system::error_code ec;
        fs::remove(pathTmp, ec);
        return WMIGRATE_ERROR;
    }

    // (4) Verify before touching the original.
    if (!VerifySQLite(pathTmp, vRecords, strError)) {
        boost::system::error_code ec;
        fs::remove(pathTmp, ec);
        fs::remove(fs::path(pathTmp.string() + "-wal"), ec);
        fs::remove(fs::path(pathTmp.string() + "-shm"), ec);
        return WMIGRATE_ERROR;
    }
    printf("Wallet migration: verified %u records.\n", (unsigned)vRecords.size());

    // (5) Atomic swap. Original preserved as a timestamped backup; both renames
    //     target paths that do not yet exist.
    std::string strStamp = DateTimeStrFormat("%Y%m%dT%H%M%SZ", GetTime());
    fs::path pathBak = GetDataDir() / (strWalletFile + "." + strStamp + ".bdb.bak");

    boost::system::error_code ec;
    fs::rename(pathWallet, pathBak, ec);
    if (ec) {
        strError = strprintf("migration: could not back up original wallet to %s (%s)",
                             pathBak.string().c_str(), ec.message().c_str());
        fs::remove(pathTmp, ec);
        return WMIGRATE_ERROR;
    }
    fs::rename(pathTmp, pathWallet, ec);
    if (ec) {
        // Roll the original back into place so the user is never left without a wallet.
        boost::system::error_code ec2;
        fs::rename(pathBak, pathWallet, ec2);
        strError = strprintf("migration: could not move new SQLite wallet into place (%s)", ec.message().c_str());
        return WMIGRATE_ERROR;
    }

    // Belt-and-suspenders: ensure no temp sidecars survive the swap. With
    // rollback-journal mode none are created, but a file left by an older build
    // (or an interrupted run) is cleaned here.
    {
        boost::system::error_code ec3;
        fs::remove(fs::path(pathTmp.string() + "-wal"), ec3);
        fs::remove(fs::path(pathTmp.string() + "-shm"), ec3);
    }

    printf("Wallet migration: complete. Original Berkeley DB wallet kept as %s\n", pathBak.filename().string().c_str());
    return WMIGRATE_DONE;
}
