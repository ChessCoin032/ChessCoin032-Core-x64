// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// v1.5.4 point #3: SQLite wallet backend. See sqlitewallet.h for the model.

#include "sqlitewallet.h"
#include "util.h"

#include <sqlite3/sqlite3.h>

#include <boost/filesystem/operations.hpp>

using namespace std;

CSQLiteEnv sqlitedb;

// The single table that holds every wallet record as raw serialized blobs.
static const char* SQL_CREATE_MAIN =
    "CREATE TABLE IF NOT EXISTS main(key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL);";

// ---------------------------------------------------------------------------
// CSQLiteEnv
// ---------------------------------------------------------------------------

CSQLiteEnv::CSQLiteEnv() {}

CSQLiteEnv::~CSQLiteEnv()
{
    LOCK(cs_sqlite);
    for (map<string, Handle>::iterator it = mapDb.begin(); it != mapDb.end(); ++it) {
        if (it->second.db) {
            sqlite3_wal_checkpoint_v2(it->second.db, NULL, SQLITE_CHECKPOINT_TRUNCATE, NULL, NULL);
            sqlite3_close(it->second.db);
        }
    }
    mapDb.clear();
}

boost::filesystem::path CSQLiteEnv::Path(const std::string& strFile) const
{
    return GetDataDir() / strFile;
}

sqlite3* CSQLiteEnv::Open(const std::string& strFile)
{
    LOCK(cs_sqlite);

    map<string, Handle>::iterator it = mapDb.find(strFile);
    if (it != mapDb.end() && it->second.db != NULL) {
        it->second.refcount++;
        return it->second.db;
    }

    sqlite3* db = NULL;
    std::string strPath = Path(strFile).string();
    int ret = sqlite3_open_v2(strPath.c_str(), &db,
                              SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
    if (ret != SQLITE_OK || db == NULL) {
        if (db) sqlite3_close(db);
        printf("CSQLiteEnv::Open() : sqlite3_open_v2 failed for %s (%d)\n", strFile.c_str(), ret);
        return NULL;
    }

    // Durability first (this whole migration exists to stop unclean-shutdown
    // corruption): WAL + synchronous=FULL. foreign_keys off (we use one table).
    sqlite3_busy_timeout(db, 30000);
    char* err = NULL;
    if (sqlite3_exec(db, "PRAGMA journal_mode=WAL;",   NULL, NULL, &err) != SQLITE_OK) { if (err) sqlite3_free(err); }
    if (sqlite3_exec(db, "PRAGMA synchronous=FULL;",   NULL, NULL, &err) != SQLITE_OK) { if (err) sqlite3_free(err); }
    if (sqlite3_exec(db, SQL_CREATE_MAIN,              NULL, NULL, &err) != SQLITE_OK) {
        printf("CSQLiteEnv::Open() : create table failed: %s\n", err ? err : "?");
        if (err) sqlite3_free(err);
        sqlite3_close(db);
        return NULL;
    }

    Handle h;
    h.db = db;
    h.refcount = 1;
    mapDb[strFile] = h;
    return db;
}

void CSQLiteEnv::Close(const std::string& strFile)
{
    LOCK(cs_sqlite);
    map<string, Handle>::iterator it = mapDb.find(strFile);
    if (it == mapDb.end())
        return;
    if (--it->second.refcount > 0)
        return;
    if (it->second.db) {
        sqlite3_wal_checkpoint_v2(it->second.db, NULL, SQLITE_CHECKPOINT_TRUNCATE, NULL, NULL);
        sqlite3_close(it->second.db);
    }
    mapDb.erase(it);
}

void CSQLiteEnv::Flush(const std::string& strFile)
{
    LOCK(cs_sqlite);
    map<string, Handle>::iterator it = mapDb.find(strFile);
    if (it != mapDb.end() && it->second.db)
        sqlite3_wal_checkpoint_v2(it->second.db, NULL, SQLITE_CHECKPOINT_TRUNCATE, NULL, NULL);
}

bool CSQLiteEnv::IntegrityCheck(const std::string& strFile)
{
    std::string strPath = (GetDataDir() / strFile).string();
    sqlite3* db = NULL;
    if (sqlite3_open_v2(strPath.c_str(), &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK || !db) {
        if (db) sqlite3_close(db);
        return false;
    }
    bool fOk = false;
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(db, "PRAGMA integrity_check;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* res = sqlite3_column_text(stmt, 0);
            fOk = (res != NULL && std::string((const char*)res) == "ok");
        }
    }
    if (stmt) sqlite3_finalize(stmt);
    sqlite3_close(db);
    return fOk;
}

// ---------------------------------------------------------------------------
// CSQLiteCursor
// ---------------------------------------------------------------------------

void CSQLiteCursor::close()
{
    if (stmt) {
        sqlite3_finalize(stmt);
        stmt = NULL;
    }
    fStarted = false;
}

// ---------------------------------------------------------------------------
// CSQLiteDB
// ---------------------------------------------------------------------------

CSQLiteDB::CSQLiteDB(const char* pszFile, const char* pszMode)
    : pdb(NULL), strFile(pszFile), fReadOnly(true), fInTxn(false)
{
    if (pszFile == NULL)
        return;
    fReadOnly = (!strchr(pszMode, '+') && !strchr(pszMode, 'w'));
    pdb = sqlitedb.Open(strFile);
    if (pdb == NULL)
        throw std::runtime_error(strprintf("CSQLiteDB() : can't open database file %s", pszFile));
}

void CSQLiteDB::Close()
{
    if (!pdb)
        return;
    if (fInTxn)
        TxnAbort();
    sqlitedb.Close(strFile);
    pdb = NULL;
}

bool CSQLiteDB::ReadBlob(const std::vector<unsigned char>& key, std::vector<unsigned char>& value)
{
    LOCK(sqlitedb.cs_sqlite);
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(pdb, "SELECT value FROM main WHERE key=?;", -1, &stmt, NULL) != SQLITE_OK)
        return false;
    bool fFound = false;
    sqlite3_bind_blob(stmt, 1, key.empty() ? (const void*)"" : (const void*)&key[0], (int)key.size(), SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* data = sqlite3_column_blob(stmt, 0);
        int len = sqlite3_column_bytes(stmt, 0);
        value.assign((const unsigned char*)data, (const unsigned char*)data + len);
        fFound = true;
    }
    sqlite3_finalize(stmt);
    return fFound;
}

bool CSQLiteDB::WriteBlob(const std::vector<unsigned char>& key, const std::vector<unsigned char>& value, bool fOverwrite)
{
    LOCK(sqlitedb.cs_sqlite);
    const char* sql = fOverwrite
        ? "INSERT INTO main(key,value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value;"
        : "INSERT INTO main(key,value) VALUES(?,?);";
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(pdb, sql, -1, &stmt, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_blob(stmt, 1, key.empty()   ? (const void*)"" : (const void*)&key[0],   (int)key.size(),   SQLITE_STATIC);
    sqlite3_bind_blob(stmt, 2, value.empty() ? (const void*)"" : (const void*)&value[0], (int)value.size(), SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    // With fOverwrite==false a duplicate key yields SQLITE_CONSTRAINT (matches
    // BDB's DB_NOOVERWRITE returning failure).
    return rc == SQLITE_DONE;
}

bool CSQLiteDB::EraseBlob(const std::vector<unsigned char>& key)
{
    LOCK(sqlitedb.cs_sqlite);
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(pdb, "DELETE FROM main WHERE key=?;", -1, &stmt, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_blob(stmt, 1, key.empty() ? (const void*)"" : (const void*)&key[0], (int)key.size(), SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool CSQLiteDB::ExistsBlob(const std::vector<unsigned char>& key)
{
    LOCK(sqlitedb.cs_sqlite);
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(pdb, "SELECT 1 FROM main WHERE key=?;", -1, &stmt, NULL) != SQLITE_OK)
        return false;
    sqlite3_bind_blob(stmt, 1, key.empty() ? (const void*)"" : (const void*)&key[0], (int)key.size(), SQLITE_STATIC);
    bool fExists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return fExists;
}

CSQLiteCursor* CSQLiteDB::GetCursor()
{
    if (!pdb)
        return NULL;
    return new CSQLiteCursor();
}

int CSQLiteDB::ReadAtCursor(CSQLiteCursor* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags)
{
    if (!pcursor)
        return 99999;
    LOCK(sqlitedb.cs_sqlite);

    // Lazily prepare the iterating statement. DB_SET_RANGE seeks to the first
    // key >= the (already-populated) ssKey; anything else is a full ordered scan.
    if (!pcursor->fStarted) {
        if (fFlags == DB_SET_RANGE) {
            if (sqlite3_prepare_v2(pdb, "SELECT key,value FROM main WHERE key>=? ORDER BY key;", -1, &pcursor->stmt, NULL) != SQLITE_OK)
                return 99999;
            std::vector<unsigned char> vchKey(ssKey.begin(), ssKey.end());
            sqlite3_bind_blob(pcursor->stmt, 1, vchKey.empty() ? (const void*)"" : (const void*)&vchKey[0], (int)vchKey.size(), SQLITE_TRANSIENT);
        } else {
            if (sqlite3_prepare_v2(pdb, "SELECT key,value FROM main ORDER BY key;", -1, &pcursor->stmt, NULL) != SQLITE_OK)
                return 99999;
        }
        pcursor->fStarted = true;
    }

    int rc = sqlite3_step(pcursor->stmt);
    if (rc == SQLITE_DONE)
        return DB_NOTFOUND;
    if (rc != SQLITE_ROW)
        return 99999;

    const void* kdata = sqlite3_column_blob(pcursor->stmt, 0);
    int klen = sqlite3_column_bytes(pcursor->stmt, 0);
    const void* vdata = sqlite3_column_blob(pcursor->stmt, 1);
    int vlen = sqlite3_column_bytes(pcursor->stmt, 1);

    ssKey.SetType(SER_DISK);
    ssKey.clear();
    ssKey.write((const char*)kdata, klen);
    ssValue.SetType(SER_DISK);
    ssValue.clear();
    ssValue.write((const char*)vdata, vlen);
    return 0;
}

bool CSQLiteDB::TxnBegin()
{
    if (!pdb || fInTxn)
        return false;
    LOCK(sqlitedb.cs_sqlite);
    if (sqlite3_exec(pdb, "BEGIN IMMEDIATE;", NULL, NULL, NULL) != SQLITE_OK)
        return false;
    fInTxn = true;
    return true;
}

bool CSQLiteDB::TxnCommit()
{
    if (!pdb || !fInTxn)
        return false;
    LOCK(sqlitedb.cs_sqlite);
    bool fOk = (sqlite3_exec(pdb, "COMMIT;", NULL, NULL, NULL) == SQLITE_OK);
    fInTxn = false;
    return fOk;
}

bool CSQLiteDB::TxnAbort()
{
    if (!pdb || !fInTxn)
        return false;
    LOCK(sqlitedb.cs_sqlite);
    bool fOk = (sqlite3_exec(pdb, "ROLLBACK;", NULL, NULL, NULL) == SQLITE_OK);
    fInTxn = false;
    return fOk;
}

bool CSQLiteDB::Rewrite(const std::string& strFile, const char* /*pszSkip*/)
{
    // SQLite compaction. pszSkip (BDB's "skip this record while rewriting") is
    // unused: callers only pass it to drop the "\x04pool" garbage BDB left
    // behind, which does not arise here.
    LOCK(sqlitedb.cs_sqlite);
    sqlite3* db = sqlitedb.Open(strFile);
    if (!db) {
        return false;
    }
    char* err = NULL;
    bool fOk = (sqlite3_exec(db, "VACUUM;", NULL, NULL, &err) == SQLITE_OK);
    if (err) sqlite3_free(err);
    sqlitedb.Close(strFile);
    return fOk;
}
