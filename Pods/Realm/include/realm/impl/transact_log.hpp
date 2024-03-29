/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2015] Realm Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of Realm Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to Realm Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from Realm Incorporated.
 *
 **************************************************************************/

#ifndef REALM_IMPL_TRANSACT_LOG_HPP
#define REALM_IMPL_TRANSACT_LOG_HPP

#include <stdexcept>
#include <iostream>

#include <realm/string_data.hpp>
#include <realm/data_type.hpp>
#include <realm/binary_data.hpp>
#include <realm/datetime.hpp>
#include <realm/mixed.hpp>
#include <realm/util/safe_int_ops.hpp>
#include <realm/util/buffer.hpp>
#include <realm/util/string_buffer.hpp>
#include <realm/util/tuple.hpp>
#include <realm/impl/input_stream.hpp>

#include <realm/group.hpp>
#include <realm/descriptor.hpp>

namespace realm {
namespace _impl {

/// Transaction log instruction encoding
enum Instruction {
    instr_InsertGroupLevelTable =  1,
    instr_EraseGroupLevelTable  =  2, // Remove columnless table from group
    instr_RenameGroupLevelTable =  3,
    instr_MoveGroupLevelTable   = 45,
    instr_SelectTable           =  4,
    instr_SetInt                =  5,
    instr_SetBool               =  6,
    instr_SetFloat              =  7,
    instr_SetDouble             =  8,
    instr_SetString             =  9,
    instr_SetBinary             = 10,
    instr_SetDateTime           = 11,
    instr_SetTable              = 12,
    instr_SetMixed              = 13,
    instr_SetLink               = 14,
    instr_NullifyLink           = 15, // Set link to null due to target being erased
    instr_SetNull               = 16,
    instr_InsertSubstring       = 43,                                                      // FIXME: Reenumerate
    instr_EraseFromString       = 44,                                                      // FIXME: Reenumerate
    instr_InsertEmptyRows       = 17,
    instr_EraseRows             = 18, // Remove (multiple) rows
    instr_SwapRows              = 19,
    instr_ClearTable            = 20, // Remove all rows in selected table
    instr_OptimizeTable         = 21,
    instr_SelectDescriptor      = 22, // Select descriptor from currently selected root table
    instr_InsertColumn          = 23, // Insert new non-nullable column into to selected descriptor (nullable is instr_InsertNullableColumn)
    instr_InsertLinkColumn      = 24, // do, but for a link-type column
    instr_InsertNullableColumn  = 25, // Insert nullable column
    instr_EraseColumn           = 26, // Remove column from selected descriptor
    instr_EraseLinkColumn       = 27, // Remove link-type column from selected descriptor
    instr_RenameColumn          = 28, // Rename column in selected descriptor
    instr_MoveColumn            = 46, // Move column in selected descriptor                // FIXME: Reenumerate
    instr_AddSearchIndex        = 29, // Add a search index to a column
    instr_RemoveSearchIndex     = 30, // Remove a search index from a column
    instr_AddPrimaryKey         = 31, // Add a primary key to a table
    instr_RemovePrimaryKey      = 32, // Remove primary key from a table
    instr_SetLinkType           = 33, // Strong/weak
    instr_SelectLinkList        = 34,
    instr_LinkListSet           = 35, // Assign to link list entry
    instr_LinkListInsert        = 36, // Insert entry into link list
    instr_LinkListMove          = 37, // Move an entry within a link list
    instr_LinkListSwap          = 38, // Swap two entries within a link list
    instr_LinkListErase         = 39, // Remove an entry from a link list
    instr_LinkListNullify       = 40, // Remove an entry from a link list due to linked row being erased
    instr_LinkListClear         = 41, // Ramove all entries from a link list
    instr_LinkListSetAll        = 42, // Assign to link list entry
};


class TransactLogStream {
public:
    /// Ensure contiguous free space in the transaction log
    /// buffer. This method must update `out_free_begin`
    /// and `out_free_end` such that they refer to a chunk
    /// of free space whose size is at least \a n.
    ///
    /// \param n The required amount of contiguous free space. Must be
    /// small (probably not greater than 1024)
    /// \param n Must be small (probably not greater than 1024)
    virtual void transact_log_reserve(size_t size, char** out_free_begin, char** out_free_end) = 0;

    /// Copy the specified data into the transaction log buffer. This
    /// function should be called only when the specified data does
    /// not fit inside the chunk of free space currently referred to
    /// by `out_free_begin` and `out_free_end`.
    ///
    /// This method must update `out_begin` and
    /// `out_end` such that, upon return, they still
    /// refer to a (possibly empty) chunk of free space.
    virtual void transact_log_append(const char* data, size_t size, char** out_free_begin, char** out_free_end) = 0;
};

class TransactLogBufferStream: public TransactLogStream {
public:
    void transact_log_reserve(size_t size, char** out_free_begin, char** out_free_end) override;
    void transact_log_append(const char* data, size_t size, char** out_free_begin, char** out_free_end) override;

    const char* transact_log_data() const;

    util::Buffer<char> m_buffer;
};


// LCOV_EXCL_START (because the NullInstructionObserver is trivial)
class NullInstructionObserver {
public:
    /// The following methods are also those that TransactLogParser expects
    /// to find on the `InstructionHandler`.

    // No selection needed:
    bool select_table(size_t, size_t, const size_t*) { return true; }
    bool select_descriptor(size_t, const size_t*) { return true; }
    bool select_link_list(size_t, size_t, size_t) { return true; }
    bool insert_group_level_table(size_t, size_t, StringData) { return true; }
    bool erase_group_level_table(size_t, size_t) { return true; }
    bool rename_group_level_table(size_t, StringData) { return true; }
    bool move_group_level_table(size_t, size_t) { return true; }

    // Must have table selected:
    bool insert_empty_rows(size_t, size_t, size_t, bool) { return true; }
    bool erase_rows(size_t, size_t, size_t, bool) { return true; }
    bool swap_rows(size_t, size_t) { return true; }
    bool clear_table() { return true; }
    bool set_int(size_t, size_t, int_fast64_t) { return true; }
    bool set_bool(size_t, size_t, bool) { return true; }
    bool set_float(size_t, size_t, float) { return true; }
    bool set_double(size_t, size_t, double) { return true; }
    bool set_string(size_t, size_t, StringData) { return true; }
    bool set_binary(size_t, size_t, BinaryData) { return true; }
    bool set_date_time(size_t, size_t, DateTime) { return true; }
    bool set_table(size_t, size_t) { return true; }
    bool set_mixed(size_t, size_t, const Mixed&) { return true; }
    bool set_link(size_t, size_t, size_t, size_t) { return true; }
    bool set_null(size_t, size_t) { return true; }
    bool nullify_link(size_t, size_t, size_t) { return true; }
    bool insert_substring(size_t, size_t, size_t, StringData) { return true; }
    bool erase_substring(size_t, size_t, size_t, size_t) { return true; }
    bool optimize_table() { return true; };

    // Must have descriptor selected:
    bool insert_link_column(size_t, DataType, StringData, size_t, size_t) { return true; }
    bool insert_column(size_t, DataType, StringData, bool) { return true; }
    bool erase_link_column(size_t, size_t, size_t) { return true; }
    bool erase_column(size_t) { return true; }
    bool rename_column(size_t, StringData) { return true; }
    bool move_column(size_t, size_t) { return true; }
    bool add_search_index(size_t) { return true; }
    bool remove_search_index(size_t) { return true; }
    bool add_primary_key(size_t) { return true; }
    bool remove_primary_key() { return true; }
    bool set_link_type(size_t, LinkType) { return true; }

    // Must have linklist selected:
    bool link_list_set(size_t, size_t) { return true; }
    bool link_list_insert(size_t, size_t) { return true; }
    bool link_list_move(size_t, size_t) { return true; }
    bool link_list_swap(size_t, size_t) { return true; }
    bool link_list_erase(size_t) { return true; }
    bool link_list_nullify(size_t) { return true; }
    bool link_list_clear(size_t) { return true; }

    void parse_complete() {}
};
// LCOV_EXCL_STOP (NullInstructionObserver)


/// See TransactLogConvenientEncoder for information about the meaning of the
/// arguments of each of the functions in this class.
class TransactLogEncoder {
public:
    /// The following methods are also those that TransactLogParser expects
    /// to find on the `InstructionHandler`.

    // No selection needed:
    bool select_table(size_t group_level_ndx, size_t levels, const size_t* path);
    bool select_descriptor(size_t levels, const size_t* path);
    bool select_link_list(size_t col_ndx, size_t row_ndx, size_t link_target_group_level_ndx);
    bool insert_group_level_table(size_t table_ndx, size_t num_tables, StringData name);
    bool erase_group_level_table(size_t table_ndx, size_t num_tables);
    bool rename_group_level_table(size_t table_ndx, StringData new_name);
    bool move_group_level_table(size_t table_ndx_1, size_t table_ndx_2);

    /// Must have table selected.
    bool insert_empty_rows(size_t row_ndx, size_t num_rows_to_insert, size_t prior_num_rows,
                           bool unordered);
    bool erase_rows(size_t row_ndx, size_t num_rows_to_erase, size_t prior_num_rows,
                    bool unordered);
    bool swap_rows(size_t row_ndx_1, size_t row_ndx_2);
    bool clear_table();
    bool set_int(size_t col_ndx, size_t row_ndx, int_fast64_t);
    bool set_bool(size_t col_ndx, size_t row_ndx, bool);
    bool set_float(size_t col_ndx, size_t row_ndx, float);
    bool set_double(size_t col_ndx, size_t row_ndx, double);
    bool set_string(size_t col_ndx, size_t row_ndx, StringData);
    bool set_binary(size_t col_ndx, size_t row_ndx, BinaryData);
    bool set_date_time(size_t col_ndx, size_t row_ndx, DateTime);
    bool set_table(size_t col_ndx, size_t row_ndx);
    bool set_mixed(size_t col_ndx, size_t row_ndx, const Mixed&);
    bool set_link(size_t col_ndx, size_t row_ndx, size_t, size_t target_group_level_ndx);
    bool set_null(size_t col_ndx, size_t row_ndx);
    bool nullify_link(size_t col_ndx, size_t row_ndx, size_t target_group_level_ndx);
    bool insert_substring(size_t col_ndx, size_t row_ndx, size_t pos, StringData);
    bool erase_substring(size_t col_ndx, size_t row_ndx, size_t pos, size_t size);
    bool optimize_table();

    // Must have descriptor selected:
    bool insert_link_column(size_t col_ndx, DataType, StringData name, size_t link_target_table_ndx, size_t backlink_col_ndx);
    bool insert_column(size_t col_ndx, DataType, StringData name, bool nullable = false);
    bool erase_link_column(size_t col_ndx, size_t link_target_table_ndx, size_t backlink_col_ndx);
    bool erase_column(size_t col_ndx);
    bool rename_column(size_t col_ndx, StringData new_name);
    bool move_column(size_t col_ndx_1, size_t col_ndx_2);
    bool add_search_index(size_t col_ndx);
    bool remove_search_index(size_t col_ndx);
    bool add_primary_key(size_t col_ndx);
    bool remove_primary_key();
    bool set_link_type(size_t col_ndx, LinkType);

    // Must have linklist selected:
    bool link_list_set(size_t link_ndx, size_t value);
    bool link_list_set_all(const IntegerColumn& values);
    bool link_list_insert(size_t link_ndx, size_t value);
    bool link_list_move(size_t old_link_ndx, size_t new_link_ndx);
    bool link_list_swap(size_t link1_ndx, size_t link2_ndx);
    bool link_list_erase(size_t link_ndx);
    bool link_list_nullify(size_t link_ndx);
    bool link_list_clear(size_t old_list_size);

    /// End of methods expected by parser.


    TransactLogEncoder(TransactLogStream& out_stream);
    void set_buffer(char* new_free_begin, char* new_free_end);
    char* write_position() const { return m_transact_log_free_begin; }

private:
    // Make sure this is in agreement with the actual integer encoding
    // scheme (see encode_int()).
    static const int max_enc_bytes_per_int = 10;
    static const int max_enc_bytes_per_double = sizeof (double);
    static const int max_enc_bytes_per_num = max_enc_bytes_per_int <
        max_enc_bytes_per_double ? max_enc_bytes_per_double : max_enc_bytes_per_int;

    TransactLogStream& m_stream;

    // These two delimit a contiguous region of free space in a
    // transaction log buffer following the last written data. It may
    // be empty.
    char* m_transact_log_free_begin;
    char* m_transact_log_free_end;

    char* reserve(size_t size);
    /// \param ptr Must be in the range [m_transact_log_free_begin, m_transact_log_free_end]
    void advance(char* ptr) noexcept;

    template<class L>
    void append_simple_instr(Instruction, const util::Tuple<L>& numbers);

    template<class L>
    void append_string_instr(Instruction, const util::Tuple<L>& numbers, StringData);

    template<class L>
    void append_mixed_instr(Instruction, const util::Tuple<L>& numbers, const Mixed&);

    template<class L, class I>
    bool append_variable_size_instr(Instruction instr, const util::Tuple<L>& numbers,
                                    I var_begin, I var_end);

    template<class T>
    static char* encode_int(char*, T value);
    static char* encode_float(char*, float value);
    static char* encode_double(char*, double value);
    template<class>
    struct EncodeNumber;
};

class TransactLogConvenientEncoder {
public:
    void insert_group_level_table(size_t table_ndx, size_t num_tables, StringData name);
    void erase_group_level_table(size_t table_ndx, size_t num_tables);
    void rename_group_level_table(size_t table_ndx, StringData new_name);
    void move_group_level_table(size_t table_ndx_1, size_t table_ndx_2);
    void insert_column(const Descriptor&, size_t col_ndx, DataType type, StringData name,
                       const Table* link_target_table, bool nullable = false);
    void erase_column(const Descriptor&, size_t col_ndx);
    void rename_column(const Descriptor&, size_t col_ndx, StringData name);
    void move_column(const Descriptor&, size_t from, size_t to);

    void set_int(const Table*, size_t col_ndx, size_t ndx, int_fast64_t value);
    void set_bool(const Table*, size_t col_ndx, size_t ndx, bool value);
    void set_float(const Table*, size_t col_ndx, size_t ndx, float value);
    void set_double(const Table*, size_t col_ndx, size_t ndx, double value);
    void set_string(const Table*, size_t col_ndx, size_t ndx, StringData value);
    void set_binary(const Table*, size_t col_ndx, size_t ndx, BinaryData value);
    void set_date_time(const Table*, size_t col_ndx, size_t ndx, DateTime value);
    void set_table(const Table*, size_t col_ndx, size_t ndx);
    void set_mixed(const Table*, size_t col_ndx, size_t ndx, const Mixed& value);
    void set_link(const Table*, size_t col_ndx, size_t ndx, size_t value);
    void set_null(const Table*, size_t col_ndx, size_t ndx);
    void set_link_list(const LinkView&, const IntegerColumn& values);
    void insert_substring(const Table*, size_t col_ndx, size_t row_ndx, size_t pos, StringData);
    void erase_substring(const Table*, size_t col_ndx, size_t row_ndx, size_t pos, size_t size);

    /// \param prior_num_rows The number of rows in the table prior to the
    /// modification.
    void insert_empty_rows(const Table*, size_t row_ndx, size_t num_rows_to_insert,
                           size_t prior_num_rows);

    /// \param prior_num_rows The number of rows in the table prior to the
    /// modification.
    void erase_rows(const Table*, size_t row_ndx, size_t num_rows_to_erase, size_t prior_num_rows,
                    bool is_move_last_over);

    void swap_rows(const Table*, size_t row_ndx_1, size_t row_ndx_2);
    void add_search_index(const Table*, size_t col_ndx);
    void remove_search_index(const Table*, size_t col_ndx);
    void add_primary_key(const Table*, size_t col_ndx);
    void remove_primary_key(const Table*);
    void set_link_type(const Table*, size_t col_ndx, LinkType);
    void clear_table(const Table*);
    void optimize_table(const Table*);

    void link_list_set(const LinkView&, size_t link_ndx, size_t value);
    void link_list_insert(const LinkView&, size_t link_ndx, size_t value);
    void link_list_move(const LinkView&, size_t old_link_ndx, size_t new_link_ndx);
    void link_list_swap(const LinkView&, size_t link_ndx_1, size_t link_ndx_2);
    void link_list_erase(const LinkView&, size_t link_ndx);
    void link_list_clear(const LinkView&);

    //@{

    /// Implicit nullifications due to removal of target row. This is redundant
    /// information from the point of view of replication, as the removal of the
    /// target row will reproduce the implicit nullifications in the target
    /// Realm anyway. The purpose of this instruction is to allow observers
    /// (reactor pattern) to be explicitly notified about the implicit
    /// nullifications.

    void nullify_link(const Table*, size_t col_ndx, size_t ndx);
    void link_list_nullify(const LinkView&, size_t link_ndx);

    //@}

    void on_table_destroyed(const Table*) noexcept;
    void on_spec_destroyed(const Spec*) noexcept;
    void on_link_list_destroyed(const LinkView&) noexcept;

protected:
    TransactLogConvenientEncoder(TransactLogStream& encoder);

    void reset_selection_caches();
    void set_buffer(char* new_free_begin, char* new_free_end) { m_encoder.set_buffer(new_free_begin, new_free_end); }
    char* write_position() const { return m_encoder.write_position(); }

private:
    TransactLogEncoder m_encoder;
    // These are mutable because they are caches.
    mutable util::Buffer<size_t> m_subtab_path_buf;
    mutable const Table*    m_selected_table;
    mutable const Spec*     m_selected_spec;
    mutable const LinkView* m_selected_link_list;

    void select_table(const Table*);
    void select_desc(const Descriptor&);
    void select_link_list(const LinkView&);
    // These reset the above caches and modify them as necessary
    void record_subtable_path(const Table&, size_t*& out_begin, size_t*& out_end);
    void do_select_table(const Table*);
    void do_select_desc(const Descriptor&);
    void do_select_link_list(const LinkView&);

    friend class TransactReverser;
};


class TransactLogParser {
public:
    class BadTransactLog; // Exception

    TransactLogParser();
    ~TransactLogParser() noexcept;

    /// See `TransactLogEncoder` for a list of methods that the `InstructionHandler` must define.
    /// parse() promises that the path passed by reference to
    /// InstructionHandler::select_descriptor() will remain valid
    /// during subsequent calls to all descriptor modifying functions.
    template<class InstructionHandler>
    void parse(InputStream&, InstructionHandler&);

    template<class InstructionHandler>
    void parse(NoCopyInputStream&, InstructionHandler&);

private:
    util::Buffer<char> m_input_buffer;

    // The input stream is assumed to consist of chunks of memory organised such that
    // every instruction resides in a single chunk only.
    NoCopyInputStream* m_input;
    // pointer into transaction log, each instruction is parsed from m_input_begin and onwards.
    // Each instruction are assumed to be contiguous in memory.
    const char* m_input_begin;
    // pointer to one past current instruction log chunk. If m_input_begin reaches m_input_end,
    // a call to next_input_buffer will move m_input_begin and m_input_end to a new chunk of
    // memory. Setting m_input_end to 0 disables this check, and is used if it is already known
    // that all of the instructions are in memory.
    const char* m_input_end;
    util::StringBuffer m_string_buffer;
    static const int m_max_levels = 1024;
    util::Buffer<size_t> m_path;

    REALM_NORETURN void parser_error() const;

    template<class InstructionHandler>
    void parse_one(InstructionHandler&);
    bool has_next() noexcept;

    template<class T>
    T read_int();

    void read_bytes(char* data, size_t size);
    BinaryData read_buffer(util::StringBuffer&, size_t size);

    float read_float();
    double read_double();

    StringData read_string(util::StringBuffer&);
    BinaryData read_binary(util::StringBuffer&);
    void read_mixed(Mixed*);

    // Advance m_input_begin and m_input_end to reflect the next block of instructions
    // Returns false if no more input was available
    bool next_input_buffer();

    // return true if input was available
    bool read_char(char&); // throws

    bool is_valid_data_type(int type);
    bool is_valid_link_type(int type);
};


class TransactLogParser::BadTransactLog: public std::exception {
public:
    const char* what() const noexcept override
    {
        return "Bad transaction log";
    }
};



/// Implementation:

inline void TransactLogBufferStream::transact_log_reserve(size_t n, char** inout_new_begin, char** out_new_end)
{
    char* data = m_buffer.data();
    REALM_ASSERT(*inout_new_begin >= data);
    REALM_ASSERT(*inout_new_begin <= (data + m_buffer.size()));
    size_t size = *inout_new_begin - data;
    m_buffer.reserve_extra(size, n);
    data = m_buffer.data(); // May have changed
    *inout_new_begin = data + size;
    *out_new_end = data + m_buffer.size();
}

inline void TransactLogBufferStream::transact_log_append(const char* data, size_t size, char** out_new_begin, char** out_new_end)
{
    transact_log_reserve(size, out_new_begin, out_new_end);
    *out_new_begin = std::copy(data, data + size, *out_new_begin);
}

inline const char* TransactLogBufferStream::transact_log_data() const
{
    return m_buffer.data();
}

inline TransactLogEncoder::TransactLogEncoder(TransactLogStream& stream):
    m_stream(stream),
    m_transact_log_free_begin(nullptr),
    m_transact_log_free_end(nullptr)
{
}

inline void TransactLogEncoder::set_buffer(char* free_begin, char* free_end)
{
    REALM_ASSERT(free_begin <= free_end);
    m_transact_log_free_begin = free_begin;
    m_transact_log_free_end   = free_end;
}

inline void TransactLogConvenientEncoder::reset_selection_caches()
{
    m_selected_table = nullptr;
    m_selected_spec  = nullptr;
    m_selected_link_list  = nullptr;
}

inline char* TransactLogEncoder::reserve(size_t n)
{
    if (size_t(m_transact_log_free_end - m_transact_log_free_begin) < n) {
        m_stream.transact_log_reserve(n, &m_transact_log_free_begin, &m_transact_log_free_end);
    }
    return m_transact_log_free_begin;
}

inline void TransactLogEncoder::advance(char* ptr) noexcept
{
    REALM_ASSERT_DEBUG(m_transact_log_free_begin <= ptr);
    REALM_ASSERT_DEBUG(ptr <= m_transact_log_free_end);
    m_transact_log_free_begin = ptr;
}


// The integer encoding is platform independent. Also, it does not
// depend on the type of the specified integer. Integers of any type
// can be encoded as long as the specified buffer is large enough (see
// below). The decoding does not have to use the same type. Decoding
// will fail if, and only if the encoded value falls outside the range
// of the requested destination type.
//
// The encoding uses one or more bytes. It never uses more than 8 bits
// per byte. The last byte in the sequence is the first one that has
// its 8th bit set to zero.
//
// Consider a particular non-negative value V. Let W be the number of
// bits needed to encode V using the trivial binary encoding of
// integers. The total number of bytes produced is then
// ceil((W+1)/7). The first byte holds the 7 least significant bits of
// V. The last byte holds at most 6 bits of V including the most
// significant one. The value of the first bit of the last byte is
// always 2**((N-1)*7) where N is the total number of bytes.
//
// A negative value W is encoded by setting the sign bit to one and
// then encoding the positive result of -(W+1) as described above. The
// advantage of this representation is that it converts small negative
// values to small positive values which require a small number of
// bytes. This would not have been true for 2's complements
// representation, for example. The sign bit is always stored as the
// 7th bit of the last byte.
//
//               value bits    value + sign    max bytes
//     --------------------------------------------------
//     int8_t         7              8              2
//     uint8_t        8              9              2
//     int16_t       15             16              3
//     uint16_t      16             17              3
//     int32_t       31             32              5
//     uint32_t      32             33              5
//     int64_t       63             64             10
//     uint64_t      64             65             10
//
template<class T>
char* TransactLogEncoder::encode_int(char* ptr, T value)
{
    static_assert(std::numeric_limits<T>::is_integer, "Integer required");
    bool negative = util::is_negative(value);
    if (negative) {
        // The following conversion is guaranteed by C++11 to never
        // overflow (contrast this with "-value" which indeed could
        // overflow). See C99+TC3 section 6.2.6.2 paragraph 2.
        value = -(value + 1);
    }
    // At this point 'value' is always a positive number. Also, small
    // negative numbers have been converted to small positive numbers.
    REALM_ASSERT(!util::is_negative(value));
    // One sign bit plus number of value bits
    const int num_bits = 1 + std::numeric_limits<T>::digits;
    // Only the first 7 bits are available per byte. Had it not been
    // for the fact that maximum guaranteed bit width of a char is 8,
    // this value could have been increased to 15 (one less than the
    // number of value bits in 'unsigned').
    const int bits_per_byte = 7;
    const int max_bytes = (num_bits + (bits_per_byte-1)) / bits_per_byte;
    static_assert(max_bytes <= max_enc_bytes_per_int, "Bad max_enc_bytes_per_int");
    // An explicit constant maximum number of iterations is specified
    // in the hope that it will help the optimizer (to do loop
    // unrolling, for example).
    typedef unsigned char uchar;
    for (int i=0; i<max_bytes; ++i) {
        if (value >> (bits_per_byte-1) == 0)
            break;
        *reinterpret_cast<uchar*>(ptr) =
            uchar((1U<<bits_per_byte) | unsigned(value & ((1U<<bits_per_byte)-1)));
        ++ptr;
        value >>= bits_per_byte;
    }
    *reinterpret_cast<uchar*>(ptr) =
        uchar(negative ? (1U<<(bits_per_byte-1)) | unsigned(value) : value);
    return ++ptr;
}

inline char* TransactLogEncoder::encode_float(char* ptr, float value)
{
    static_assert(std::numeric_limits<float>::is_iec559 &&
                          sizeof (float) * std::numeric_limits<unsigned char>::digits == 32,
                          "Unsupported 'float' representation");
    const char* val_ptr = reinterpret_cast<char*>(&value);
    return std::copy(val_ptr, val_ptr + sizeof value, ptr);
}

inline char* TransactLogEncoder::encode_double(char* ptr, double value)
{
    static_assert(std::numeric_limits<double>::is_iec559 &&
                          sizeof (double) * std::numeric_limits<unsigned char>::digits == 64,
                          "Unsupported 'double' representation");
    const char* val_ptr = reinterpret_cast<char*>(&value);
    return std::copy(val_ptr, val_ptr + sizeof value, ptr);
}

template<class T>
struct TransactLogEncoder::EncodeNumber {
    void operator()(T value, char** ptr)
    {
        auto value_2 = value + 0; // Perform integral promotion
        *ptr = encode_int(*ptr, value_2);
    }
};
template<>
struct TransactLogEncoder::EncodeNumber<float> {
    void operator()(float value, char** ptr)
    {
        *ptr = encode_float(*ptr, value);
    }
};
template<>
struct TransactLogEncoder::EncodeNumber<double> {
    void operator()(double value, char** ptr)
    {
        *ptr = encode_double(*ptr, value);
    }
};

template<class L>
void TransactLogEncoder::append_simple_instr(Instruction instr, const util::Tuple<L>& numbers)
{
    size_t num_numbers = util::TypeCount<L>::value;
    size_t max_required_bytes = 1 + max_enc_bytes_per_num * num_numbers;
    char* ptr = reserve(max_required_bytes); // Throws
    *ptr++ = char(instr);
    util::for_each<EncodeNumber>(numbers, &ptr);
    advance(ptr);
}

template<class L>
void TransactLogEncoder::append_string_instr(Instruction instr, const util::Tuple<L>& numbers,
                                             StringData string)
{
    size_t num_numbers = util::TypeCount<L>::value + 1;
    size_t max_required_bytes = 1 + max_enc_bytes_per_num * num_numbers + string.size();
    char* ptr = reserve(max_required_bytes); // Throws
    *ptr++ = char(instr);
    util::for_each<EncodeNumber>(append(numbers, string.size()), &ptr);
    ptr = std::copy(string.data(), string.data() + string.size(), ptr);
    advance(ptr);
}

template<class L>
void TransactLogEncoder::append_mixed_instr(Instruction instr, const util::Tuple<L>& numbers,
                                            const Mixed& value)
{
    DataType type = value.get_type();
    auto numbers_2 = append(numbers, type);
    switch (type) {
        case type_Int:
            append_simple_instr(instr, append(numbers_2, value.get_int())); // Throws
            return;
        case type_Bool:
            append_simple_instr(instr, append(numbers_2, value.get_bool())); // Throws
            return;
        case type_Float:
            append_simple_instr(instr, append(numbers_2, value.get_float())); // Throws
            return;
        case type_Double:
            append_simple_instr(instr, append(numbers_2, value.get_double())); // Throws
            return;
        case type_DateTime: {
            auto value_2 = value.get_datetime().get_datetime();
            append_simple_instr(instr, append(numbers_2, value_2)); // Throws
            return;
        }
        case type_String: {
            append_string_instr(instr, numbers_2, value.get_string()); // Throws
            return;
        }
        case type_Binary: {
            BinaryData value_2 = value.get_binary();
            StringData value_3(value_2.data(), value_2.size());
            append_string_instr(instr, numbers_2, value_3); // Throws
            return;
        }
        case type_Table:
            append_simple_instr(instr, numbers_2); // Throws
            return;
        case type_Mixed:
            // Mixed in mixed is not possible
            REALM_ASSERT_RELEASE(false);
        case type_Link:
        case type_LinkList:
            // FIXME: Need to handle new link types here.
            REALM_ASSERT_RELEASE(false);
    }
    REALM_ASSERT_RELEASE(false);
}

template<class L, class I>
bool TransactLogEncoder::append_variable_size_instr(Instruction instr,
                                                    const util::Tuple<L>& numbers,
                                                    I var_begin, I var_end)
{
    // Space is reserved in chunks to avoid excessive over allocation.
#ifdef REALM_DEBUG
    const int max_numbers_per_chunk = 2; // Increase the chance of chunking in debug mode
#else
    const int max_numbers_per_chunk = 8;
#endif
    size_t num_numbers = util::TypeCount<L>::value + max_numbers_per_chunk;
    size_t max_required_bytes = 1 + max_enc_bytes_per_num * num_numbers;
    char* ptr = reserve(max_required_bytes); // Throws
    *ptr++ = char(instr);
    util::for_each<EncodeNumber>(numbers, &ptr);
    I i = var_begin;
    while (var_end - i > max_numbers_per_chunk) {
        for (int j = 0; j < max_numbers_per_chunk; ++j)
            ptr = encode_int(ptr, *i++);
        advance(ptr);
        size_t max_required_bytes_2 = max_enc_bytes_per_num * max_numbers_per_chunk;
        ptr = reserve(max_required_bytes_2); // Throws
    }
    while (i != var_end)
        ptr = encode_int(ptr, *i++);
    advance(ptr);
    return true;
}

inline
void TransactLogConvenientEncoder::select_table(const Table* table)
{
    if (table != m_selected_table) {
        do_select_table(table);
    }
}

inline
void TransactLogConvenientEncoder::select_desc(const Descriptor& desc)
{
    typedef _impl::DescriptorFriend df;
    if (&df::get_spec(desc) != m_selected_spec)
        do_select_desc(desc); // Throws
}

inline
void TransactLogConvenientEncoder::select_link_list(const LinkView& list)
{
    if (&list != m_selected_link_list)
        do_select_link_list(list); // Throws
}


inline bool TransactLogEncoder::insert_group_level_table(size_t table_ndx, size_t num_tables,
                                                         StringData name)
{
    append_string_instr(instr_InsertGroupLevelTable, util::tuple(table_ndx, num_tables),
                        name); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::insert_group_level_table(size_t table_ndx,
                                                                   size_t num_tables,
                                                                   StringData name)
{
    m_encoder.insert_group_level_table(table_ndx, num_tables, name); // Throws
}

inline bool TransactLogEncoder::erase_group_level_table(size_t table_ndx, size_t num_tables)
{
    append_simple_instr(instr_EraseGroupLevelTable, util::tuple(table_ndx, num_tables)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::erase_group_level_table(size_t table_ndx,
                                                                  size_t num_tables)
{
    m_encoder.erase_group_level_table(table_ndx, num_tables); // Throws
}

inline bool TransactLogEncoder::rename_group_level_table(size_t table_ndx, StringData new_name)
{
    append_string_instr(instr_RenameGroupLevelTable, util::tuple(table_ndx), new_name); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::rename_group_level_table(size_t table_ndx,
                                                                   StringData new_name)
{
    m_encoder.rename_group_level_table(table_ndx, new_name); // Throws
}

inline bool TransactLogEncoder::move_group_level_table(size_t table_ndx_1, size_t table_ndx_2)
{
    append_simple_instr(instr_MoveGroupLevelTable, util::tuple(table_ndx_1, table_ndx_2));
    return true;
}

inline void TransactLogConvenientEncoder::move_group_level_table(size_t table_ndx_1, size_t table_ndx_2)
{
    m_encoder.move_group_level_table(table_ndx_1, table_ndx_2);
}

inline bool TransactLogEncoder::insert_column(size_t col_ndx, DataType type, StringData name,
                                              bool nullable)
{
    Instruction instr = (nullable ? instr_InsertNullableColumn : instr_InsertColumn);
    append_string_instr(instr, util::tuple(col_ndx, type), name); // Throws
    return true;
}

inline bool TransactLogEncoder::insert_link_column(size_t col_ndx, DataType type, StringData name,
                                                   size_t link_target_table_ndx,
                                                   size_t backlink_col_ndx)
{
    REALM_ASSERT(_impl::TableFriend::is_link_type(ColumnType(type)));
    append_string_instr(instr_InsertLinkColumn, util::tuple(col_ndx, type, link_target_table_ndx,
                                                            backlink_col_ndx), name); // Throws
    return true;
}


inline void TransactLogConvenientEncoder::insert_column(const Descriptor& desc, size_t col_ndx,
                                                        DataType type,
                                                        StringData name,
                                                        const Table* link_target_table,
                                                        bool nullable)
{
    select_desc(desc); // Throws
    if (link_target_table) {
        typedef _impl::TableFriend tf;
        typedef _impl::DescriptorFriend df;
        size_t target_table_ndx = link_target_table->get_index_in_group();
        const Table& origin_table = df::get_root_table(desc);
        REALM_ASSERT(origin_table.is_group_level());
        const Spec& target_spec = tf::get_spec(*link_target_table);
        size_t origin_table_ndx = origin_table.get_index_in_group();
        size_t backlink_col_ndx = target_spec.find_backlink_column(origin_table_ndx, col_ndx);
        m_encoder.insert_link_column(col_ndx, type, name, target_table_ndx, backlink_col_ndx); // Throws
    }
    else {
        m_encoder.insert_column(col_ndx, type, name, nullable); // Throws
    }
}

inline bool TransactLogEncoder::erase_column(size_t col_ndx)
{
    append_simple_instr(instr_EraseColumn, util::tuple(col_ndx)); // Throws
    return true;
}

inline bool TransactLogEncoder::erase_link_column(size_t col_ndx, size_t link_target_table_ndx,
                                                  size_t backlink_col_ndx)
{
    append_simple_instr(instr_EraseLinkColumn, util::tuple(col_ndx, link_target_table_ndx,
                                                           backlink_col_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::erase_column(const Descriptor& desc, size_t col_ndx)
{
    select_desc(desc); // Throws

    DataType type = desc.get_column_type(col_ndx);
    typedef _impl::TableFriend tf;
    if (!tf::is_link_type(ColumnType(type))) {
        m_encoder.erase_column(col_ndx); // Throws
    }
    else { // it's a link column:
        REALM_ASSERT(desc.is_root());
        typedef _impl::DescriptorFriend df;
        const Table& origin_table = df::get_root_table(desc);
        REALM_ASSERT(origin_table.is_group_level());
        const Table& target_table = *tf::get_link_target_table_accessor(origin_table, col_ndx);
        size_t target_table_ndx = target_table.get_index_in_group();
        const Spec& target_spec = tf::get_spec(target_table);
        size_t origin_table_ndx = origin_table.get_index_in_group();
        size_t backlink_col_ndx = target_spec.find_backlink_column(origin_table_ndx, col_ndx);
        m_encoder.erase_link_column(col_ndx, target_table_ndx, backlink_col_ndx); // Throws
    }
}

inline bool TransactLogEncoder::rename_column(size_t col_ndx, StringData new_name)
{
    append_string_instr(instr_RenameColumn, util::tuple(col_ndx), new_name); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::rename_column(const Descriptor& desc, size_t col_ndx,
                                       StringData name)
{
    select_desc(desc); // Throws
    m_encoder.rename_column(col_ndx, name); // Throws
}


inline bool TransactLogEncoder::move_column(size_t from, size_t to)
{
    append_simple_instr(instr_MoveColumn, util::tuple(from, to)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::move_column(const Descriptor& desc, size_t from, size_t to)
{
    select_desc(desc); // Throws
    m_encoder.move_column(from, to);
}


inline bool TransactLogEncoder::set_int(size_t col_ndx, size_t ndx, int_fast64_t value)
{
    append_simple_instr(instr_SetInt, util::tuple(col_ndx, ndx, value)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_int(const Table* t, size_t col_ndx,
                                 size_t ndx, int_fast64_t value)
{
    select_table(t); // Throws
    m_encoder.set_int(col_ndx, ndx, value); // Throws
}

inline bool TransactLogEncoder::set_bool(size_t col_ndx, size_t ndx, bool value)
{
    append_simple_instr(instr_SetBool, util::tuple(col_ndx, ndx, value)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_bool(const Table* t, size_t col_ndx,
                                  size_t ndx, bool value)
{
    select_table(t); // Throws
    m_encoder.set_bool(col_ndx, ndx, value); // Throws
}

inline bool TransactLogEncoder::set_float(size_t col_ndx, size_t ndx, float value)
{
    append_simple_instr(instr_SetFloat, util::tuple(col_ndx, ndx, value)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_float(const Table* t, size_t col_ndx,
                                   size_t ndx, float value)
{
    select_table(t); // Throws
    m_encoder.set_float(col_ndx, ndx, value); // Throws
}

inline bool TransactLogEncoder::set_double(size_t col_ndx, size_t ndx, double value)
{
    append_simple_instr(instr_SetDouble, util::tuple(col_ndx, ndx, value)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_double(const Table* t, size_t col_ndx,
                                    size_t ndx, double value)
{
    select_table(t); // Throws
    m_encoder.set_double(col_ndx, ndx, value); // Throws
}

inline bool TransactLogEncoder::set_string(size_t col_ndx, size_t ndx, StringData value)
{
    if (value.is_null()) {
        set_null(col_ndx, ndx); // Throws
    }
    else {
        append_string_instr(instr_SetString, util::tuple(col_ndx, ndx), value); // Throws
    }
    return true;
}

inline void TransactLogConvenientEncoder::set_string(const Table* t, size_t col_ndx,
                                    size_t ndx, StringData value)
{
    select_table(t); // Throws
    m_encoder.set_string(col_ndx, ndx, value); // Throws
}

inline bool TransactLogEncoder::set_binary(size_t col_ndx, size_t row_ndx, BinaryData value)
{
    if (value.is_null()) {
        set_null(col_ndx, row_ndx); // Throws
    }
    else {
        StringData value_2(value.data(), value.size());
        append_string_instr(instr_SetBinary, util::tuple(col_ndx, row_ndx), value_2); // Throws
    }
    return true;
}

inline void TransactLogConvenientEncoder::set_binary(const Table* t, size_t col_ndx,
                                    size_t ndx, BinaryData value)
{
    select_table(t); // Throws
    m_encoder.set_binary(col_ndx, ndx, value); // Throws
}

inline bool TransactLogEncoder::set_date_time(size_t col_ndx, size_t ndx, DateTime value)
{
    append_simple_instr(instr_SetDateTime, util::tuple(col_ndx, ndx,
                                                       value.get_datetime())); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_date_time(const Table* t, size_t col_ndx,
                                       size_t ndx, DateTime value)
{
    select_table(t); // Throws
    m_encoder.set_date_time(col_ndx, ndx, value); // Throws
}

inline bool TransactLogEncoder::set_table(size_t col_ndx, size_t ndx)
{
    append_simple_instr(instr_SetTable, util::tuple(col_ndx, ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_table(const Table* t, size_t col_ndx,
                                   size_t ndx)
{
    select_table(t); // Throws
    m_encoder.set_table(col_ndx, ndx); // Throws
}

inline bool TransactLogEncoder::set_mixed(size_t col_ndx, size_t ndx, const Mixed& value)
{
    append_mixed_instr(instr_SetMixed, util::tuple(col_ndx, ndx), value); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_mixed(const Table* t, size_t col_ndx,
                                   size_t ndx, const Mixed& value)
{
    select_table(t); // Throws
    m_encoder.set_mixed(col_ndx, ndx, value); // Throws
}

inline bool TransactLogEncoder::set_link(size_t col_ndx, size_t ndx,
                                         size_t value, size_t target_group_level_ndx)
{
    // Map `realm::npos` to zero, and `n` to `n+1`, where `n` is a target row
    // index.
    size_t value_2 = size_t(1) + value;
    append_simple_instr(instr_SetLink, util::tuple(col_ndx, ndx, value_2,
                                                   target_group_level_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_link(const Table* t, size_t col_ndx,
                                  size_t ndx, size_t value)
{
    select_table(t); // Throws
    size_t target_group_level_ndx = t->get_descriptor()->get_column_link_target(col_ndx);
    m_encoder.set_link(col_ndx, ndx, value, target_group_level_ndx); // Throws
}

inline bool TransactLogEncoder::set_null(size_t col_ndx, size_t ndx)
{
    append_simple_instr(instr_SetNull, util::tuple(col_ndx, ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_null(const Table* t, size_t col_ndx,
                                                   size_t row_ndx)
{
    select_table(t); // Throws
    m_encoder.set_null(col_ndx, row_ndx); // Throws
}

inline bool TransactLogEncoder::nullify_link(size_t col_ndx, size_t ndx,
                                             size_t target_group_level_ndx)
{
    append_simple_instr(instr_NullifyLink, util::tuple(col_ndx, ndx,
                                                       target_group_level_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::nullify_link(const Table* t, size_t col_ndx, size_t ndx)
{
    select_table(t); // Throws
    size_t target_group_level_ndx = t->get_descriptor()->get_column_link_target(col_ndx);
    m_encoder.nullify_link(col_ndx, ndx, target_group_level_ndx); // Throws
}

inline bool TransactLogEncoder::insert_substring(size_t col_ndx, size_t row_ndx, size_t pos,
                                                 StringData value)
{
    append_string_instr(instr_InsertSubstring, util::tuple(col_ndx, row_ndx, pos),
                        value); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::insert_substring(const Table* t, size_t col_ndx,
                                                           size_t row_ndx, size_t pos,
                                                           StringData value)
{
    if (value.size() > 0) {
        select_table(t); // Throws
        m_encoder.insert_substring(col_ndx, row_ndx, pos, value); // Throws
    }
}

inline bool TransactLogEncoder::erase_substring(size_t col_ndx, size_t row_ndx, size_t pos,
                                                size_t size)
{
    append_simple_instr(instr_EraseFromString, util::tuple(col_ndx, row_ndx, pos, size)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::erase_substring(const Table* t, size_t col_ndx,
                                                          size_t row_ndx, size_t pos,
                                                          size_t size)
{
    if (size > 0) {
        select_table(t); // Throws
        m_encoder.erase_substring(col_ndx, row_ndx, pos, size); // Throws
    }
}

inline bool TransactLogEncoder::insert_empty_rows(size_t row_ndx, size_t num_rows_to_insert,
                                                  size_t prior_num_rows, bool unordered)
{
    append_simple_instr(instr_InsertEmptyRows, util::tuple(row_ndx, num_rows_to_insert,
                                                           prior_num_rows, unordered)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::insert_empty_rows(const Table* t, size_t row_ndx,
                                                            size_t num_rows_to_insert,
                                                            size_t prior_num_rows)
{
    select_table(t); // Throws
    bool unordered = false;
    m_encoder.insert_empty_rows(row_ndx, num_rows_to_insert, prior_num_rows,
                                unordered); // Throws
}

inline bool TransactLogEncoder::erase_rows(size_t row_ndx, size_t num_rows_to_erase,
                                           size_t prior_num_rows, bool unordered)
{
    append_simple_instr(instr_EraseRows, util::tuple(row_ndx, num_rows_to_erase, prior_num_rows,
                                                     unordered)); // Throws
    return true;
}


inline void TransactLogConvenientEncoder::erase_rows(const Table* t, size_t row_ndx,
                                                     size_t num_rows_to_erase,
                                                     size_t prior_num_rows,
                                                     bool is_move_last_over)
{
    select_table(t); // Throws
    bool unordered = is_move_last_over;
    m_encoder.erase_rows(row_ndx, num_rows_to_erase, prior_num_rows, unordered); // Throws
}

inline bool TransactLogEncoder::swap_rows(size_t row_ndx_1, size_t row_ndx_2)
{
    append_simple_instr(instr_SwapRows, util::tuple(row_ndx_1, row_ndx_2)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::swap_rows(const Table* t, size_t row_ndx_1, size_t row_ndx_2)
{
    REALM_ASSERT(row_ndx_1 < row_ndx_2);
    select_table(t); // Throws
    m_encoder.swap_rows(row_ndx_1, row_ndx_2);
}

inline bool TransactLogEncoder::add_search_index(size_t col_ndx)
{
    append_simple_instr(instr_AddSearchIndex, util::tuple(col_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::add_search_index(const Table* t, size_t col_ndx)
{
    select_table(t); // Throws
    m_encoder.add_search_index(col_ndx); // Throws
}


inline bool TransactLogEncoder::remove_search_index(size_t col_ndx)
{
    append_simple_instr(instr_RemoveSearchIndex, util::tuple(col_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::remove_search_index(const Table* t, size_t col_ndx)
{
    select_table(t); // Throws
    m_encoder.remove_search_index(col_ndx); // Throws
}


inline bool TransactLogEncoder::add_primary_key(size_t col_ndx)
{
    append_simple_instr(instr_AddPrimaryKey, util::tuple(col_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::add_primary_key(const Table* t, size_t col_ndx)
{
    select_table(t); // Throws
    m_encoder.add_primary_key(col_ndx); // Throws
}


inline bool TransactLogEncoder::remove_primary_key()
{
    append_simple_instr(instr_RemovePrimaryKey, util::tuple()); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::remove_primary_key(const Table* t)
{
    select_table(t); // Throws
    m_encoder.remove_primary_key(); // Throws
}


inline bool TransactLogEncoder::set_link_type(size_t col_ndx, LinkType link_type)
{
    append_simple_instr(instr_SetLinkType, util::tuple(col_ndx, int(link_type))); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_link_type(const Table* t, size_t col_ndx, LinkType link_type)
{
    select_table(t); // Throws
    m_encoder.set_link_type(col_ndx, link_type); // Throws
}


inline bool TransactLogEncoder::clear_table()
{
    append_simple_instr(instr_ClearTable, util::tuple()); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::clear_table(const Table* t)
{
    select_table(t); // Throws
    m_encoder.clear_table(); // Throws
}

inline bool TransactLogEncoder::optimize_table()
{
    append_simple_instr(instr_OptimizeTable, util::tuple()); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::optimize_table(const Table* t)
{
    select_table(t); // Throws
    m_encoder.optimize_table(); // Throws
}

inline bool TransactLogEncoder::link_list_set(size_t link_ndx, size_t value)
{
    append_simple_instr(instr_LinkListSet, util::tuple(link_ndx, value)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::link_list_set(const LinkView& list, size_t link_ndx,
                                       size_t value)
{
    select_link_list(list); // Throws
    m_encoder.link_list_set(link_ndx, value); // Throws
}

inline bool TransactLogEncoder::link_list_nullify(size_t link_ndx)
{
    append_simple_instr(instr_LinkListNullify, util::tuple(link_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::link_list_nullify(const LinkView& list, size_t link_ndx)
{
    select_link_list(list); // Throws
    m_encoder.link_list_nullify(link_ndx); // Throws
}

inline bool TransactLogEncoder::link_list_set_all(const IntegerColumn& values)
{
    struct iter {
        iter(const IntegerColumn& values, size_t ndx): m_values(&values), m_ndx(ndx) {}
        const IntegerColumn* m_values;
        size_t m_ndx;
        bool operator==(const iter& i) const { return m_ndx == i.m_ndx; }
        bool operator!=(const iter& i) const { return m_ndx != i.m_ndx; }
        size_t operator-(const iter& i) const { return m_ndx - i.m_ndx; }
        int_fast64_t operator*() const { return m_values->get(m_ndx); }
        iter& operator++() { ++m_ndx; return *this; }
        iter operator++(int) { iter i = *this; ++m_ndx; return i; }
    };
    size_t num_values = values.size();
    append_variable_size_instr(instr_LinkListSetAll, util::tuple(num_values),
                               iter(values, 0), iter(values, num_values)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::set_link_list(const LinkView& list, const IntegerColumn& values)
{
    select_link_list(list); // Throws
    m_encoder.link_list_set_all(values); // Throws
}

inline bool TransactLogEncoder::link_list_insert(size_t link_ndx, size_t value)
{
    append_simple_instr(instr_LinkListInsert, util::tuple(link_ndx, value)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::link_list_insert(const LinkView& list, size_t link_ndx,
                                          size_t value)
{
    select_link_list(list); // Throws
    m_encoder.link_list_insert(link_ndx, value); // Throws
}

inline bool TransactLogEncoder::link_list_move(size_t old_link_ndx, size_t new_link_ndx)
{
    append_simple_instr(instr_LinkListMove, util::tuple(old_link_ndx, new_link_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::link_list_move(const LinkView& list, size_t old_link_ndx,
                                        size_t new_link_ndx)
{
    select_link_list(list); // Throws
    m_encoder.link_list_move(old_link_ndx, new_link_ndx); // Throws
}

inline bool TransactLogEncoder::link_list_swap(size_t link1_ndx, size_t link2_ndx)
{
    append_simple_instr(instr_LinkListSwap, util::tuple(link1_ndx, link2_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::link_list_swap(const LinkView& list, size_t link1_ndx,
                                                         size_t link2_ndx)
{
    select_link_list(list); // Throws
    m_encoder.link_list_swap(link1_ndx, link2_ndx); // Throws
}

inline bool TransactLogEncoder::link_list_erase(size_t link_ndx)
{
    append_simple_instr(instr_LinkListErase, util::tuple(link_ndx)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::link_list_erase(const LinkView& list, size_t link_ndx)
{
    select_link_list(list); // Throws
    m_encoder.link_list_erase(link_ndx); // Throws
}

inline bool TransactLogEncoder::link_list_clear(size_t old_list_size)
{
    append_simple_instr(instr_LinkListClear, util::tuple(old_list_size)); // Throws
    return true;
}

inline void TransactLogConvenientEncoder::on_table_destroyed(const Table* t) noexcept
{
    if (m_selected_table == t)
        m_selected_table = nullptr;
}

inline void TransactLogConvenientEncoder::on_spec_destroyed(const Spec* s) noexcept
{
    if (m_selected_spec == s)
        m_selected_spec = nullptr;
}


inline void TransactLogConvenientEncoder::on_link_list_destroyed(const LinkView& list) noexcept
{
    if (m_selected_link_list == &list)
        m_selected_link_list = nullptr;
}


inline TransactLogParser::TransactLogParser():
    m_input_buffer(1024) // Throws
{
}


inline TransactLogParser::~TransactLogParser() noexcept
{
}


template<class InstructionHandler>
void TransactLogParser::parse(NoCopyInputStream& in, InstructionHandler& handler)
{
    m_input = &in;
    m_input_begin = m_input_end = nullptr;

    while (has_next())
        parse_one(handler); // Throws
}

template<class InstructionHandler>
void TransactLogParser::parse(InputStream& in, InstructionHandler& handler)
{
    NoCopyInputStreamAdaptor in_2(in, m_input_buffer.data(), m_input_buffer.size());
    parse(in_2, handler); // Throws
}

inline bool TransactLogParser::has_next() noexcept
{
    return m_input_begin != m_input_end || next_input_buffer();
}

template<class InstructionHandler>
void TransactLogParser::parse_one(InstructionHandler& handler)
{
    char instr;
    if (!read_char(instr))
        parser_error();
//    std::cerr << "parsing " << util::promote(instr) << " @ " << std::hex << long(m_input_begin) << std::dec << "\n";
    switch (Instruction(instr)) {
        case instr_SetInt: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            // FIXME: Don't depend on the existence of int64_t,
            // but don't allow values to use more than 64 bits
            // either.
            int_fast64_t value = read_int<int64_t>(); // Throws
            if (!handler.set_int(col_ndx, row_ndx, value)) // Throws
                parser_error();
            return;
        }
        case instr_SetBool: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            bool value = read_int<bool>(); // Throws
            if (!handler.set_bool(col_ndx, row_ndx, value)) // Throws
                parser_error();
            return;
        }
        case instr_SetFloat: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            float value = read_float(); // Throws
            if (!handler.set_float(col_ndx, row_ndx, value)) // Throws
                parser_error();
            return;
        }
        case instr_SetDouble: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            double value = read_double(); // Throws
            if (!handler.set_double(col_ndx, row_ndx, value)) // Throws
                parser_error();
            return;
        }
        case instr_SetString: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            StringData value = read_string(m_string_buffer); // Throws
            if (!handler.set_string(col_ndx, row_ndx, value)) // Throws
                parser_error();
            return;
        }
        case instr_SetBinary: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            BinaryData value = read_binary(m_string_buffer); // Throws
            if (!handler.set_binary(col_ndx, row_ndx, value)) // Throws
                parser_error();
            return;
        }
        case instr_SetDateTime: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            int_fast64_t value = read_int<int_fast64_t>(); // Throws
            if (!handler.set_date_time(col_ndx, row_ndx, value)) // Throws
                parser_error();
            return;
        }
        case instr_SetTable: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            if (!handler.set_table(col_ndx, row_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_SetMixed: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            Mixed value;
            read_mixed(&value); // Throws
            if (!handler.set_mixed(col_ndx, row_ndx, value)) // Throws
                parser_error();
            return;
        }
        case instr_SetLink: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            size_t value = read_int<size_t>(); // Throws
            // Map zero to realm::npos, and `n+1` to `n`, where `n` is a target row index.
            size_t target_row_ndx = size_t(value - 1);
            size_t target_group_level_ndx = read_int<size_t>(); // Throws
            if (!handler.set_link(col_ndx, row_ndx, target_row_ndx, target_group_level_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_SetNull: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            if (!handler.set_null(col_ndx, row_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_NullifyLink: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            size_t target_group_level_ndx = read_int<size_t>(); // Throws
            if (!handler.nullify_link(col_ndx, row_ndx, target_group_level_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_InsertSubstring: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            size_t pos = read_int<size_t>(); // Throws
            StringData value = read_string(m_string_buffer); // Throws
            if (!handler.insert_substring(col_ndx, row_ndx, pos, value)) // Throws
                parser_error();
            return;
        }
        case instr_EraseFromString: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            size_t pos = read_int<size_t>(); // Throws
            size_t size = read_int<size_t>(); // Throws
            if (!handler.erase_substring(col_ndx, row_ndx, pos, size)) // Throws
                parser_error();
            return;
        }
        case instr_InsertEmptyRows: {
            size_t row_ndx = read_int<size_t>(); // Throws
            size_t num_rows_to_insert = read_int<size_t>(); // Throws
            size_t prior_num_rows = read_int<size_t>(); // Throws
            bool unordered = read_int<bool>(); // Throws
            if (!handler.insert_empty_rows(row_ndx, num_rows_to_insert, prior_num_rows,
                                           unordered)) // Throws
                parser_error();
            return;
        }
        case instr_EraseRows: {
            size_t row_ndx = read_int<size_t>(); // Throws
            size_t num_rows_to_erase = read_int<size_t>(); // Throws
            size_t prior_num_rows = read_int<size_t>(); // Throws
            bool unordered = read_int<bool>(); // Throws
            if (!handler.erase_rows(row_ndx, num_rows_to_erase, prior_num_rows,
                                    unordered)) // Throws
                parser_error();
            return;
        }
        case instr_SwapRows: {
            size_t row_ndx_1 = read_int<size_t>(); // Throws
            size_t row_ndx_2 = read_int<size_t>(); // Throws
            if (!handler.swap_rows(row_ndx_1, row_ndx_2)) // Throws
                parser_error();
            return;
        }
        case instr_SelectTable: {
            int levels = read_int<int>(); // Throws
            if (levels < 0 || levels > m_max_levels)
                parser_error();
            m_path.reserve(0, 2*levels); // Throws
            size_t* path = m_path.data();
            size_t group_level_ndx = read_int<size_t>(); // Throws
            for (int i = 0; i != levels; ++i) {
                size_t col_ndx = read_int<size_t>(); // Throws
                size_t row_ndx = read_int<size_t>(); // Throws
                path[2*i + 0] = col_ndx;
                path[2*i + 1] = row_ndx;
            }
            if (!handler.select_table(group_level_ndx, levels, path)) // Throws
                parser_error();
            return;
        }
        case instr_ClearTable: {
            if (!handler.clear_table()) // Throws
                parser_error();
            return;
        }
        case instr_LinkListSet: {
            size_t link_ndx = read_int<size_t>(); // Throws
            size_t value = read_int<size_t>(); // Throws
            if (!handler.link_list_set(link_ndx, value)) // Throws
                parser_error();
            return;
        }
        case instr_LinkListSetAll: {
            // todo, log that it's a SetAll we're doing
            size_t size = read_int<size_t>(); // Throws
            for (size_t i = 0; i < size; i++) {
                size_t link = read_int<size_t>(); // Throws
                if (!handler.link_list_set(i, link)) // Throws
                    parser_error();
            }
            return;
        }
        case instr_LinkListInsert: {
            size_t link_ndx = read_int<size_t>(); // Throws
            size_t value = read_int<size_t>(); // Throws
            if (!handler.link_list_insert(link_ndx, value)) // Throws
                parser_error();
            return;
        }
        case instr_LinkListMove: {
            size_t old_link_ndx = read_int<size_t>(); // Throws
            size_t new_link_ndx = read_int<size_t>(); // Throws
            if (!handler.link_list_move(old_link_ndx, new_link_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_LinkListSwap: {
            size_t link1_ndx = read_int<size_t>(); // Throws
            size_t link2_ndx = read_int<size_t>(); // Throws
            if (!handler.link_list_swap(link1_ndx, link2_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_LinkListErase: {
            size_t link_ndx = read_int<size_t>(); // Throws
            if (!handler.link_list_erase(link_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_LinkListNullify: {
            size_t link_ndx = read_int<size_t>(); // Throws
            if (!handler.link_list_nullify(link_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_LinkListClear: {
            size_t old_list_size = read_int<size_t>(); // Throws
            if (!handler.link_list_clear(old_list_size)) // Throws
                parser_error();
            return;
        }
        case instr_SelectLinkList: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t row_ndx = read_int<size_t>(); // Throws
            size_t target_group_level_ndx = read_int<size_t>(); // Throws
            if (!handler.select_link_list(col_ndx, row_ndx, target_group_level_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_AddSearchIndex: {
            size_t col_ndx = read_int<size_t>(); // Throws
            if (!handler.add_search_index(col_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_RemoveSearchIndex: {
            size_t col_ndx = read_int<size_t>(); // Throws
            if (!handler.remove_search_index(col_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_AddPrimaryKey: {
            size_t col_ndx = read_int<size_t>(); // Throws
            if (!handler.add_primary_key(col_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_RemovePrimaryKey: {
            if (!handler.remove_primary_key()) // Throws
                parser_error();
            return;
        }
        case instr_SetLinkType: {
            size_t col_ndx = read_int<size_t>(); // Throws
            int link_type = read_int<int>(); // Throws
            if (!is_valid_link_type(link_type))
                parser_error();
            if (!handler.set_link_type(col_ndx, LinkType(link_type))) // Throws
                parser_error();
            return;
        }
        case instr_InsertColumn:
        case instr_InsertNullableColumn: {
            size_t col_ndx = read_int<size_t>(); // Throws
            int type = read_int<int>(); // Throws
            if (!is_valid_data_type(type))
                parser_error();
            if (REALM_UNLIKELY(type == type_Link || type == type_LinkList))
                parser_error();
            StringData name = read_string(m_string_buffer); // Throws
            bool nullable = (Instruction(instr) == instr_InsertNullableColumn);
            if (REALM_UNLIKELY(nullable && (type == type_Table || type == type_Mixed))) {
                // Nullability not supported for Table and Mixed columns.
                parser_error();
            }
            if (!handler.insert_column(col_ndx, DataType(type), name, nullable)) // Throws
                parser_error();
            return;
        }
        case instr_InsertLinkColumn: {
            size_t col_ndx = read_int<size_t>(); // Throws
            int type = read_int<int>(); // Throws
            if (!is_valid_data_type(type))
                parser_error();
            if (REALM_UNLIKELY(type != type_Link && type != type_LinkList))
                parser_error();
            size_t link_target_table_ndx = read_int<size_t>(); // Throws
            size_t backlink_col_ndx = read_int<size_t>(); // Throws
            StringData name = read_string(m_string_buffer); // Throws
            if (!handler.insert_link_column(col_ndx, DataType(type), name,
                                            link_target_table_ndx, backlink_col_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_EraseColumn: {
            size_t col_ndx = read_int<size_t>(); // Throws
            if (!handler.erase_column(col_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_EraseLinkColumn: {
            size_t col_ndx = read_int<size_t>(); // Throws
            size_t link_target_table_ndx = read_int<size_t>(); // Throws
            size_t backlink_col_ndx      = read_int<size_t>(); // Throws
            if (!handler.erase_link_column(col_ndx, link_target_table_ndx,
                                           backlink_col_ndx)) // Throws
                parser_error();
            return;
        }
        case instr_RenameColumn: {
            size_t col_ndx = read_int<size_t>(); // Throws
            StringData name = read_string(m_string_buffer); // Throws
            if (!handler.rename_column(col_ndx, name)) // Throws
                parser_error();
            return;
        }
        case instr_MoveColumn: {
            size_t col_ndx_1 = read_int<size_t>(); // Throws
            size_t col_ndx_2 = read_int<size_t>(); // Throws
            if (!handler.move_column(col_ndx_1, col_ndx_2)) // Throws
                parser_error();
            return;
        }
        case instr_SelectDescriptor: {
            int levels = read_int<int>(); // Throws
            if (levels < 0 || levels > m_max_levels)
                parser_error();
            m_path.reserve(0, levels); // Throws
            size_t* path = m_path.data();
            for (int i = 0; i != levels; ++i) {
                size_t col_ndx = read_int<size_t>(); // Throws
                path[i] = col_ndx;
            }
            if (!handler.select_descriptor(levels, path)) // Throws
                parser_error();
            return;
        }
        case instr_InsertGroupLevelTable: {
            size_t table_ndx  = read_int<size_t>(); // Throws
            size_t num_tables = read_int<size_t>(); // Throws
            StringData name = read_string(m_string_buffer); // Throws
            if (!handler.insert_group_level_table(table_ndx, num_tables, name)) // Throws
                parser_error();
            return;
        }
        case instr_EraseGroupLevelTable: {
            size_t table_ndx  = read_int<size_t>(); // Throws
            size_t num_tables = read_int<size_t>(); // Throws
            if (!handler.erase_group_level_table(table_ndx, num_tables)) // Throws
                parser_error();
            return;
        }
        case instr_RenameGroupLevelTable: {
            size_t table_ndx = read_int<size_t>(); // Throws
            StringData new_name = read_string(m_string_buffer); // Throws
            if (!handler.rename_group_level_table(table_ndx, new_name)) // Throws
                parser_error();
            return;
        }
        case instr_MoveGroupLevelTable: {
            size_t table_ndx_1 = read_int<size_t>(); // Throws
            size_t table_ndx_2 = read_int<size_t>(); // Throws
            if (!handler.move_group_level_table(table_ndx_1, table_ndx_2)) // Throws
                parser_error();
            return;
        }
        case instr_OptimizeTable: {
            if (!handler.optimize_table()) // Throws
                parser_error();
            return;
        }
    }

    throw BadTransactLog();
}


template<class T>
T TransactLogParser::read_int()
{
    T value = 0;
    int part = 0;
    const int max_bytes = (std::numeric_limits<T>::digits+1+6)/7;
    for (int i = 0; i != max_bytes; ++i) {
        char c;
        if (!read_char(c))
            goto bad_transact_log;
        part = static_cast<unsigned char>(c);
        if (0xFF < part)
            goto bad_transact_log; // Only the first 8 bits may be used in each byte
        if ((part & 0x80) == 0) {
            T p = part & 0x3F;
            if (util::int_shift_left_with_overflow_detect(p, i*7))
                goto bad_transact_log;
            value |= p;
            break;
        }
        if (i == max_bytes-1)
            goto bad_transact_log; // Too many bytes
        value |= T(part & 0x7F) << (i*7);
    }
    if (part & 0x40) {
        // The real value is negative. Because 'value' is positive at
        // this point, the following negation is guaranteed by C++11
        // to never overflow. See C99+TC3 section 6.2.6.2 paragraph 2.
        value = -value;
        if (util::int_subtract_with_overflow_detect(value, 1))
            goto bad_transact_log;
    }
    return value;

  bad_transact_log:
    throw BadTransactLog();
}


inline void TransactLogParser::read_bytes(char* data, size_t size)
{
    for (;;) {
        const size_t avail = m_input_end - m_input_begin;
        if (size <= avail)
            break;
        const char* to = m_input_begin + avail;
        std::copy(m_input_begin, to, data);
        if (!next_input_buffer())
            throw BadTransactLog();
        data += avail;
        size -= avail;
    }
    const char* to = m_input_begin + size;
    std::copy(m_input_begin, to, data);
    m_input_begin = to;
}


inline BinaryData TransactLogParser::read_buffer(util::StringBuffer& buf, size_t size)
{
    const size_t avail = m_input_end - m_input_begin;
    if (avail >= size) {
        m_input_begin += size;
        return BinaryData(m_input_begin - size, size);
    }

    buf.clear();
    buf.resize(size); // Throws
    read_bytes(buf.data(), size);
    return BinaryData(buf.data(), size);
}


inline float TransactLogParser::read_float()
{
    static_assert(std::numeric_limits<float>::is_iec559 &&
                          sizeof (float) * std::numeric_limits<unsigned char>::digits == 32,
                          "Unsupported 'float' representation");
    float value;
    read_bytes(reinterpret_cast<char*>(&value), sizeof value); // Throws
    return value;
}


inline double TransactLogParser::read_double()
{
    static_assert(std::numeric_limits<double>::is_iec559 &&
                          sizeof (double) * std::numeric_limits<unsigned char>::digits == 64,
                          "Unsupported 'double' representation");
    double value;
    read_bytes(reinterpret_cast<char*>(&value), sizeof value); // Throws
    return value;
}


inline StringData TransactLogParser::read_string(util::StringBuffer& buf)
{
    size_t size = read_int<size_t>(); // Throws

    if (size > Table::max_string_size)
        parser_error();

    BinaryData buffer = read_buffer(buf, size);
    return StringData{buffer.data(), size};
}


inline BinaryData TransactLogParser::read_binary(util::StringBuffer& buf)
{
    size_t size = read_int<size_t>(); // Throws

    if (size > Table::max_binary_size)
        parser_error();

    return read_buffer(buf, size);
}


inline void TransactLogParser::read_mixed(Mixed* mixed)
{
    DataType type = DataType(read_int<int>()); // Throws
    switch (type) {
        case type_Int: {
            // FIXME: Don't depend on the existence of
            // int64_t, but don't allow values to use more
            // than 64 bits either.
            int_fast64_t value = read_int<int64_t>(); // Throws
            mixed->set_int(value);
            return;
        }
        case type_Bool: {
            bool value = read_int<bool>(); // Throws
            mixed->set_bool(value);
            return;
        }
        case type_Float: {
            float value = read_float(); // Throws
            mixed->set_float(value);
            return;
        }
        case type_Double: {
            double value = read_double(); // Throws
            mixed->set_double(value);
            return;
        }
        case type_DateTime: {
            int_fast64_t value = read_int<int_fast64_t>(); // Throws
            mixed->set_datetime(value);
            return;
        }
        case type_String: {
            StringData value = read_string(m_string_buffer); // Throws
            mixed->set_string(value);
            return;
        }
        case type_Binary: {
            BinaryData value = read_binary(m_string_buffer); // Throws
            mixed->set_binary(value);
            return;
        }
        case type_Table: {
            *mixed = Mixed::subtable_tag();
            return;
        }
        case type_Mixed:
            break;
        case type_Link:
        case type_LinkList:
            // FIXME: Need to handle new link types here
            break;
    }
    throw BadTransactLog();
}


inline bool TransactLogParser::next_input_buffer()
{
    size_t sz = m_input->next_block(m_input_begin, m_input_end);
    if (sz == 0)
        return false;
    else
        return true;
}


inline bool TransactLogParser::read_char(char& c)
{
    if (m_input_begin == m_input_end && !next_input_buffer())
        return false;
    c = *m_input_begin++;
    return true;
}


inline bool TransactLogParser::is_valid_data_type(int type)
{
    switch (DataType(type)) {
        case type_Int:
        case type_Bool:
        case type_Float:
        case type_Double:
        case type_String:
        case type_Binary:
        case type_DateTime:
        case type_Table:
        case type_Mixed:
        case type_Link:
        case type_LinkList:
            return true;
    }
    return false;
}


inline bool TransactLogParser::is_valid_link_type(int type)
{
    switch (LinkType(type)) {
        case link_Strong:
        case link_Weak:
            return true;
    }
    return false;
}


class TransactReverser {
public:
    bool select_table(size_t group_level_ndx, size_t levels, const size_t* path)
    {
        sync_table();
        m_encoder.select_table(group_level_ndx, levels, path);
        m_pending_ts_instr = get_inst();
        return true;
    }

    bool select_descriptor(size_t levels, const size_t* path)
    {
        sync_descriptor();
        m_encoder.select_descriptor(levels, path);
        m_pending_ds_instr = get_inst();
        return true;
    }

    bool insert_group_level_table(size_t table_ndx, size_t num_tables, StringData)
    {
        m_encoder.erase_group_level_table(table_ndx, num_tables + 1);
        append_instruction();
        return true;
    }

    bool erase_group_level_table(size_t table_ndx, size_t num_tables)
    {
        m_encoder.insert_group_level_table(table_ndx, num_tables - 1, "");
        append_instruction();
        return true;
    }

    bool rename_group_level_table(size_t, StringData)
    {
        return true; // No-op
    }

    bool move_group_level_table(size_t, size_t)
    {
        return true; // No-op
    }

    bool optimize_table()
    {
        return true; // No-op
    }

    bool insert_empty_rows(size_t row_ndx, size_t num_rows_to_insert, size_t prior_num_rows,
                           bool unordered)
    {
        size_t num_rows_to_erase = num_rows_to_insert;
        size_t prior_num_rows_2 = prior_num_rows + num_rows_to_insert;
        m_encoder.erase_rows(row_ndx, num_rows_to_erase, prior_num_rows_2, unordered); // Throws
        append_instruction();
        return true;
    }

    bool erase_rows(size_t row_ndx, size_t num_rows_to_erase, size_t prior_num_rows,
                    bool unordered)
    {
        size_t num_rows_to_insert = num_rows_to_erase;
        // Number of rows in table after removal, but before inverse insertion
        size_t prior_num_rows_2 = prior_num_rows - num_rows_to_erase;
        m_encoder.insert_empty_rows(row_ndx, num_rows_to_insert, prior_num_rows_2,
                                    unordered); // Throws
        append_instruction();
        return true;
    }

    bool swap_rows(size_t row_ndx_1, size_t row_ndx_2)
    {
        m_encoder.swap_rows(row_ndx_1, row_ndx_2);
        append_instruction();
        return true;
    }

    bool set_int(size_t col_ndx, size_t row_ndx, int_fast64_t value)
    {
        m_encoder.set_int(col_ndx, row_ndx, value);
        append_instruction();
        return true;
    }

    bool set_bool(size_t col_ndx, size_t row_ndx, bool value)
    {
        m_encoder.set_bool(col_ndx, row_ndx, value);
        append_instruction();
        return true;
    }

    bool set_float(size_t col_ndx, size_t row_ndx, float value)
    {
        m_encoder.set_float(col_ndx, row_ndx, value);
        append_instruction();
        return true;
    }

    bool set_double(size_t col_ndx, size_t row_ndx, double value)
    {
        m_encoder.set_double(col_ndx, row_ndx, value);
        append_instruction();
        return true;
    }

    bool set_string(size_t col_ndx, size_t row_ndx, StringData value)
    {
        m_encoder.set_string(col_ndx, row_ndx, value);
        append_instruction();
        return true;
    }

    bool set_binary(size_t col_ndx, size_t row_ndx, BinaryData value)
    {
        m_encoder.set_binary(col_ndx, row_ndx, value);
        append_instruction();
        return true;
    }

    bool set_date_time(size_t col_ndx, size_t row_ndx, DateTime value)
    {
        m_encoder.set_date_time(col_ndx, row_ndx, value);
        append_instruction();
        return true;
    }

    bool set_table(size_t col_ndx, size_t row_ndx)
    {
        m_encoder.set_table(col_ndx, row_ndx);
        append_instruction();
        return true;
    }

    bool set_mixed(size_t col_ndx, size_t row_ndx, const Mixed& value)
    {
        m_encoder.set_mixed(col_ndx, row_ndx, value);
        append_instruction();
        return true;
    }

    bool set_null(size_t col_ndx, size_t row_ndx)
    {
        m_encoder.set_null(col_ndx, row_ndx);
        append_instruction();
        return true;
    }

    bool set_link(size_t col_ndx, size_t row_ndx, size_t value, size_t target_group_level_ndx)
    {
        m_encoder.set_link(col_ndx, row_ndx, value, target_group_level_ndx);
        append_instruction();
        return true;
    }

    bool insert_substring(size_t, size_t, size_t, StringData)
    {
        return true; // No-op
    }

    bool erase_substring(size_t, size_t, size_t, size_t)
    {
        return true; // No-op
    }

    bool clear_table()
    {
        m_encoder.insert_empty_rows(0, 0, 0, true); // FIXME: Explain what is going on here (Finn).
        append_instruction();
        return true;
    }

    bool add_search_index(size_t)
    {
        return true; // No-op
    }

    bool remove_search_index(size_t)
    {
        return true; // No-op
    }

    bool add_primary_key(size_t)
    {
        return true; // No-op
    }

    bool remove_primary_key()
    {
        return true; // No-op
    }

    bool set_link_type(size_t, LinkType)
    {
        return true; // No-op
    }

    bool insert_link_column(size_t col_idx, DataType, StringData,
                            size_t target_table_idx, size_t backlink_col_ndx)
    {
        m_encoder.erase_link_column(col_idx, target_table_idx, backlink_col_ndx);
        append_instruction();
        return true;
    }

    bool erase_link_column(size_t col_idx, size_t target_table_idx,
                           size_t backlink_col_idx)
    {
        DataType type = type_Link; // The real type of the column doesn't matter here,
                                   // but the encoder asserts that it's actually a link type.
        m_encoder.insert_link_column(col_idx, type, "", target_table_idx, backlink_col_idx);
        append_instruction();
        return true;
    }

    bool insert_column(size_t col_idx, DataType, StringData, bool)
    {
        m_encoder.erase_column(col_idx);
        append_instruction();
        return true;
    }

    bool erase_column(size_t col_idx)
    {
        m_encoder.insert_column(col_idx, DataType(), "");
        append_instruction();
        return true;
    }

    bool rename_column(size_t, StringData)
    {
        return true; // No-op
    }

    bool move_column(size_t col_ndx_1, size_t col_ndx_2)
    {
        m_encoder.move_column(col_ndx_2, col_ndx_1);
        append_instruction();
        return true;
    }

    bool select_link_list(size_t col_ndx, size_t row_ndx, size_t link_target_group_level_ndx)
    {
        sync_linkview();
        m_encoder.select_link_list(col_ndx, row_ndx, link_target_group_level_ndx);
        m_pending_lv_instr = get_inst();
        return true;
    }

    bool link_list_set(size_t row, size_t value)
    {
        m_encoder.link_list_set(row, value);
        append_instruction();
        return true;
    }

    bool link_list_insert(size_t link_ndx, size_t)
    {
        m_encoder.link_list_erase(link_ndx);
        append_instruction();
        return true;
    }

    bool link_list_move(size_t old_link_ndx, size_t new_link_ndx)
    {
        m_encoder.link_list_move(new_link_ndx, old_link_ndx);
        append_instruction();
        return true;
    }

    bool link_list_swap(size_t link1_ndx, size_t link2_ndx)
    {
        m_encoder.link_list_swap(link1_ndx, link2_ndx);
        append_instruction();
        return true;
    }

    bool link_list_erase(size_t link_ndx)
    {
        m_encoder.link_list_insert(link_ndx, 0);
        append_instruction();
        return true;
    }

    bool link_list_clear(size_t old_list_size)
    {
        // Append in reverse order because the reversed log is itself applied
        // in reverse, and this way it generates all back-insertions rather than
        // all front-insertions
        for (size_t i = old_list_size; i > 0; --i) {
            m_encoder.link_list_insert(i - 1, 0);
            append_instruction();
        }
        return true;
    }

    bool nullify_link(size_t col_ndx, size_t row_ndx, size_t target_group_level_ndx)
    {
        size_t value = 0;
        // FIXME: Is zero this right value to pass here, or should
        // TransactReverser::nullify_link() also have taken a
        // `target_group_level_ndx` argument.
        m_encoder.set_link(col_ndx, row_ndx, value, target_group_level_ndx);
        append_instruction();
        return true;
    }

    bool link_list_nullify(size_t link_ndx)
    {
        m_encoder.link_list_insert(link_ndx, 0);
        append_instruction();
        return true;
    }

private:
    _impl::TransactLogBufferStream m_buffer;
    _impl::TransactLogEncoder m_encoder{m_buffer};
    struct Instr { size_t begin; size_t end; };
    std::vector<Instr> m_instructions;
    size_t current_instr_start = 0;
    Instr m_pending_ts_instr{0, 0};
    Instr m_pending_ds_instr{0, 0};
    Instr m_pending_lv_instr{0, 0};

    Instr get_inst()
    {
        Instr instr;
        instr.begin = current_instr_start;
        current_instr_start = transact_log_size();
        instr.end = current_instr_start;
        return instr;
    }

    size_t transact_log_size() const
    {
        REALM_ASSERT_3(m_encoder.write_position(), >=, m_buffer.transact_log_data());
        return m_encoder.write_position() - m_buffer.transact_log_data();
    }

    void append_instruction()
    {
        m_instructions.push_back(get_inst());
    }

    void append_instruction(Instr instr)
    {
        m_instructions.push_back(instr);
    }

    void sync_select(Instr& pending_instr)
    {
        if (pending_instr.begin != pending_instr.end) {
            append_instruction(pending_instr);
            pending_instr = {0, 0};
        }
    }

    void sync_linkview()
    {
        sync_select(m_pending_lv_instr);
    }

    void sync_descriptor()
    {
        sync_linkview();
        sync_select(m_pending_ds_instr);
    }

    void sync_table()
    {
        sync_descriptor();
        sync_select(m_pending_ts_instr);
    }

    friend class ReversedNoCopyInputStream;
};


class ReversedNoCopyInputStream: public NoCopyInputStream {
public:
    ReversedNoCopyInputStream(TransactReverser& reverser):
        m_instr_order(reverser.m_instructions)
    {
        // push any pending select_table or select_descriptor into the buffer
        reverser.sync_table();

        m_buffer = reverser.m_buffer.transact_log_data();
        m_current = m_instr_order.size();
    }

    size_t next_block(const char*& begin, const char*& end) override
    {
        if (m_current != 0) {
            m_current--;
            begin = m_buffer + m_instr_order[m_current].begin;
            end   = m_buffer + m_instr_order[m_current].end;
            return end-begin;
        }
        return 0;
    }

private:
    const char* m_buffer;
    std::vector<TransactReverser::Instr>& m_instr_order;
    size_t m_current;
};

} // namespace _impl
} // namespace realm

#endif // REALM_IMPL_TRANSACT_LOG_HPP
