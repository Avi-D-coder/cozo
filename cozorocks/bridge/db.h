//
// Created by Ziyang Hu on 2022/7/3.
//

#ifndef COZOROCKS_DB_H
#define COZOROCKS_DB_H

#include <utility>

#include "iostream"
#include "common.h"
#include "tx.h"
#include "slice.h"

struct RawRocksDbBridge {
    unique_ptr<DB> db;
    unique_ptr<Comparator> comparator;
    unique_ptr<Options> options;
    unique_ptr<ReadOptions> r_opts;
    unique_ptr<WriteOptions> w_opts;
    bool destroy_on_exit;
    string db_path;

    inline ~RawRocksDbBridge() {
        if (destroy_on_exit) {
            auto status = db->Close();
            if (!status.ok()) {
                cerr << status.ToString() << endl;
            }
            db.reset();
            auto status2 = DestroyDB(db_path, *options);
            if (!status2.ok()) {
                cerr << status.ToString() << endl;
            }
        }
    }

    inline void set_ignore_range_deletions(bool v) const {
        r_opts->ignore_range_deletions = v;
    }

    [[nodiscard]] inline const string &get_db_path() const {
        return db_path;
    }

    inline unique_ptr<IterBridge> iterator() const {
        return make_unique<IterBridge>(&*db);
    };

    inline unique_ptr<PinnableSlice> get(RustBytes key, RocksDbStatus &status) const {
        Slice key_ = convert_slice(key);
        auto ret = make_unique<PinnableSlice>();
        auto s = db->Get(*r_opts, db->DefaultColumnFamily(), key_, &*ret);
        write_status(s, status);
        return ret;
    }

    inline void exists(RustBytes key, RocksDbStatus &status) const {
        Slice key_ = convert_slice(key);
        auto ret = PinnableSlice();
        auto s = db->Get(*r_opts, db->DefaultColumnFamily(), key_, &ret);
        write_status(s, status);
    }

    inline void put(RustBytes key, RustBytes val, RocksDbStatus &status) const {
        write_status(db->Put(*w_opts, convert_slice(key), convert_slice(val)), status);
    }

    inline void del(RustBytes key, RocksDbStatus &status) const {
        write_status(db->Delete(*w_opts, convert_slice(key)), status);
    }

    inline void del_range(RustBytes start, RustBytes end, RocksDbStatus &status) const {
        write_status(db->DeleteRange(*w_opts, db->DefaultColumnFamily(), convert_slice(start), convert_slice(end)),
                     status);
    }
};

struct RocksDbBridge {
    unique_ptr<Comparator> comparator;
    unique_ptr<Options> options;
    bool destroy_on_exit;
    string db_path;

    [[nodiscard]] virtual unique_ptr<TxBridge> transact() const = 0;

    [[nodiscard]] inline const string &get_db_path() const {
        return db_path;
    }
};

struct OptimisticRocksDb : public RocksDbBridge {
    unique_ptr<OptimisticTransactionDB> db;

    [[nodiscard]] inline unique_ptr<TxBridge> transact() const override {
        auto ret = make_unique<TxBridge>(&*this->db);
        ret->o_tx_opts->cmp = &*comparator;
        return ret;
    }

    virtual ~OptimisticRocksDb();
};

struct PessimisticRocksDb : public RocksDbBridge {
    unique_ptr<TransactionDBOptions> tdb_opts;
    unique_ptr<TransactionDB> db;

    [[nodiscard]] inline unique_ptr<TxBridge> transact() const override {
        auto ret = make_unique<TxBridge>(&*this->db);
        return ret;
    }

    virtual ~PessimisticRocksDb();
};

//typedef int8_t (*CmpFn)(RustBytes a, RustBytes b);
typedef rust::Fn<std::int8_t(rust::Slice<const std::uint8_t>, rust::Slice<const std::uint8_t>)> RustComparatorFn;

class RustComparator : public Comparator {
public:
    inline RustComparator(string name_, bool can_different_bytes_be_equal_, RustComparatorFn f) :
            name(std::move(name_)),
            ext_cmp(f),
            can_different_bytes_be_equal(can_different_bytes_be_equal_) {
    }

    [[nodiscard]] inline int Compare(const Slice &a, const Slice &b) const override {
        return ext_cmp(convert_slice_back(a), convert_slice_back(b));
    }

    [[nodiscard]] inline const char *Name() const override {
        return name.c_str();
    }

    [[nodiscard]] inline bool CanKeysWithDifferentByteContentsBeEqual() const override {
        return can_different_bytes_be_equal;
    }

    inline void FindShortestSeparator(string *, const Slice &) const override {}

    inline void FindShortSuccessor(string *) const override {}

    string name;
    RustComparatorFn ext_cmp;
    bool can_different_bytes_be_equal;
};

shared_ptr<RawRocksDbBridge>
open_raw_db(const DbOpts &opts, RocksDbStatus &status, bool use_cmp, RustComparatorFn cmp_impl, bool no_wal);

shared_ptr<RocksDbBridge> open_db(const DbOpts &opts, RocksDbStatus &status, bool use_cmp, RustComparatorFn cmp_impl);

#endif //COZOROCKS_DB_H