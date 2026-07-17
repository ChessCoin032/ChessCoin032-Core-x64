// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// v1.5.4 point #3: wallet storage backend migrated from Berkeley DB to SQLite.
//
// CSQLiteDB provides exactly the same protected template interface that CDB
// (the BDB wrapper in db.h) exposed -- Read/Write/Erase/Exists, a cursor, and
// Txn{Begin,Commit,Abort} -- so CWalletDB (walletdb.h) can derive from it
// instead of CDB with no change to its higher-level methods.
//
// Storage model (identical to Bitcoin Core's SQLite wallet): a single table
//     main(key BLOB PRIMARY KEY NOT NULL, value BLOB NOT NULL)
// holding the SAME serialized key/value byte pairs BDB stored. Nothing is
// re-encoded per record type; we move opaque blobs. SQLite compares BLOB keys
// with memcmp, which matches BDB's default btree byte ordering, so ordered
// cursor scans and prefix seeks behave the same.
//
// NOTE: Berkeley DB stays linked in the project (peers.dat still uses CDB, and
// the migration reads the legacy wallet through it). Only the wallet's live
// store changes here.

#ifndef BITCOIN_SQLITEWALLET_H
#define BITCOIN_SQLITEWALLET_H

#include <map>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>

#include "serialize.h"   // CDataStream, SER_DISK
#include "version.h"     // CLIENT_VERSION
#include "sync.h"        // CCriticalSection / LOCK
#include "db.h"          // for DB_NEXT / DB_SET_RANGE / DB_NOTFOUND macros (BDB headers)

struct sqlite3;
struct sqlite3_stmt;

/** Process-wide manager of open SQLite wallet connections, analogous to the
 *  BDB `bitdb` (CDBEnv). One shared sqlite3* per filename, all access
 *  serialized by cs_sqlite. */
class CSQLiteEnv
{
public:
    mutable CCriticalSection cs_sqlite;

    CSQLiteEnv();
    ~CSQLiteEnv();

    // Open (creating if necessary) the wallet database file, returning a shared
    // connection. Reference-counted per filename.
    sqlite3* Open(const std::string& strFile);
    // Release one reference; closes (with a WAL checkpoint) when the last one
    // goes away.
    void Close(const std::string& strFile);
    // Flush/checkpoint without closing (called by the periodic flush thread and
    // at shutdown).
    void Flush(const std::string& strFile);
    // PRAGMA integrity_check on a not-yet-opened file. Returns true if OK.
    static bool IntegrityCheck(const std::string& strFile);

private:
    struct Handle { sqlite3* db; int refcount; };
    std::map<std::string, Handle> mapDb;
    boost::filesystem::path Path(const std::string& strFile) const;
};

extern CSQLiteEnv sqlitedb;

/** Ordered cursor over the main table. Supports a full scan (DB_NEXT from the
 *  start) and a prefix/range seek (DB_SET_RANGE), matching how walletdb.cpp
 *  uses the BDB cursor. */
class CSQLiteCursor
{
public:
    sqlite3_stmt* stmt;
    bool fStarted;
    CSQLiteCursor() : stmt(NULL), fStarted(false) {}
    void close();   // named to match the BDB Dbc::close() call site
};

/** RAII access to a SQLite-backed wallet database, drop-in for CDB. */
class CSQLiteDB
{
protected:
    sqlite3* pdb;
    std::string strFile;
    bool fReadOnly;
    bool fInTxn;

    explicit CSQLiteDB(const char* pszFile, const char* pszMode = "r+");
    ~CSQLiteDB() { Close(); }

public:
    void Close();

private:
    CSQLiteDB(const CSQLiteDB&);
    void operator=(const CSQLiteDB&);

    // Low-level blob primitives (raw bytes in/out of the main table).
    bool ReadBlob(const std::vector<unsigned char>& key, std::vector<unsigned char>& value);
    bool WriteBlob(const std::vector<unsigned char>& key, const std::vector<unsigned char>& value, bool fOverwrite);
    bool EraseBlob(const std::vector<unsigned char>& key);
    bool ExistsBlob(const std::vector<unsigned char>& key);

protected:
    template<typename K, typename T>
    bool Read(const K& key, T& value)
    {
        if (!pdb)
            return false;
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        std::vector<unsigned char> vchKey(ssKey.begin(), ssKey.end());
        std::vector<unsigned char> vchValue;
        if (!ReadBlob(vchKey, vchValue))
            return false;
        try {
            CDataStream ssValue((char*)&vchValue[0], (char*)&vchValue[0] + vchValue.size(), SER_DISK, CLIENT_VERSION);
            ssValue >> value;
        }
        catch (std::exception&) {
            return false;
        }
        return true;
    }

    template<typename K, typename T>
    bool Write(const K& key, const T& value, bool fOverwrite = true)
    {
        if (!pdb)
            return false;
        if (fReadOnly)
            assert(!"Write called on database in read-only mode");
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(10000);
        ssValue << value;
        std::vector<unsigned char> vchKey(ssKey.begin(), ssKey.end());
        std::vector<unsigned char> vchValue(ssValue.begin(), ssValue.end());
        return WriteBlob(vchKey, vchValue, fOverwrite);
    }

    template<typename K>
    bool Erase(const K& key)
    {
        if (!pdb)
            return false;
        if (fReadOnly)
            assert(!"Erase called on database in read-only mode");
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        std::vector<unsigned char> vchKey(ssKey.begin(), ssKey.end());
        return EraseBlob(vchKey);
    }

    template<typename K>
    bool Exists(const K& key)
    {
        if (!pdb)
            return false;
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(1000);
        ssKey << key;
        std::vector<unsigned char> vchKey(ssKey.begin(), ssKey.end());
        return ExistsBlob(vchKey);
    }

    // Cursor API — same shape as CDB's. ReadAtCursor returns 0 on a row,
    // DB_NOTFOUND at end, and nonzero on error, so walletdb.cpp's existing loop
    // logic is unchanged (only the cursor pointer's type differs).
    CSQLiteCursor* GetCursor();
    int ReadAtCursor(CSQLiteCursor* pcursor, CDataStream& ssKey, CDataStream& ssValue, unsigned int fFlags = DB_NEXT);

public:
    bool TxnBegin();
    bool TxnCommit();
    bool TxnAbort();

    bool ReadVersion(int& nVersion)
    {
        nVersion = 0;
        return Read(std::string("version"), nVersion);
    }
    bool WriteVersion(int nVersion)
    {
        return Write(std::string("version"), nVersion);
    }

    // VACUUM-based compaction (replaces BDB's rewrite-to-new-file).
    bool static Rewrite(const std::string& strFile, const char* pszSkip = NULL);
};

#endif // BITCOIN_SQLITEWALLET_H
