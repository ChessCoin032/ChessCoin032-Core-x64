// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// v1.5.4 point #3: one-time, launch-time migration of the wallet file from
// Berkeley DB to SQLite.
//
// Safety contract (this handles user funds):
//   1. If wallet.dat is missing or already SQLite -> do nothing.
//   2. Open the legacy BDB wallet inside a *recovered* DbEnv so every committed
//      record is visible, even if the previous build did not checkpoint.
//   3. Copy records as RAW key/value blobs into a TEMP SQLite file. Nothing is
//      reinterpreted; private keys are moved as opaque bytes.
//   4. Verify the temp file: same record count AND identical key set, plus an
//      integrity_check. Abort on any mismatch.
//   5. Only then swap: move the original to wallet.<timestamp>.bdb.bak, then
//      move the temp into place. The original is NEVER overwritten in place;
//      both renames target non-existing paths (safe on Windows).
//   6. Any failure before the swap leaves the original untouched and returns
//      an error so startup can abort rather than run on a half-migrated state.

#ifndef BITCOIN_WALLETMIGRATE_H
#define BITCOIN_WALLETMIGRATE_H

#include <string>

enum WalletMigrateResult
{
    WMIGRATE_NONE,    // nothing to do (no file, or already SQLite)
    WMIGRATE_DONE,    // migrated successfully
    WMIGRATE_ERROR    // failed; original wallet left intact, do not start
};

// strWalletFile is the bare filename (e.g. "wallet.dat") inside the data dir.
WalletMigrateResult MigrateWalletToSQLite(const std::string& strWalletFile, std::string& strError);

#endif // BITCOIN_WALLETMIGRATE_H
