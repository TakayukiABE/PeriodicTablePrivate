/*************************************************************************
 *
 * REALM CONFIDENTIAL
 * __________________
 *
 *  [2011] - [2012] Realm Inc
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
#ifndef REALM_TABLE_HPP
#define REALM_TABLE_HPP

#include <algorithm>
#include <map>
#include <utility>
#include <typeinfo>
#include <memory>

#include <realm/util/features.h>
#include <realm/util/thread.hpp>
#include <realm/util/tuple.hpp>
#include <realm/column_fwd.hpp>
#include <realm/table_ref.hpp>
#include <realm/link_view_fwd.hpp>
#include <realm/row.hpp>
#include <realm/descriptor_fwd.hpp>
#include <realm/spec.hpp>
#include <realm/mixed.hpp>
#include <realm/query.hpp>
#include <realm/column.hpp>

namespace realm {

class TableView;
class LinkView;
class TableViewBase;
class ConstTableView;
class StringIndex;
class Group;
class LinkColumnBase;
class LinkColumn;
class LinkListColumn;
class BacklinkColumn;
template<class>
class Columns;

struct Link {};
typedef Link LinkList;

namespace _impl { class TableFriend; }

class Replication;


/// The Table class is non-polymorphic, that is, it has no virtual
/// functions. This is important because it ensures that there is no run-time
/// distinction between a Table instance and an instance of any variation of
/// BasicTable<T>, and this, in turn, makes it valid to cast a pointer from
/// Table to BasicTable<T> even when the instance is constructed as a Table. Of
/// course, this also assumes that BasicTable<> is non-polymorphic, has no
/// destructor, and adds no extra data members.
///
/// FIXME: Table assignment (from any group to any group) could be made aliasing
/// safe as follows: Start by cloning source table into target allocator. On
/// success, assign, and then deallocate any previous structure at the target.
///
/// FIXME: It might be desirable to have a 'table move' feature between two
/// places inside the same group (say from a subtable or a mixed column to group
/// level). This could be done in a very efficient manner.
///
/// FIXME: When compiling in debug mode, all public non-static table functions
/// should REALM_ASSERT(is_attached()).
class Table {
public:
    /// Construct a new freestanding top-level table with static
    /// lifetime.
    ///
    /// This constructor should be used only when placing a table
    /// instance on the stack, and it is then the responsibility of
    /// the application that there are no objects of type TableRef or
    /// ConstTableRef that refer to it, or to any of its subtables,
    /// when it goes out of scope. To create a top-level table with
    /// dynamic lifetime, use Table::create() instead.
    Table(Allocator& = Allocator::get_default());

    /// Construct a copy of the specified table as a new freestanding
    /// top-level table with static lifetime.
    ///
    /// This constructor should be used only when placing a table
    /// instance on the stack, and it is then the responsibility of
    /// the application that there are no objects of type TableRef or
    /// ConstTableRef that refer to it, or to any of its subtables,
    /// when it goes out of scope. To create a top-level table with
    /// dynamic lifetime, use Table::copy() instead.
    Table(const Table&, Allocator& = Allocator::get_default());

    ~Table() noexcept;

    Allocator& get_alloc() const;

    /// Construct a new freestanding top-level table with dynamic lifetime.
    static TableRef create(Allocator& = Allocator::get_default());

    /// Construct a copy of the specified table as a new freestanding top-level
    /// table with dynamic lifetime.
    TableRef copy(Allocator& = Allocator::get_default()) const;

    /// Returns true if, and only if this accessor is currently attached to an
    /// underlying table.
    ///
    /// A table accessor may get detached from the underlying row for various
    /// reasons (see below). When it does, it no longer refers to anything, and
    /// can no longer be used, except for calling is_attached(). The
    /// consequences of calling other non-static functions on a detached table
    /// accessor are unspecified. Table accessors obtained by calling functions in
    /// the Realm API are always in the 'attached' state immediately upon
    /// return from those functions.
    ///
    /// A table accessor of a free-standing table never becomes detached (except
    /// during its eventual destruction). A group-level table accessor becomes
    /// detached if the underlying table is removed from the group, or when the
    /// group accessor is destroyed. A subtable accessor becomes detached if the
    /// underlying subtable is removed, or if the parent table accessor is
    /// detached. A table accessor does not become detached for any other reason
    /// than those mentioned here.
    ///
    /// FIXME: High level language bindings will probably want to be able to
    /// explicitely detach a group and all tables of that group if any modifying
    /// operation fails (e.g. memory allocation failure) (and something similar
    /// for freestanding tables) since that leaves the group in state where any
    /// further access is disallowed. This way they will be able to reliably
    /// intercept any attempt at accessing such a failed group.
    ///
    /// FIXME: The C++ documentation must state that if any modifying operation
    /// on a group (incl. tables, subtables, and specs) or on a free standing
    /// table (incl. subtables and specs) fails, then any further access to that
    /// group (except ~Group()) or freestanding table (except ~Table()) has
    /// undefined behaviour and is considered an error on behalf of the
    /// application. Note that even Table::is_attached() is disallowed in this
    /// case.
    bool is_attached() const noexcept;

    /// Get the name of this table, if it has one. Only group-level tables have
    /// names. For a table of any other kind, this function returns the empty
    /// string.
    StringData get_name() const noexcept;

    // Whether or not elements can be null.
    bool is_nullable(size_t col_ndx) const;

    //@{
    /// Conventience functions for inspecting the dynamic table type.
    ///
    /// These functions behave as if they were called on the descriptor returned
    /// by get_descriptor().
    size_t get_column_count() const noexcept;
    DataType    get_column_type(size_t column_ndx) const noexcept;
    StringData  get_column_name(size_t column_ndx) const noexcept;
    size_t get_column_index(StringData name) const noexcept;
    //@}

    //@{
    /// Convenience functions for manipulating the dynamic table type.
    ///
    /// These function must be called only for tables with independent dynamic
    /// type. A table has independent dynamic type if the function
    /// has_shared_type() returns false. A table that is a direct member of a
    /// group has independent dynamic type. So does a free-standing table, and a
    /// subtable in a column of type 'mixed'. All other tables have shared
    /// dynamic type. The consequences of calling any of these functions for a
    /// table with shared dynamic type are undefined.
    ///
    /// Apart from that, these functions behave as if they were called on the
    /// descriptor returned by get_descriptor(). Note especially that the
    /// `_link` suffixed functions must be used when inserting link-type
    /// columns.
    ///
    /// If you need to change the shared dynamic type of the subtables in a
    /// subtable column, consider using the API offered by the Descriptor class.
    ///
    /// \sa has_shared_type()
    /// \sa get_descriptor()

    size_t add_column(DataType type, StringData name, bool nullable = false, DescriptorRef* subdesc = nullptr);
    void insert_column(size_t column_ndx, DataType type, StringData name, bool nullable = false,
                       DescriptorRef* subdesc = nullptr);

    // Todo, these prototypes only exist for backwards compatibility. We should remove them because they are error
    // prone (optional arguments and implicit bool to null-ptr conversion)
    size_t add_column(DataType type, StringData name, DescriptorRef* subdesc)
    {
        return add_column(type, name, false, subdesc);
    }
    void insert_column(size_t column_ndx, DataType type, StringData name, DescriptorRef* subdesc)
    {
        insert_column(column_ndx, type, name, false, subdesc);
    }

    size_t add_column_link(DataType type, StringData name, Table& target, LinkType link_type = link_Weak);
    void insert_column_link(size_t column_ndx, DataType type, StringData name, Table& target,
                            LinkType link_type = link_Weak);
    void remove_column(size_t column_ndx);
    void rename_column(size_t column_ndx, StringData new_name);
    //@}

    //@{

    /// has_search_index() returns true if, and only if a search index has been
    /// added to the specified column. Rather than throwing, it returns false if
    /// the table accessor is detached or the specified index is out of range.
    ///
    /// add_search_index() adds a search index to the specified column of this
    /// table. It has no effect if a search index has already been added to the
    /// specified column (idempotency).
    ///
    /// remove_search_index() removes the search index from the specified column
    /// of this table. It has no effect if the specified column has no search
    /// index. The search index cannot be removed from the primary key of a
    /// table.
    ///
    /// has_primary_key() returns true if, and only if a primary key has been
    /// added to this table. Rather than throwing, it returns false if the table
    /// accessor is detached.
    ///
    /// try_add_primary_key() tries to add a primary key to this table, by
    /// forming it from the specified column. It fails and returns false if the
    /// specified column has duplicate values, otherwise it returns true. The
    /// specified column must already have a search index. This table must have
    /// no preexisting primary key.
    ///
    /// remove_primary_key() removes a previously added primary key. It is an
    /// error if this table has no primary key.
    ///
    /// This table must be a root table; that is, it must have an independent
    /// descriptor. Freestanding tables, group-level tables, and subtables in a
    /// column of type 'mixed' are all examples of root tables. See add_column()
    /// for more on this.
    ///
    /// \param column_ndx The index of a column of this table.

    bool has_search_index(size_t column_ndx) const noexcept;
//    void remove_search_index(size_t col_ndx);
    void add_search_index(size_t column_ndx);
    void remove_search_index(size_t column_ndx);
    bool has_primary_key() const noexcept;
    bool try_add_primary_key(size_t column_ndx);
    void remove_primary_key();

    //@}

    //@{
    /// Get the dynamic type descriptor for this table.
    ///
    /// Every table has an associated descriptor that specifies its dynamic
    /// type. For simple tables, that is, tables without subtable columns, the
    /// dynamic type can be inspected and modified directly using member
    /// functions such as get_column_count() and add_column(). For more complex
    /// tables, the type is best managed through the associated descriptor
    /// object which is returned by this function.
    ///
    /// \sa has_shared_type()
    DescriptorRef get_descriptor();
    ConstDescriptorRef get_descriptor() const;
    //@}

    //@{
    /// Get the dynamic type descriptor for the column with the
    /// specified index. That column must have type 'table'.
    ///
    /// This is merely a shorthand for calling `get_subdescriptor(column_ndx)`
    /// on the descriptor returned by `get_descriptor()`.
    DescriptorRef get_subdescriptor(size_t column_ndx);
    ConstDescriptorRef get_subdescriptor(size_t column_ndx) const;
    //@}

    //@{
    /// Get access to an arbitrarily nested dynamic type descriptor.
    ///
    /// The returned descriptor is the one you would get by calling
    /// Descriptor::get_subdescriptor() once for each entry in the specified
    /// path, starting with the descriptor returned by get_descriptor(). The
    /// path is allowed to be empty.
    typedef std::vector<size_t> path_vec;
    DescriptorRef get_subdescriptor(const path_vec& path);
    ConstDescriptorRef get_subdescriptor(const path_vec& path) const;
    //@}

    //@{
    /// Convenience functions for manipulating nested table types.
    ///
    /// These functions behave as if they were called on the descriptor returned
    /// by `get_subdescriptor(path)`. These function must be called only on
    /// tables with independent dynamic type.
    ///
    /// \return The value returned by add_subcolumn(), is the index of
    /// the added column within the descriptor referenced by the
    /// specified path.
    ///
    /// \sa Descriptor::add_column()
    /// \sa has_shared_type()
    size_t add_subcolumn(const path_vec& path, DataType type, StringData name);
    void insert_subcolumn(const path_vec& path, size_t column_ndx,
                          DataType type, StringData name);
    void remove_subcolumn(const path_vec& path, size_t column_ndx);
    void rename_subcolumn(const path_vec& path, size_t column_ndx, StringData new_name);
    //@}

    /// Does this table share its type with other tables?
    ///
    /// Tables that are direct members of groups have independent
    /// dynamic types. The same is true for free-standing tables and
    /// subtables in coulmns of type 'mixed'. For such tables, this
    /// function returns false.
    ///
    /// When a table has a column of type 'table', the cells in that
    /// column contain subtables. All those subtables have the same
    /// dynamic type, and they share a single type descriptor. For all
    /// such subtables, this function returns true. See
    /// Descriptor::is_root() for more on this.
    ///
    /// Please note that Table functions that modify the dynamic type
    /// directly, such as add_column(), are only allowed to be used on
    /// tables with non-shared type. If you need to modify a shared
    /// type, you will have to do that through the descriptor returned
    /// by get_descriptor(), but note that it will then affect all the
    /// tables sharing that descriptor.
    ///
    /// \sa get_descriptor()
    /// \sa Descriptor::is_root()
    bool has_shared_type() const noexcept;


    template<class T>
    Columns<T> column(size_t column); // FIXME: Should this one have been declared noexcept?

    // Table size and deletion
    bool is_empty() const noexcept;
    size_t size() const noexcept;

    typedef BasicRowExpr<Table> RowExpr;
    typedef BasicRowExpr<const Table> ConstRowExpr;

    RowExpr get(size_t row_ndx) noexcept;
    ConstRowExpr get(size_t row_ndx) const noexcept;

    RowExpr front() noexcept;
    ConstRowExpr front() const noexcept;

    RowExpr back() noexcept;
    ConstRowExpr back() const noexcept;

    RowExpr operator[](size_t row_ndx) noexcept;
    ConstRowExpr operator[](size_t row_ndx) const noexcept;


    //@{

    /// Row handling.
    ///
    /// remove() removes the specified row from the table and shifts all rows at
    /// higher index to fill the vacated slot. This operation assumes that the
    /// table is ordered, and it is therefore allowed only on tables **without**
    /// link columns, as link columns are only allowed in unordered tables.
    ///
    /// move_last_over() removes the specified row from the table, and if it is
    /// not the last row in the table, it then moves the last row into the
    /// vacated slot. This operation assumes that the table is unordered, and it
    /// may therfore be used on tables with link columns.
    ///
    /// The removal of a row from an unordered table (move_last_over()) may
    /// cause other linked rows to be cascade-removed. The clearing of a table
    /// may also cause linked rows to be cascade-removed, but in this respect,
    /// the effect is exactly as if each row had been removed individually. See
    /// Descriptor::set_link_type() for details.
    ///
    /// It is an error to call add_empty_row() or insert_empty_row() on a table
    /// with a primary key, if that would result in a violation the implied
    /// *unique constraint* of the primary key. The consequenses of doing so are
    /// unspecified.

    size_t add_empty_row(size_t num_rows = 1);
    void insert_empty_row(size_t row_ndx, size_t num_rows = 1);
    void remove(size_t row_ndx);
    void remove_last();
    void move_last_over(size_t row_ndx);
    void clear();
    void swap_rows(size_t row_ndx_1, size_t row_ndx_2);

    //@}

    // Get cell values
    int64_t     get_int(size_t column_ndx, size_t row_ndx) const noexcept;
    bool        get_bool(size_t column_ndx, size_t row_ndx) const noexcept;
    DateTime    get_datetime(size_t column_ndx, size_t row_ndx) const noexcept;
    float       get_float(size_t column_ndx, size_t row_ndx) const noexcept;
    double      get_double(size_t column_ndx, size_t row_ndx) const noexcept;
    StringData  get_string(size_t column_ndx, size_t row_ndx) const noexcept;
    BinaryData  get_binary(size_t column_ndx, size_t row_ndx) const noexcept;
    Mixed       get_mixed(size_t column_ndx, size_t row_ndx) const noexcept;
    DataType    get_mixed_type(size_t column_ndx, size_t row_ndx) const noexcept;
    size_t get_link(size_t column_ndx, size_t row_ndx) const noexcept;
    bool is_null_link(size_t column_ndx, size_t row_ndx) const noexcept;
    LinkViewRef get_linklist(size_t column_ndx, size_t row_ndx);
    ConstLinkViewRef get_linklist(size_t column_ndx, size_t row_ndx) const;
    size_t get_link_count(size_t column_ndx, size_t row_ndx) const noexcept;
    bool linklist_is_empty(size_t column_ndx, size_t row_ndx) const noexcept;
    bool is_null(size_t column_ndx, size_t row_ndx) const noexcept;

    TableRef get_link_target(size_t column_ndx) noexcept;
    ConstTableRef get_link_target(size_t column_ndx) const noexcept;

    template<class T>
    typename T::RowAccessor get_link_accessor(size_t column_ndx, size_t row_ndx);

    //@{

    /// Set cell values.
    ///
    /// It is an error to specify a column index, row index, or string position
    /// that is out of range.
    ///
    /// The number of bytes in a string value must not exceed `max_string_size`,
    /// and the number of bytes in a binary data value must not exceed
    /// `max_binary_size`. String must also contain valid UTF-8 encodings. These
    /// requirements also apply when modifying a string with insert_substring()
    /// and remove_substring(), and for strings in a mixed columnt. Passing, or
    /// producing an oversized string or binary data value will cause an
    /// exception to be thrown.
    ///
    /// It is an error to modify a value in a column that participates in a
    /// primary key, if that would result in a violation the implied *unique
    /// constraint* of that primary key. The consequenses of doing so are
    /// unspecified.
    ///
    /// insert_substring() inserts the specified string into the currently
    /// stored string at the specified position. The position must be less than
    /// or equal to the size of the currently stored string.
    ///
    /// remove_substring() removes the specified byte range from the currently
    /// stored string. The beginning of the range (\a pos) must be less than or
    /// equal to the size of the currently stored string. If the specified range
    /// cextends beyond the end of the currently stored string, it will be
    /// silently clamped.
    ///
    /// String level modifications performed via insert_substring() and
    /// remove_substring() are mergable and subject to operational
    /// trsnaformation. That is, the effect of two causally unrelated
    /// modifications will in general both be retained during synchronization.

    static const size_t max_string_size = 0xFFFFF8 - Array::header_size - 1;
    static const size_t max_binary_size = 0xFFFFF8 - Array::header_size;

    void set_int(size_t column_ndx, size_t row_ndx, int_fast64_t value);
    void set_bool(size_t column_ndx, size_t row_ndx, bool value);
    void set_datetime(size_t column_ndx, size_t row_ndx, DateTime value);
    template<class E>
    void set_enum(size_t column_ndx, size_t row_ndx, E value);
    void set_float(size_t column_ndx, size_t row_ndx, float value);
    void set_double(size_t column_ndx, size_t row_ndx, double value);
    void set_string(size_t column_ndx, size_t row_ndx, StringData value);
    void set_binary(size_t column_ndx, size_t row_ndx, BinaryData value);
    void set_mixed(size_t column_ndx, size_t row_ndx, Mixed value);
    void set_link(size_t column_ndx, size_t row_ndx, size_t target_row_ndx);
    void nullify_link(size_t column_ndx, size_t row_ndx);
    void set_null(size_t column_ndx, size_t row_ndx);

    void insert_substring(size_t col_ndx, size_t row_ndx, size_t pos, StringData);
    void remove_substring(size_t col_ndx, size_t row_ndx, size_t pos, size_t size = realm::npos);

    //@}

    /// Assumes that the specified column is a subtable column (in
    /// particular, not a mixed column) and that the specified table
    /// has a spec that is compatible with that column, that is, the
    /// number of columns must be the same, and corresponding columns
    /// must have identical data types (as returned by
    /// get_column_type()).
    void set_subtable(size_t col_ndx, size_t row_ndx, const Table*);
    void set_mixed_subtable(size_t col_ndx, size_t row_ndx, const Table*);


    // Sub-tables (works on columns whose type is either 'subtable' or
    // 'mixed', for a value in a mixed column that is not a subtable,
    // get_subtable() returns null, get_subtable_size() returns zero,
    // and clear_subtable() replaces the value with an empty table.)
    TableRef get_subtable(size_t column_ndx, size_t row_ndx);
    ConstTableRef get_subtable(size_t column_ndx, size_t row_ndx) const;
    size_t get_subtable_size(size_t column_ndx, size_t row_ndx) const noexcept;
    void clear_subtable(size_t column_ndx, size_t row_ndx);

    // Backlinks
    size_t get_backlink_count(size_t row_ndx, const Table& origin,
                              size_t origin_col_ndx) const noexcept;
    size_t get_backlink(size_t row_ndx, const Table& origin,
                        size_t origin_col_ndx, size_t backlink_ndx) const noexcept;


    //@{

    /// If this accessor is attached to a subtable, then that subtable has a
    /// parent table, and the subtable either resides in a column of type
    /// `table` or of type `mixed` in that parent. In that case
    /// get_parent_table() returns a reference to the accessor associated with
    /// the parent, and get_parent_row_index() returns the index of the row in
    /// which the subtable resides. In all other cases (free-standing and
    /// group-level tables), get_parent_table() returns null and
    /// get_parent_row_index() returns realm::npos.
    ///
    /// If this accessor is attached to a subtable, and \a column_ndx_out is
    /// specified, then `*column_ndx_out` is set to the index of the column of
    /// the parent table in which the subtable resides. If this accessor is not
    /// attached to a subtable, then `*column_ndx_out` will retain its original
    /// value upon return.

    TableRef get_parent_table(size_t* column_ndx_out = nullptr) noexcept;
    ConstTableRef get_parent_table(size_t* column_ndx_out = nullptr) const noexcept;
    size_t get_parent_row_index() const noexcept;

    //@}


    /// Only group-level unordered tables can be used as origins or targets of
    /// links.
    bool is_group_level() const noexcept;

    /// If this table is a group-level table, then this function returns the
    /// index of this table within the group. Otherwise it returns realm::npos.
    size_t get_index_in_group() const noexcept;

    // Aggregate functions
    size_t count_int(size_t column_ndx, int64_t value) const;
    size_t count_string(size_t column_ndx, StringData value) const;
    size_t count_float(size_t column_ndx, float value) const;
    size_t count_double(size_t column_ndx, double value) const;

    int64_t sum_int(size_t column_ndx) const;
    double  sum_float(size_t column_ndx) const;
    double  sum_double(size_t column_ndx) const;
    int64_t maximum_int(size_t column_ndx, size_t* return_ndx = nullptr) const;
    float   maximum_float(size_t column_ndx, size_t* return_ndx = nullptr) const;
    double  maximum_double(size_t column_ndx, size_t* return_ndx = nullptr) const;
    DateTime maximum_datetime(size_t column_ndx, size_t* return_ndx = nullptr) const;
    int64_t minimum_int(size_t column_ndx, size_t* return_ndx = nullptr) const;
    float   minimum_float(size_t column_ndx, size_t* return_ndx = nullptr) const;
    double  minimum_double(size_t column_ndx, size_t* return_ndx = nullptr) const;
    DateTime minimum_datetime(size_t column_ndx, size_t* return_ndx = nullptr) const;
    double  average_int(size_t column_ndx, size_t* value_count = nullptr) const;
    double  average_float(size_t column_ndx, size_t* value_count = nullptr) const;
    double  average_double(size_t column_ndx, size_t* value_count = nullptr) const;

    // Searching
    size_t    find_first_link(size_t target_row_index) const;
    size_t    find_first_int(size_t column_ndx, int64_t value) const;
    size_t    find_first_bool(size_t column_ndx, bool value) const;
    size_t    find_first_datetime(size_t column_ndx, DateTime value) const;
    size_t    find_first_float(size_t column_ndx, float value) const;
    size_t    find_first_double(size_t column_ndx, double value) const;
    size_t    find_first_string(size_t column_ndx, StringData value) const;
    size_t    find_first_binary(size_t column_ndx, BinaryData value) const;
    size_t    find_first_null(size_t column_ndx) const;

    TableView      find_all_link(size_t target_row_index);
    ConstTableView find_all_link(size_t target_row_index) const;
    TableView      find_all_int(size_t column_ndx, int64_t value);
    ConstTableView find_all_int(size_t column_ndx, int64_t value) const;
    TableView      find_all_bool(size_t column_ndx, bool value);
    ConstTableView find_all_bool(size_t column_ndx, bool value) const;
    TableView      find_all_datetime(size_t column_ndx, DateTime value);
    ConstTableView find_all_datetime(size_t column_ndx, DateTime value) const;
    TableView      find_all_float(size_t column_ndx, float value);
    ConstTableView find_all_float(size_t column_ndx, float value) const;
    TableView      find_all_double(size_t column_ndx, double value);
    ConstTableView find_all_double(size_t column_ndx, double value) const;
    TableView      find_all_string(size_t column_ndx, StringData value);
    ConstTableView find_all_string(size_t column_ndx, StringData value) const;
    TableView      find_all_binary(size_t column_ndx, BinaryData value);
    ConstTableView find_all_binary(size_t column_ndx, BinaryData value) const;
    TableView      find_all_null(size_t column_ndx);
    ConstTableView find_all_null(size_t column_ndx) const;

    /// The following column types are supported: String, Integer, DateTime, Bool
    TableView      get_distinct_view(size_t column_ndx);
    ConstTableView get_distinct_view(size_t column_ndx) const;

    TableView      get_sorted_view(size_t column_ndx, bool ascending = true);
    ConstTableView get_sorted_view(size_t column_ndx, bool ascending = true) const;

    TableView      get_sorted_view(std::vector<size_t> column_ndx, std::vector<bool> ascending);
    ConstTableView get_sorted_view(std::vector<size_t> column_ndx, std::vector<bool> ascending) const;

    TableView      get_range_view(size_t begin, size_t end);
    ConstTableView get_range_view(size_t begin, size_t end) const;

    TableView      get_backlink_view(size_t row_ndx, Table *src_table,
                                     size_t src_col_ndx);

    //@{

    /// Find the row with the specified primary key.
    ///
    /// It is an error to call any of these function on a table that has no
    /// primary key, or to call one of them for a column with a mismatching
    /// type.

    RowExpr find_pkey_int(int_fast64_t pkey_value);
    ConstRowExpr find_pkey_int(int_fast64_t pkey_value) const;

    RowExpr find_pkey_string(StringData pkey_value);
    ConstRowExpr find_pkey_string(StringData pkey_value) const;

    //@}


    // Pivot / aggregate operation types. Experimental! Please do not document method publicly.
    enum AggrType {
        aggr_count,
        aggr_sum,
        aggr_avg,
        aggr_min,
        aggr_max
    };

    // Simple pivot aggregate method. Experimental! Please do not document method publicly.
    void aggregate(size_t group_by_column, size_t aggr_column, AggrType op, Table& result, const IntegerColumn* viewrefs = nullptr) const;


private:
    template<class T>
    size_t find_first(size_t column_ndx, T value) const; // called by above methods
    template<class T>
    TableView find_all(size_t column_ndx, T value);
public:


    //@{
    /// Find the lower/upper bound according to a column that is
    /// already sorted in ascending order.
    ///
    /// For an integer column at index 0, and an integer value '`v`',
    /// lower_bound_int(0,v) returns the index '`l`' of the first row
    /// such that `get_int(0,l) &ge; v`, and upper_bound_int(0,v)
    /// returns the index '`u`' of the first row such that
    /// `get_int(0,u) &gt; v`. In both cases, if no such row is found,
    /// the returned value is the number of rows in the table.
    ///
    ///     3 3 3 4 4 4 5 6 7 9 9 9
    ///     ^     ^     ^     ^     ^
    ///     |     |     |     |     |
    ///     |     |     |     |      -- Lower and upper bound of 15
    ///     |     |     |     |
    ///     |     |     |      -- Lower and upper bound of 8
    ///     |     |     |
    ///     |     |      -- Upper bound of 4
    ///     |     |
    ///     |      -- Lower bound of 4
    ///     |
    ///      -- Lower and upper bound of 1
    ///
    /// These functions are similar to std::lower_bound() and
    /// std::upper_bound().
    ///
    /// The string versions assume that the column is sorted according
    /// to StringData::operator<().
    size_t lower_bound_int(size_t column_ndx, int64_t value) const noexcept;
    size_t upper_bound_int(size_t column_ndx, int64_t value) const noexcept;
    size_t lower_bound_bool(size_t column_ndx, bool value) const noexcept;
    size_t upper_bound_bool(size_t column_ndx, bool value) const noexcept;
    size_t lower_bound_float(size_t column_ndx, float value) const noexcept;
    size_t upper_bound_float(size_t column_ndx, float value) const noexcept;
    size_t lower_bound_double(size_t column_ndx, double value) const noexcept;
    size_t upper_bound_double(size_t column_ndx, double value) const noexcept;
    size_t lower_bound_string(size_t column_ndx, StringData value) const noexcept;
    size_t upper_bound_string(size_t column_ndx, StringData value) const noexcept;
    //@}

    // Queries
    // Using where(tv) is the new method to perform queries on TableView. The 'tv' can have any order; it does not
    // need to be sorted, and, resulting view retains its order.
    Query where(TableViewBase* tv = nullptr) { return Query(*this, tv); }

    // FIXME: We need a ConstQuery class or runtime check against modifications in read transaction.
    Query where(TableViewBase* tv = nullptr) const { return Query(*this, tv); }

    // Perform queries on a LinkView. The returned Query holds a reference to lv.
    Query where(const LinkViewRef& lv) { return Query(*this, lv); }

    Table& link(size_t link_column);

    // Optimizing. enforce == true will enforce enumeration of all string columns;
    // enforce == false will auto-evaluate if they should be enumerated or not
    void optimize(bool enforce = false);

    /// Write this table (or a slice of this table) to the specified
    /// output stream.
    ///
    /// The output will have the same format as any other Realm
    /// database file, such as those produced by Group::write(). In
    /// this case, however, the resulting database file will contain
    /// exactly one table, and that table will contain only the
    /// specified slice of the source table (this table).
    ///
    /// The new table will always have the same dynamic type (see
    /// Descriptor) as the source table (this table), and unless it is
    /// overridden (\a override_table_name), the new table will have
    /// the same name as the source table (see get_name()). Indexes
    /// (see add_search_index()) will not be carried over to the new
    /// table.
    ///
    /// \param offset Index of first row to include (if `size >
    /// 0`). Must be less than, or equal to size().
    ///
    /// \param size Number of rows to include. May be zero. If `size >
    /// size() - offset`, then the effective size of the written slice
    /// will be `size() - offset`.
    ///
    /// \throw std::out_of_range If `offset > size()`.
    ///
    /// FIXME: While this function does provided a maximally efficient
    /// way of serializing part of a table, it offers little in terms
    /// of general utility. This is unfortunate, because it pulls
    /// quite a large amount of code into the core library to support
    /// it.
    void write(std::ostream&, size_t offset = 0, size_t size = npos,
               StringData override_table_name = StringData()) const;

    // Conversion
    void to_json(std::ostream& out, size_t link_depth = 0, std::map<std::string,
                 std::string>* renames = nullptr) const;
    void to_string(std::ostream& out, size_t limit = 500) const;
    void row_to_string(size_t row_ndx, std::ostream& out) const;

    // Get a reference to this table
    TableRef get_table_ref() { return TableRef(this); }
    ConstTableRef get_table_ref() const { return ConstTableRef(this); }

    /// \brief Compare two tables for equality.
    ///
    /// Two tables are equal if they have equal descriptors
    /// (`Descriptor::operator==()`) and equal contents. Equal descriptors imply
    /// that the two tables have the same columns in the same order. Equal
    /// contents means that the two tables must have the same number of rows,
    /// and that for each row index, the two rows must have the same values in
    /// each column.
    ///
    /// In mixed columns, both the value types and the values are required to be
    /// equal.
    ///
    /// For a particular row and column, if the two values are themselves tables
    /// (subtable and mixed columns) value equality implies a recursive
    /// invocation of `Table::operator==()`.
    bool operator==(const Table&) const;

    /// \brief Compare two tables for inequality.
    ///
    /// See operator==().
    bool operator!=(const Table& t) const;

    /// A subtable in a column of type 'table' (which shares descriptor with
    /// other subtables in the same column) is initially in a degenerate state
    /// where it takes up a minimal amout of space. This function returns true
    /// if, and only if the table accessor is attached to such a subtable. This
    /// function is mainly intended for debugging purposes.
    bool is_degenerate() const noexcept;

    // Debug
#ifdef REALM_DEBUG
    void verify() const; // Must be upper case to avoid conflict with macro in ObjC
    void to_dot(std::ostream&, StringData title = StringData()) const;
    void print() const;
    MemStats stats() const;
    void dump_node_structure() const; // To std::cerr (for GDB)
    void dump_node_structure(std::ostream&, int level) const;
#else
    void verify() const {}
#endif

    class Parent;

protected:
    /// Get a pointer to the accessor of the specified subtable. The
    /// accessor will be created if it does not already exist.
    ///
    /// The returned table pointer must **always** end up being
    /// wrapped in some instantiation of BasicTableRef<>.
    Table* get_subtable_ptr(size_t col_ndx, size_t row_ndx);

    /// See non-const get_subtable_ptr().
    const Table* get_subtable_ptr(size_t col_ndx, size_t row_ndx) const;

    /// Compare the rows of two tables under the assumption that the two tables
    /// have the same number of columns, and the same data type at each column
    /// index (as expressed through the DataType enum).
    bool compare_rows(const Table&) const;

    void set_into_mixed(Table* parent, size_t col_ndx, size_t row_ndx) const;

private:
    class SliceWriter;

    // Number of rows in this table
    size_t m_size;

    // Underlying array structure. `m_top` is in use only for root tables; that
    // is, for tables with independent descriptor. `m_columns` contains a ref
    // for each column and search index in order of the columns. A search index
    // ref always occurs immediately after the ref of the column to which the
    // search index belongs.
    //
    // A subtable column (a column of type `type_table`) is essentially just a
    // column of 'refs' pointing to the root node of each subtable.
    //
    // To save space in the database file, a subtable in such a column always
    // starts out in a degenerate form where nothing is allocated on its behalf,
    // and a null 'ref' is stored in the corresponding slot of the column. A
    // subtable remains in this degenerate state until the first row is added to
    // the subtable.
    //
    // For this scheme to work, it must be (and is) possible to create a table
    // accessor that refers to a degenerate subtable. A table accessor (instance
    // of `Table`) refers to a degenerate subtable if, and only if `m_columns`
    // is unattached.
    //
    // FIXME: The fact that `m_columns` may be detached means that many
    // functions (even non-modifying functions) need to check for that before
    // accessing the contents of the table. This incurs a runtime
    // overhead. Consider whether this overhead can be eliminated by having
    // `Table::m_columns` always attached to something, and then detect the
    // degenerate state in a different way.
    Array m_top;
    Array m_columns; // 2nd slot in m_top (for root tables)
    Spec m_spec;     // 1st slot in m_top (for root tables)

    // Is guaranteed to be empty for a detached accessor. Otherwise it is empty
    // when the table accessor is attached to a degenerate subtable (unattached
    // `m_columns`), otherwise it contains precisely one column accessor for
    // each column in the table, in order.
    //
    // In some cases an entry may be null. This is currently possible only in
    // connection with Group::advance_transact(), but it means that several
    // member functions must be prepared to handle these null entries; in
    // particular, detach(), ~Table(), functions called on behalf of detach()
    // and ~Table(), and functiones called on behalf of
    // Group::advance_transact().
    typedef std::vector<ColumnBase*> column_accessors;
    column_accessors m_cols;

    mutable size_t m_ref_count;
    mutable const StringIndex* m_primary_key;

    // If this table is a root table (has independent descriptor),
    // then Table::m_descriptor refers to the accessor of its
    // descriptor when, and only when the descriptor accessor
    // exists. This is used to ensure that at most one descriptor
    // accessor exists for each underlying descriptor at any given
    // point in time. Subdescriptors are kept unique by means of a
    // registry in the parent descriptor. Table::m_descriptor is
    // always null for tables with shared descriptor.
    mutable Descriptor* m_descriptor;

    // Table view instances
    typedef std::vector<TableViewBase*> views;
    mutable views m_views;

    // Points to first bound row accessor, or is null if there are none.
    mutable RowBase* m_row_accessors = nullptr;

    // Mutex which must be locked any time the row accessor chain or m_views is used
    mutable util::Mutex m_accessor_mutex;

    // Used for queries: Items are added with link() method during buildup of query
    mutable std::vector<size_t> m_link_chain;

    /// Used only in connection with Group::advance_transact() and
    /// Table::refresh_accessor_tree().
    mutable bool m_mark;

    mutable uint_fast64_t m_version;

    void erase_row(size_t row_ndx, bool is_move_last_over);
    void batch_erase_rows(const IntegerColumn& row_indexes, bool is_move_last_over);
    void do_remove(size_t row_ndx, bool broken_reciprocal_backlinks);
    void do_move_last_over(size_t row_ndx, bool broken_reciprocal_backlinks);
    void do_swap_rows(size_t row_ndx_1, size_t row_ndx_2);
    void do_clear(bool broken_reciprocal_backlinks);
    size_t do_set_link(size_t col_ndx, size_t row_ndx, size_t target_row_ndx);

    void upgrade_file_format();

    /// Update the version of this table and all tables which have links to it.
    /// This causes all views referring to those tables to go out of sync, so that
    /// calls to sync_if_needed() will bring the view up to date by reexecuting the
    /// query.
    ///
    /// \param bump_global chooses whether the global versioning counter must be
    /// bumped first as part of the update. This is the normal mode of operation,
    /// when a change is made to the table. When calling recursively (following links
    /// or going to the parent table), the parameter should be set to false to correctly
    /// prune traversal.
    void bump_version(bool bump_global = true) const noexcept;

    /// Disable copying assignment.
    ///
    /// It could easily be implemented by calling assign(), but the
    /// non-checking nature of the low-level dynamically typed API
    /// makes it too risky to offer this feature as an
    /// operator.
    ///
    /// FIXME: assign() has not yet been implemented, but the
    /// intention is that it will copy the rows of the argument table
    /// into this table after clearing the original contents, and for
    /// target tables without a shared spec, it would also copy the
    /// spec. For target tables with shared spec, it would be an error
    /// to pass an argument table with an incompatible spec, but
    /// assign() would not check for spec compatibility. This would
    /// make it ideal as a basis for implementing operator=() for
    /// typed tables.
    Table& operator=(const Table&);

    /// Used when constructing an accessor whose lifetime is going to be managed
    /// by reference counting. The lifetime of accessors of free-standing tables
    /// allocated on the stack by the application is not managed by reference
    /// counting, so that is a case where this tag must **not** be specified.
    class ref_count_tag {};

    /// Create an uninitialized accessor whose lifetime is managed by reference
    /// counting.
    Table(ref_count_tag, Allocator&);

    void init(ref_type top_ref, ArrayParent*, size_t ndx_in_parent,
              bool skip_create_column_accessors = false);
    void init(ConstSubspecRef shared_spec, ArrayParent* parent_column,
              size_t parent_row_ndx);

    void reveal_primary_key() const;
    size_t do_find_pkey_int(int_fast64_t) const;
    size_t do_find_pkey_string(StringData) const;

    static void do_insert_column(Descriptor&, size_t col_ndx, DataType type,
                                 StringData name, Table* link_target_table, bool nullable = false);
    static void do_insert_column_unless_exists(Descriptor&, size_t col_ndx, DataType type,
                                               StringData name, Table* link_target_table, bool nullable = false,
                                               bool* was_inserted = nullptr);
    static void do_erase_column(Descriptor&, size_t col_ndx);
    static void do_rename_column(Descriptor&, size_t col_ndx, StringData name);
    static void do_move_column(Descriptor&, size_t col_ndx_1, size_t col_ndx_2);

    struct InsertSubtableColumns;
    struct EraseSubtableColumns;
    struct RenameSubtableColumns;
    struct MoveSubtableColumns;

    void insert_root_column(size_t col_ndx, DataType type, StringData name,
                            Table* link_target_table, bool nullable = false);
    void erase_root_column(size_t col_ndx);
    void move_root_column(size_t from, size_t to);
    void do_insert_root_column(size_t col_ndx, ColumnType, StringData name, bool nullable = false);
    void do_erase_root_column(size_t col_ndx);
    void do_move_root_column(size_t from, size_t to);
    void do_set_link_type(size_t col_ndx, LinkType);
    void insert_backlink_column(size_t origin_table_ndx, size_t origin_col_ndx);
    void erase_backlink_column(size_t origin_table_ndx, size_t origin_col_ndx);
    void update_link_target_tables(size_t old_col_ndx_begin, size_t new_col_ndx_begin);
    void update_link_target_tables_after_column_move(size_t moved_from, size_t moved_to);

    struct SubtableUpdater {
        virtual void update(const SubtableColumn&, Array& subcolumns) = 0;
        virtual void update_accessor(Table&) = 0;
        virtual ~SubtableUpdater() {}
    };
    static void update_subtables(Descriptor&, SubtableUpdater*);
    void update_subtables(const size_t* col_path_begin, const size_t* col_path_end,
                          SubtableUpdater*);

    struct AccessorUpdater {
        virtual void update(Table&) = 0;
        virtual void update_parent(Table&) = 0;
        virtual ~AccessorUpdater() {}
    };
    void update_accessors(const size_t* col_path_begin, const size_t* col_path_end,
                          AccessorUpdater&);

    void create_degen_subtab_columns();
    ColumnBase* create_column_accessor(ColumnType, size_t col_ndx, size_t ndx_in_parent);
    void destroy_column_accessors() noexcept;

    /// Called in the context of Group::commit() to ensure that
    /// attached table accessors stay valid across a commit. Please
    /// note that this works only for non-transactional commits. Table
    /// accessors obtained during a transaction are always detached
    /// when the transaction ends.
    void update_from_parent(size_t old_baseline) noexcept;

    // Support function for conversions
    void to_string_header(std::ostream& out, std::vector<size_t>& widths) const;
    void to_string_row(size_t row_ndx, std::ostream& out,
                       const std::vector<size_t>& widths) const;

    // recursive methods called by to_json, to follow links
    void to_json(std::ostream& out, size_t link_depth, std::map<std::string, std::string>& renames,
                 std::vector<ref_type>& followed) const;
    void to_json_row(size_t row_ndx, std::ostream& out, size_t link_depth,
                     std::map<std::string, std::string>& renames, std::vector<ref_type>& followed) const;
    void to_json_row(size_t row_ndx, std::ostream& out, size_t link_depth = 0,
                     std::map<std::string, std::string>* renames = nullptr) const;

    // Detach accessor from underlying table. Caller must ensure that
    // a reference count exists upon return, for example by obtaining
    // an extra reference count before the call.
    //
    // This function puts this table accessor into the detached
    // state. This detaches it from the underlying structure of array
    // nodes. It also recursively detaches accessors for subtables,
    // and the type descriptor accessor. When this function returns,
    // is_attached() will return false.
    //
    // This function may be called for a table accessor that is
    // already in the detached state (idempotency).
    //
    // It is also valid to call this function for a table accessor
    // that has not yet been detached, but whose underlying structure
    // of arrays have changed in an unpredictable/unknown way. This
    // kind of change generally happens when a modifying table
    // operation fails, and also when one transaction is ended and a
    // new one is started.
    void detach() noexcept;

    /// Detach and remove all attached row, link list, and subtable
    /// accessors. This function does not discard the descriptor accessor, if
    /// any, and it does not discard column accessors either.
    void discard_child_accessors() noexcept;

    void discard_row_accessors() noexcept;

    // Detach the type descriptor accessor if it exists.
    void discard_desc_accessor() noexcept;

    void bind_ptr() const noexcept { ++m_ref_count; }
    void unbind_ptr() const noexcept { if (--m_ref_count == 0) delete this; }

    void register_view(const TableViewBase* view);
    void unregister_view(const TableViewBase* view) noexcept;
    void move_registered_view(const TableViewBase* old_addr,
                              const TableViewBase* new_addr) noexcept;
    void discard_views() noexcept;

    void register_row_accessor(RowBase*) const noexcept;
    void unregister_row_accessor(RowBase*) const noexcept;
    void do_unregister_row_accessor(RowBase*) const noexcept;

    class UnbindGuard;

    ColumnType get_real_column_type(size_t column_ndx) const noexcept;

    /// If this table is a group-level table, the parent group is returned,
    /// otherwise null is returned.
    Group* get_parent_group() const noexcept;

    const ColumnBase& get_column_base(size_t column_ndx) const noexcept;
    ColumnBase& get_column_base(size_t column_ndx);

    template<class T, ColumnType col_type>
    T& get_column(size_t ndx);

    template<class T, ColumnType col_type>
    const T& get_column(size_t ndx) const noexcept;

    IntegerColumn& get_column(size_t column_ndx);
    const IntegerColumn& get_column(size_t column_ndx) const noexcept;
    IntNullColumn& get_column_int_null(size_t column_ndx);
    const IntNullColumn& get_column_int_null(size_t column_ndx) const noexcept;
    FloatColumn& get_column_float(size_t column_ndx);
    const FloatColumn& get_column_float(size_t column_ndx) const noexcept;
    DoubleColumn& get_column_double(size_t column_ndx);
    const DoubleColumn& get_column_double(size_t column_ndx) const noexcept;
    StringColumn& get_column_string(size_t column_ndx);
    const StringColumn& get_column_string(size_t column_ndx) const noexcept;
    BinaryColumn& get_column_binary(size_t column_ndx);
    const BinaryColumn& get_column_binary(size_t column_ndx) const noexcept;
    StringEnumColumn& get_column_string_enum(size_t column_ndx);
    const StringEnumColumn& get_column_string_enum(size_t column_ndx) const noexcept;
    SubtableColumn& get_column_table(size_t column_ndx);
    const SubtableColumn& get_column_table(size_t column_ndx) const noexcept;
    MixedColumn& get_column_mixed(size_t column_ndx);
    const MixedColumn& get_column_mixed(size_t column_ndx) const noexcept;
    const LinkColumnBase& get_column_link_base(size_t ndx) const noexcept;
    LinkColumnBase& get_column_link_base(size_t ndx);
    const LinkColumn& get_column_link(size_t ndx) const noexcept;
    LinkColumn& get_column_link(size_t ndx);
    const LinkListColumn& get_column_link_list(size_t ndx) const noexcept;
    LinkListColumn& get_column_link_list(size_t ndx);
    const BacklinkColumn& get_column_backlink(size_t ndx) const noexcept;
    BacklinkColumn& get_column_backlink(size_t ndx);

    void instantiate_before_change();
    void validate_column_type(const ColumnBase& column, ColumnType expected_type,
                              size_t ndx) const;

    static size_t get_size_from_ref(ref_type top_ref, Allocator&) noexcept;
    static size_t get_size_from_ref(ref_type spec_ref, ref_type columns_ref,
                                         Allocator&) noexcept;

    const Table* get_parent_table_ptr(size_t* column_ndx_out = nullptr) const noexcept;
    Table* get_parent_table_ptr(size_t* column_ndx_out = nullptr) noexcept;

    /// Create an empty table with independent spec and return just
    /// the reference to the underlying memory.
    static ref_type create_empty_table(Allocator&);

    /// Create a column of the specified type, fill it with the
    /// specified number of default values, and return just the
    /// reference to the underlying memory.
    static ref_type create_column(ColumnType column_type, size_t num_default_values, bool nullable, Allocator&);

    /// Construct a copy of the columns array of this table using the
    /// specified allocator and return just the ref to that array.
    ///
    /// In the clone, no string column will be of the enumeration
    /// type.
    ref_type clone_columns(Allocator&) const;

    /// Construct a complete copy of this table (including its spec)
    /// using the specified allocator and return just the ref to the
    /// new top array.
    ref_type clone(Allocator&) const;

    /// True for `col_type_Link` and `col_type_LinkList`.
    static bool is_link_type(ColumnType) noexcept;

    void connect_opposite_link_columns(size_t link_col_ndx, Table& target_table,
                                       size_t backlink_col_ndx) noexcept;

    size_t get_num_strong_backlinks(size_t row_ndx) const noexcept;

    //@{

    /// Cascading removal of strong links.
    ///
    /// cascade_break_backlinks_to() removes all backlinks pointing to the row
    /// at \a row_ndx. Additionally, if this causes the number of **strong**
    /// backlinks originating from a particular opposite row (target row of
    /// corresponding forward link) to drop to zero, and that row is not already
    /// in \a state.rows, then that row is added to \a state.rows, and
    /// cascade_break_backlinks_to() is called recursively for it. This
    /// operation is the first half of the cascading row removal operation. The
    /// second half is performed by passing the resulting contents of \a
    /// state.rows to remove_backlink_broken_rows().
    ///
    /// Operations that trigger cascading row removal due to explicit removal of
    /// one or more rows (the *initiating rows*), should add those rows to \a
    /// rows initially, and then call cascade_break_backlinks_to() once for each
    /// of them in turn. This is opposed to carrying out the explicit row
    /// removals independently, which is also possible, but does require that
    /// any initiating rows, that end up in \a state.rows due to link cycles,
    /// are removed before passing \a state.rows to
    /// remove_backlink_broken_rows(). In the case of clear(), where all rows of
    /// a table are explicitly removed, it is better to use
    /// cascade_break_backlinks_to_all_rows(), and then carry out the table
    /// clearing as an independent step. For operations that trigger cascading
    /// row removal for other reasons than explicit row removal, \a state.rows
    /// must be empty initially, but cascade_break_backlinks_to() must still be
    /// called for each of the initiating rows.
    ///
    /// When the last non-recursive invocation of cascade_break_backlinks_to()
    /// returns, all forward links originating from a row in \a state.rows have
    /// had their reciprocal backlinks removed, so remove_backlink_broken_rows()
    /// does not perform reciprocal backlink removal at all. Additionally, all
    /// remaining backlinks originating from rows in \a state.rows are
    /// guaranteed to point to rows that are **not** in \a state.rows. This is
    /// true because any backlink that was pointing to a row in \a state.rows
    /// has been removed by one of the invocations of
    /// cascade_break_backlinks_to(). The set of forward links, that correspond
    /// to these remaining backlinks, is precisely the set of forward links that
    /// need to be removed/nullified by remove_backlink_broken_rows(), which it
    /// does by way of reciprocal forward link removal. Note also, that while
    /// all the rows in \a state.rows can have remaining **weak** backlinks
    /// originating from them, only the initiating rows in \a state.rows can
    /// have remaining **strong** backlinks originating from them. This is true
    /// because a non-initiating row is added to \a state.rows only when the
    /// last backlink originating from it is lost.
    ///
    /// Each row removal is replicated individually (as opposed to one
    /// replication instruction for the entire cascading operation). This is
    /// done because it provides an easy way for Group::advance_transact() to
    /// know which tables are affected by the cascade. Note that this has
    /// several important consequences: First of all, the replication log
    /// receiver must execute the row removal instructions in a non-cascading
    /// fashion, meaning that there will be an asymmetry between the two sides
    /// in how the effect of the cascade is brought about. While this is fine
    /// for simple 1-to-1 replication, it may end up interfering badly with
    /// *transaction merging*, when that feature is introduced. Imagine for
    /// example that the cascade initiating operation gets canceled during
    /// conflict resolution, but some, or all of the induced row removals get to
    /// stay. That would break causal consistency. It is important, however, for
    /// transaction merging that the cascaded row removals are explicitly
    /// mentioned in the replication log, such that they can be used to adjust
    /// row indexes during the *operational transform*.
    ///
    /// cascade_break_backlinks_to_all_rows() has the same affect as calling
    /// cascade_break_backlinks_to() once for each row in the table. When
    /// calling this function, \a state.stop_on_table must be set to the origin
    /// table (origin table of corresponding forward links), and \a
    /// state.stop_on_link_list_column must be null.
    ///
    /// It is immaterial which table remove_backlink_broken_rows() is called on,
    /// as long it that table is in the same group as the removed rows.

    void cascade_break_backlinks_to(size_t row_ndx, CascadeState& state);
    void cascade_break_backlinks_to_all_rows(CascadeState& state);
    void remove_backlink_broken_rows(const CascadeState&);

    //@}

    /// Used by query. Follows chain of link columns and returns final target table
    const Table* get_link_chain_target(const std::vector<size_t>& link_chain) const;

    /// Remove the specified row by the 'move last over' method.
    void do_move_last_over(size_t row_ndx);

    // Precondition: 1 <= end - begin
    size_t* record_subtable_path(size_t* begin, size_t* end) const noexcept;

    /// Check if an accessor exists for the specified subtable. If it does,
    /// return a pointer to it, otherwise return null. This function assumes
    /// that the specified column index in a valid index into `m_cols` but does
    /// not otherwise assume more than minimal accessor consistency (see
    /// AccessorConsistencyLevels.)
    Table* get_subtable_accessor(size_t col_ndx, size_t row_ndx) noexcept;

    /// Unless the column accessor is missing, this function returns the
    /// accessor for the target table of the specified link-type column. The
    /// column accessor is said to be missing if `m_cols[col_ndx]` is null, and
    /// this can happen only during certain operations such as the updating of
    /// the accessor tree when a read transaction is advanced. Note that for
    /// link type columns, the target table accessor exists when, and only when
    /// the origin table accessor exists. This function assumes that the
    /// specified column index in a valid index into `m_cols` and that the
    /// column is a link-type column. Beyond that, it assume nothing more than
    /// minimal accessor consistency (see AccessorConsistencyLevels.)
    Table* get_link_target_table_accessor(size_t col_ndx) noexcept;

    void discard_subtable_accessor(size_t col_ndx, size_t row_ndx) noexcept;

    void adj_acc_insert_rows(size_t row_ndx, size_t num_rows) noexcept;
    void adj_acc_erase_row(size_t row_ndx) noexcept;
    void adj_acc_swap_rows(size_t row_ndx_1, size_t row_ndx_2) noexcept;

    /// Adjust this table accessor and its subordinates after move_last_over()
    /// (or its inverse).
    ///
    /// First, any row, subtable, or link list accessors registered as being at
    /// \a to_row_ndx will be detached, as that row is assumed to have been
    /// replaced. Next, any row, subtable, or link list accessors registered as
    /// being at \a from_row_ndx, will be reregistered as being at \a
    /// to_row_ndx, as the row at \a from_row_ndx is assumed to have been moved
    /// to \a to_row_ndx.
    ///
    /// Crucially, if \a to_row_ndx is equal to \a from_row_ndx, then row,
    /// subtable, or link list accessors at that row are **still detached**.
    ///
    /// Additionally, this function causes all link-adjacent tables to be marked
    /// (dirty). Two tables are link-adjacent if one is the target table of a
    /// link column of the other table. Note that this marking follows these
    /// relations in both directions, but only to a depth of one.
    ///
    /// When this function is used in connection with move_last_over(), set \a
    /// to_row_ndx to the index of the row to be removed, and set \a
    /// from_row_ndx to the index of the last row in the table. As mentioned
    /// earlier, this function can also be used in connection with the **inverse
    /// of** move_last_over(), which is an operation that vacates a row by
    /// moving its contents into a new last row of the table. In that case, set
    /// \a to_row_ndx to one plus the index of the last row in the table, and
    /// set \a from_row_ndx to the index of the row to be vacated.
    ///
    /// This function is used as part of Table::refresh_accessor_tree() to
    /// promote the state of the accessors from Minimal Consistency into
    /// Structural Correspondence, so it must be able to execute without
    /// accessing the underlying array nodes.
    void adj_acc_move_over(size_t from_row_ndx, size_t to_row_ndx) noexcept;

    void adj_acc_clear_root_table() noexcept;
    void adj_acc_clear_nonroot_table() noexcept;
    void adj_row_acc_insert_rows(size_t row_ndx, size_t num_rows) noexcept;
    void adj_row_acc_erase_row(size_t row_ndx) noexcept;
    void adj_row_acc_swap_rows(size_t row_ndx_1, size_t row_ndx_2) noexcept;

    /// Called by adj_acc_move_over() to adjust row accessors.
    void adj_row_acc_move_over(size_t from_row_ndx, size_t to_row_ndx) noexcept;

    void adj_insert_column(size_t col_ndx);
    void adj_erase_column(size_t col_ndx) noexcept;
    void adj_move_column(size_t col_ndx_1, size_t col_ndx_2) noexcept;

    bool is_marked() const noexcept;
    void mark() noexcept;
    void unmark() noexcept;
    void recursive_mark() noexcept;
    void mark_link_target_tables(size_t col_ndx_begin) noexcept;
    void mark_opposite_link_tables() noexcept;

    Replication* get_repl() noexcept;

    void set_ndx_in_parent(size_t ndx_in_parent) noexcept;

    /// Refresh the part of the accessor tree that is rooted at this
    /// table. Subtable accessors will be refreshed only if they are marked
    /// (Table::m_mark), and this applies recursively to subtables of
    /// subtables. All refreshed table accessors (including this one) will be
    /// unmarked upon return.
    ///
    /// The following conditions are necessary and sufficient for the proper
    /// operation of this function:
    ///
    ///  - This table must be a group-level table, or a subtable. It must not be
    ///    a free-standing table (because a free-standing table has no parent).
    ///
    ///  - The `index in parent` property is correct. The `index in parent`
    ///    property of the table is the `index in parent` property of
    ///    `m_columns` for subtables with shared descriptor, and the `index in
    ///    parent` property of `m_top` for all other tables.
    ///
    ///  - If this table has shared descriptor, then the `index in parent`
    ///    property of the contained spec accessor is correct.
    ///
    ///  - The parent accessor is in a valid state (already refreshed). If the
    ///    parent is a group, then the group accessor (excluding its table
    ///    accessors) must be in a valid state. If the parent is a table, then
    ///    the table accessor (excluding its subtable accessors) must be in a
    ///    valid state.
    ///
    ///  - Every descendant subtable accessor is marked if it needs to be
    ///    refreshed, or if it has a descendant accessor that needs to be
    ///    refreshed.
    ///
    ///  - This table accessor, as well as all its descendant accessors, are in
    ///    structural correspondence with the underlying node hierarchy whose
    ///    root ref is stored in the parent (see AccessorConsistencyLevels).
    void refresh_accessor_tree();

    void refresh_column_accessors(size_t col_ndx_begin = 0);

    bool is_cross_table_link_target() const noexcept;

#ifdef REALM_DEBUG
    void to_dot_internal(std::ostream&) const;
#endif

    friend class SubtableNode;
    friend class _impl::TableFriend;
    friend class Query;
    template<class>
    friend class util::bind_ptr;
    friend class LangBindHelper;
    friend class TableViewBase;
    template<class T>
    friend class Columns;
    friend class Columns<StringData>;
    friend class ParentNode;
    template<class>
    friend class SequentialGetter;
    friend class RowBase;
    friend class LinksToNode;
    friend class LinkMap;
    friend class LinkView;
    friend class Group;
};



class Table::Parent: public ArrayParent {
public:
    ~Parent() noexcept override {}

protected:
    virtual StringData get_child_name(size_t child_ndx) const noexcept;

    /// If children are group-level tables, then this function returns the
    /// group. Otherwise it returns null.
    virtual Group* get_parent_group() noexcept;

    /// If children are subtables, then this function returns the
    /// parent table. Otherwise it returns null.
    ///
    /// If \a column_ndx_out is not null, this function must assign the index of
    /// the column within the parent table to `*column_ndx_out` when , and only
    /// when this table parent is a column in a parent table.
    virtual Table* get_parent_table(size_t* column_ndx_out = nullptr) noexcept;

    /// Must be called whenever a child table accessor is about to be destroyed.
    ///
    /// Note that the argument is a pointer to the child Table rather than its
    /// `ndx_in_parent` property. This is because only minimal accessor
    /// consistency can be assumed by this function.
    virtual void child_accessor_destroyed(Table* child) noexcept = 0;

    virtual size_t* record_subtable_path(size_t* begin, size_t* end) noexcept;

    friend class Table;
};





// Implementation:


inline void Table::bump_version(bool bump_global) const noexcept
{
    if (bump_global) {
        // This is only set on initial entry through an operation on the same
        // table.  recursive calls (via parent or via backlinks) must be done
        // with bump_global=false.
        m_top.get_alloc().bump_global_version();
    }
    if (m_top.get_alloc().should_propagate_version(m_version)) {
        if (const Table* parent = get_parent_table_ptr())
            parent->bump_version(false);
        // Recurse through linked tables, use m_mark to avoid infinite recursion
        size_t limit = m_cols.size();
        for (size_t i = 0; i < limit; ++i) {
            // We may meet a null pointer in place of a backlink column, pending
            // replacement with a new one. This can happen ONLY when creation of
            // the corresponding forward link column in the origin table is
            // pending as well. In this case it is ok to just ignore the zeroed
            // backlink column, because the origin table is guaranteed to also
            // be refreshed/marked dirty and hence have it's version bumped.
            if (ColumnBase* col = m_cols[i])
                col->bump_link_origin_table_version();
        }
    }
}

inline void Table::remove(size_t row_ndx)
{
    bool is_move_last_over = false;
    erase_row(row_ndx, is_move_last_over); // Throws
}

inline void Table::move_last_over(size_t row_ndx)
{
    bool is_move_last_over = true;
    erase_row(row_ndx, is_move_last_over); // Throws
}

inline void Table::remove_last()
{
    if (!is_empty())
        remove(size()-1);
}

inline void Table::register_view(const TableViewBase* view)
{
    // Casting away constness here - operations done on tableviews
    // through m_views are all internal and preserving "some" kind
    // of logical constness.
    m_views.push_back(const_cast<TableViewBase*>(view));
}

inline bool Table::is_attached() const noexcept
{
    // Note that it is not possible to tie the state of attachment of a table to
    // the state of attachment of m_top, because tables with shared spec do not
    // have a 'top' array. Neither is it possible to tie it to the state of
    // attachment of m_columns, because subtables with shared spec start out in
    // a degenerate form where they do not have a 'columns' array. For these
    // reasons, it is neccessary to define the notion of attachment for a table
    // as follows: A table is attached if, and ony if m_column stores a non-null
    // parent pointer. This works because even for degenerate subtables,
    // m_columns is initialized with the correct parent pointer.
    return m_columns.has_parent();
}

inline StringData Table::get_name() const noexcept
{
    REALM_ASSERT(is_attached());
    const Array& real_top = m_top.is_attached() ? m_top : m_columns;
    ArrayParent* parent = real_top.get_parent();
    if (!parent)
        return StringData("");
    size_t index_in_parent = real_top.get_ndx_in_parent();
    REALM_ASSERT(dynamic_cast<Parent*>(parent));
    return static_cast<Parent*>(parent)->get_child_name(index_in_parent);
}

inline size_t Table::get_column_count() const noexcept
{
    REALM_ASSERT(is_attached());
    return m_spec.get_public_column_count();
}

inline StringData Table::get_column_name(size_t ndx) const noexcept
{
    REALM_ASSERT_3(ndx, <, get_column_count());
    return m_spec.get_column_name(ndx);
}

inline size_t Table::get_column_index(StringData name) const noexcept
{
    REALM_ASSERT(is_attached());
    return m_spec.get_column_index(name);
}

inline ColumnType Table::get_real_column_type(size_t ndx) const noexcept
{
    REALM_ASSERT_3(ndx, <, m_spec.get_column_count());
    return m_spec.get_column_type(ndx);
}

inline DataType Table::get_column_type(size_t ndx) const noexcept
{
    REALM_ASSERT_3(ndx, <, m_spec.get_column_count());
    return m_spec.get_public_column_type(ndx);
}

template<class Col, ColumnType col_type>
inline Col& Table::get_column(size_t ndx)
{
    ColumnBase& col = get_column_base(ndx);
#ifdef REALM_DEBUG
    validate_column_type(col, col_type, ndx);
#endif
    REALM_ASSERT(typeid(Col) == typeid(col));
    return static_cast<Col&>(col);
}

template<class Col, ColumnType col_type>
inline const Col& Table::get_column(size_t ndx) const noexcept
{
    const ColumnBase& col = get_column_base(ndx);
#ifdef REALM_DEBUG
    validate_column_type(col, col_type, ndx);
#endif
    REALM_ASSERT(typeid(Col) == typeid(col));
    return static_cast<const Col&>(col);
}

inline bool Table::has_shared_type() const noexcept
{
    REALM_ASSERT(is_attached());
    return !m_top.is_attached();
}


class Table::UnbindGuard {
public:
    UnbindGuard(Table* table) noexcept: m_table(table)
    {
    }

    ~UnbindGuard() noexcept
    {
        if (m_table)
            m_table->unbind_ptr();
    }

    Table& operator*() const noexcept
    {
        return *m_table;
    }

    Table* operator->() const noexcept
    {
        return m_table;
    }

    Table* get() const noexcept
    {
        return m_table;
    }

    Table* release() noexcept
    {
        Table* table = m_table;
        m_table = nullptr;
        return table;
    }

private:
    Table* m_table;
};


inline Table::Table(Allocator& alloc):
    m_top(alloc),
    m_columns(alloc),
    m_spec(alloc)
{
    m_ref_count = 1; // Explicitely managed lifetime
    m_descriptor = nullptr;

    ref_type ref = create_empty_table(alloc); // Throws
    Parent* parent = nullptr;
    size_t ndx_in_parent = 0;
    init(ref, parent, ndx_in_parent);
}

inline Table::Table(const Table& t, Allocator& alloc):
    m_top(alloc),
    m_columns(alloc),
    m_spec(alloc)
{
    m_ref_count = 1; // Explicitely managed lifetime
    m_descriptor = nullptr;

    ref_type ref = t.clone(alloc); // Throws
    Parent* parent = nullptr;
    size_t ndx_in_parent = 0;
    init(ref, parent, ndx_in_parent);
}

inline Table::Table(ref_count_tag, Allocator& alloc):
    m_top(alloc),
    m_columns(alloc),
    m_spec(alloc)
{
    m_ref_count = 0; // Lifetime managed by reference counting
    m_descriptor = nullptr;
}

inline Allocator& Table::get_alloc() const
{
    return m_top.get_alloc();
}

inline TableRef Table::create(Allocator& alloc)
{
    std::unique_ptr<Table> table(new Table(ref_count_tag(), alloc)); // Throws
    ref_type ref = create_empty_table(alloc); // Throws
    Parent* parent = nullptr;
    size_t ndx_in_parent = 0;
    table->init(ref, parent, ndx_in_parent); // Throws
    return table.release()->get_table_ref();
}

inline TableRef Table::copy(Allocator& alloc) const
{
    std::unique_ptr<Table> table(new Table(ref_count_tag(), alloc)); // Throws
    ref_type ref = clone(alloc); // Throws
    Parent* parent = nullptr;
    size_t ndx_in_parent = 0;
    table->init(ref, parent, ndx_in_parent); // Throws
    return table.release()->get_table_ref();
}

// For use by queries
template<class T>
inline Columns<T> Table::column(size_t column)
{
    std::vector<size_t> tmp = m_link_chain;
    if (std::is_same<T, Link>::value || std::is_same<T, LinkList>::value) {
        tmp.push_back(column);
    }

    // Check if user-given template type equals Realm type. Todo, we should clean up and reuse all our
    // type traits (all the is_same() cases below).
    const Table* table = get_link_chain_target(m_link_chain);

    realm::DataType ct = table->get_column_type(column);
    if (std::is_same<T, int64_t>::value && ct != type_Int)
        throw(LogicError::type_mismatch);
    else if (std::is_same<T, bool>::value && ct != type_Bool)
        throw(LogicError::type_mismatch);
    else if (std::is_same<T, realm::DateTime>::value && ct != type_DateTime)
        throw(LogicError::type_mismatch);
    else if (std::is_same<T, float>::value && ct != type_Float)
        throw(LogicError::type_mismatch);
    else if (std::is_same<T, double>::value && ct != type_Double)
        throw(LogicError::type_mismatch);


    m_link_chain.clear();
    return Columns<T>(column, this, tmp);
}

// For use by queries
inline Table& Table::link(size_t link_column)
{
    m_link_chain.push_back(link_column);
    return *this;
}

inline bool Table::is_empty() const noexcept
{
    return m_size == 0;
}

inline size_t Table::size() const noexcept
{
    return m_size;
}

inline Table::RowExpr Table::get(size_t row_ndx) noexcept
{
    REALM_ASSERT_3(row_ndx, <, size());
    return RowExpr(this, row_ndx);
}

inline Table::ConstRowExpr Table::get(size_t row_ndx) const noexcept
{
    REALM_ASSERT_3(row_ndx, <, size());
    return ConstRowExpr(this, row_ndx);
}

inline Table::RowExpr Table::front() noexcept
{
    return get(0);
}

inline Table::ConstRowExpr Table::front() const noexcept
{
    return get(0);
}

inline Table::RowExpr Table::back() noexcept
{
    return get(m_size-1);
}

inline Table::ConstRowExpr Table::back() const noexcept
{
    return get(m_size-1);
}

inline Table::RowExpr Table::operator[](size_t row_ndx) noexcept
{
    return get(row_ndx);
}

inline Table::ConstRowExpr Table::operator[](size_t row_ndx) const noexcept
{
    return get(row_ndx);
}

inline size_t Table::add_empty_row(size_t num_rows)
{
    size_t row_ndx = m_size;
    insert_empty_row(row_ndx, num_rows); // Throws
    return row_ndx; // Return index of first new row
}

inline const Table* Table::get_subtable_ptr(size_t col_ndx, size_t row_ndx) const
{
    return const_cast<Table*>(this)->get_subtable_ptr(col_ndx, row_ndx); // Throws
}

inline bool Table::is_null_link(size_t col_ndx, size_t row_ndx) const noexcept
{
    return get_link(col_ndx, row_ndx) == realm::npos;
}

inline ConstTableRef Table::get_link_target(size_t col_ndx) const noexcept
{
    return const_cast<Table*>(this)->get_link_target(col_ndx);
}

template<class E>
inline void Table::set_enum(size_t column_ndx, size_t row_ndx, E value)
{
    set_int(column_ndx, row_ndx, value);
}

inline void Table::nullify_link(size_t col_ndx, size_t row_ndx)
{
    set_link(col_ndx, row_ndx, realm::npos);
}

inline TableRef Table::get_subtable(size_t column_ndx, size_t row_ndx)
{
    return TableRef(get_subtable_ptr(column_ndx, row_ndx));
}

inline ConstTableRef Table::get_subtable(size_t column_ndx, size_t row_ndx) const
{
    return ConstTableRef(get_subtable_ptr(column_ndx, row_ndx));
}

inline ConstTableRef Table::get_parent_table(size_t* column_ndx_out) const noexcept
{
    return ConstTableRef(get_parent_table_ptr(column_ndx_out));
}

inline TableRef Table::get_parent_table(size_t* column_ndx_out) noexcept
{
    return TableRef(get_parent_table_ptr(column_ndx_out));
}

inline bool Table::is_group_level() const noexcept
{
    return bool(get_parent_group());
}

inline Table::RowExpr Table::find_pkey_int(int_fast64_t value)
{
    Table* table = nullptr;
    size_t row_ndx = do_find_pkey_int(value); // Throws
    if (row_ndx != realm::not_found)
        table = this;
    return RowExpr(table, row_ndx);
}

inline Table::ConstRowExpr Table::find_pkey_int(int_fast64_t value) const
{
    const Table* table = nullptr;
    size_t row_ndx = do_find_pkey_int(value); // Throws
    if (row_ndx != realm::not_found)
        table = this;
    return ConstRowExpr(table, row_ndx);
}

inline Table::RowExpr Table::find_pkey_string(StringData value)
{
    Table* table = nullptr;
    size_t row_ndx = do_find_pkey_string(value); // Throws
    if (row_ndx != realm::not_found)
        table = this;
    return RowExpr(table, row_ndx);
}

inline Table::ConstRowExpr Table::find_pkey_string(StringData value) const
{
    const Table* table = nullptr;
    size_t row_ndx = do_find_pkey_string(value); // Throws
    if (row_ndx != realm::not_found)
        table = this;
    return ConstRowExpr(table, row_ndx);
}

inline bool Table::operator==(const Table& t) const
{
    return m_spec == t.m_spec && compare_rows(t); // Throws
}

inline bool Table::operator!=(const Table& t) const
{
    return !(*this == t); // Throws
}

inline bool Table::is_degenerate() const noexcept
{
    return !m_columns.is_attached();
}

inline void Table::set_into_mixed(Table* parent, size_t col_ndx, size_t row_ndx) const
{
    parent->set_mixed_subtable(col_ndx, row_ndx, this);
}

inline size_t Table::get_size_from_ref(ref_type top_ref, Allocator& alloc) noexcept
{
    const char* top_header = alloc.translate(top_ref);
    std::pair<int_least64_t, int_least64_t> p = Array::get_two(top_header, 0);
    ref_type spec_ref = to_ref(p.first), columns_ref = to_ref(p.second);
    return get_size_from_ref(spec_ref, columns_ref, alloc);
}

inline Table* Table::get_parent_table_ptr(size_t* column_ndx_out) noexcept
{
    const Table* parent = const_cast<const Table*>(this)->get_parent_table_ptr(column_ndx_out);
    return const_cast<Table*>(parent);
}

inline bool Table::is_link_type(ColumnType col_type) noexcept
{
    return col_type == col_type_Link || col_type == col_type_LinkList;
}

inline size_t* Table::record_subtable_path(size_t* begin, size_t* end) const noexcept
{
    const Array& real_top = m_top.is_attached() ? m_top : m_columns;
    size_t index_in_parent = real_top.get_ndx_in_parent();
    REALM_ASSERT_3(begin, <, end);
    *begin++ = index_in_parent;
    ArrayParent* parent = real_top.get_parent();
    REALM_ASSERT(parent);
    REALM_ASSERT(dynamic_cast<Parent*>(parent));
    return static_cast<Parent*>(parent)->record_subtable_path(begin, end);
}

inline size_t* Table::Parent::record_subtable_path(size_t* begin, size_t*) noexcept
{
    return begin;
}

template<class T>
typename T::RowAccessor Table::get_link_accessor(size_t column_ndx, size_t row_ndx)
{
    size_t row_pos_in_target = get_link(column_ndx, row_ndx);
    TableRef target_table = get_link_target(column_ndx);

    Table* table = &*target_table;
    T* typed_table = reinterpret_cast<T*>(table);
    return (*typed_table)[row_pos_in_target];
}

inline bool Table::is_marked() const noexcept
{
    return m_mark;
}

inline void Table::mark() noexcept
{
    m_mark = true;
}

inline void Table::unmark() noexcept
{
    m_mark = false;
}

inline Replication* Table::get_repl() noexcept
{
    return m_top.get_alloc().get_replication();
}

inline void Table::set_ndx_in_parent(size_t ndx_in_parent) noexcept
{
    if (m_top.is_attached()) {
        // Root table (independent descriptor)
        m_top.set_ndx_in_parent(ndx_in_parent);
    }
    else {
        // Subtable with shared descriptor
        m_columns.set_ndx_in_parent(ndx_in_parent);
    }
}


// The purpose of this class is to give internal access to some, but
// not all of the non-public parts of the Table class.
class _impl::TableFriend {
public:
    typedef Table::UnbindGuard UnbindGuard;

    static ref_type create_empty_table(Allocator& alloc)
    {
        return Table::create_empty_table(alloc); // Throws
    }

    static ref_type clone(const Table& table, Allocator& alloc)
    {
        return table.clone(alloc); // Throws
    }

    static ref_type clone_columns(const Table& table, Allocator& alloc)
    {
        return table.clone_columns(alloc); // Throws
    }

    static Table* create_accessor(Allocator& alloc, ref_type top_ref,
                                  Table::Parent* parent, size_t ndx_in_parent)
    {
        std::unique_ptr<Table> table(new Table(Table::ref_count_tag(), alloc)); // Throws
        table->init(top_ref, parent, ndx_in_parent); // Throws
        return table.release();
    }

    static Table* create_accessor(ConstSubspecRef shared_spec, Table::Parent* parent_column,
                                  size_t parent_row_ndx)
    {
        Allocator& alloc = shared_spec.get_alloc();
        std::unique_ptr<Table> table(new Table(Table::ref_count_tag(), alloc)); // Throws
        table->init(shared_spec, parent_column, parent_row_ndx); // Throws
        return table.release();
    }

    // Intended to be used only by Group::create_table_accessor()
    static Table* create_incomplete_accessor(Allocator& alloc, ref_type top_ref,
                                             Table::Parent* parent, size_t ndx_in_parent)
    {
        std::unique_ptr<Table> table(new Table(Table::ref_count_tag(), alloc)); // Throws
        bool skip_create_column_accessors = true;
        table->init(top_ref, parent, ndx_in_parent, skip_create_column_accessors); // Throws
        return table.release();
    }

    // Intended to be used only by Group::create_table_accessor()
    static void complete_accessor(Table& table)
    {
        table.refresh_column_accessors(); // Throws
    }

    static void set_top_parent(Table& table, ArrayParent* parent,
                               size_t ndx_in_parent) noexcept
    {
        table.m_top.set_parent(parent, ndx_in_parent);
    }

    static void update_from_parent(Table& table, size_t old_baseline) noexcept
    {
        table.update_from_parent(old_baseline);
    }

    static void detach(Table& table) noexcept
    {
        table.detach();
    }

    static void discard_row_accessors(Table& table) noexcept
    {
        table.discard_row_accessors();
    }

    static void discard_child_accessors(Table& table) noexcept
    {
        table.discard_child_accessors();
    }

    static void discard_subtable_accessor(Table& table, size_t col_ndx, size_t row_ndx) noexcept
    {
        table.discard_subtable_accessor(col_ndx, row_ndx);
    }

    static void bind_ptr(Table& table) noexcept
    {
        table.bind_ptr();
    }

    static void unbind_ptr(Table& table) noexcept
    {
        table.unbind_ptr();
    }

    static bool compare_rows(const Table& a, const Table& b)
    {
        return a.compare_rows(b); // Throws
    }

    static size_t get_size_from_ref(ref_type ref, Allocator& alloc) noexcept
    {
        return Table::get_size_from_ref(ref, alloc);
    }

    static size_t get_size_from_ref(ref_type spec_ref, ref_type columns_ref,
                                         Allocator& alloc) noexcept
    {
        return Table::get_size_from_ref(spec_ref, columns_ref, alloc);
    }

    static Spec& get_spec(Table& table) noexcept
    {
        return table.m_spec;
    }

    static const Spec& get_spec(const Table& table) noexcept
    {
        return table.m_spec;
    }

    static ColumnBase& get_column(const Table& table, size_t col_ndx)
    {
        return *table.m_cols[col_ndx];
    }

    static void do_remove(Table& table, size_t row_ndx)
    {
        bool broken_reciprocal_backlinks = false;
        table.do_remove(row_ndx, broken_reciprocal_backlinks); // Throws
    }

    static void do_move_last_over(Table& table, size_t row_ndx)
    {
        bool broken_reciprocal_backlinks = false;
        table.do_move_last_over(row_ndx, broken_reciprocal_backlinks); // Throws
    }

    static void do_swap_rows(Table& table, size_t row_ndx_1, size_t row_ndx_2)
    {
        table.do_swap_rows(row_ndx_1, row_ndx_2); // Throws
    }

    static void do_clear(Table& table)
    {
        bool broken_reciprocal_backlinks = false;
        table.do_clear(broken_reciprocal_backlinks); // Throws
    }

    static void do_set_link(Table& table, size_t col_ndx, size_t row_ndx,
                            size_t target_row_ndx)
    {
        table.do_set_link(col_ndx, row_ndx, target_row_ndx); // Throws
    }

    static size_t get_num_strong_backlinks(const Table& table,
                                                size_t row_ndx) noexcept
    {
        return table.get_num_strong_backlinks(row_ndx);
    }

    static void cascade_break_backlinks_to(Table& table, size_t row_ndx,
                                           CascadeState& state)
    {
        table.cascade_break_backlinks_to(row_ndx, state); // Throws
    }

    static void remove_backlink_broken_rows(Table& table, const CascadeState& rows)
    {
        table.remove_backlink_broken_rows(rows); // Throws
    }

    static size_t* record_subtable_path(const Table& table, size_t* begin,
                                             size_t* end) noexcept
    {
        return table.record_subtable_path(begin, end);
    }

    static void insert_column(Descriptor& desc, size_t column_ndx, DataType type,
                              StringData name, Table* link_target_table, bool nullable = false)
    {
        Table::do_insert_column(desc, column_ndx, type, name, link_target_table, nullable); // Throws
    }

    static void insert_column_unless_exists(Descriptor& desc, size_t column_ndx, DataType type,
                                            StringData name, Table* link_target_table, bool nullable = false,
                                            bool* was_inserted = nullptr)
    {
        Table::do_insert_column_unless_exists(desc, column_ndx, type, name, link_target_table, nullable, was_inserted); // Throws
    }

    static void erase_column(Descriptor& desc, size_t column_ndx)
    {
        Table::do_erase_column(desc, column_ndx); // Throws
    }

    static void rename_column(Descriptor& desc, size_t column_ndx, StringData name)
    {
        Table::do_rename_column(desc, column_ndx, name); // Throws
    }

    static void move_column(Descriptor& desc, size_t col_ndx_1, size_t col_ndx_2)
    {
        Table::do_move_column(desc, col_ndx_1, col_ndx_2); // Throws
    }

    static void set_link_type(Table& table, size_t column_ndx, LinkType link_type)
    {
        table.do_set_link_type(column_ndx, link_type); // Throws
    }

    static void erase_row(Table& table, size_t row_ndx, bool is_move_last_over)
    {
        table.erase_row(row_ndx, is_move_last_over); // Throws
    }

    static void batch_erase_rows(Table& table, const IntegerColumn& row_indexes,
                                 bool is_move_last_over)
    {
        table.batch_erase_rows(row_indexes, is_move_last_over); // Throws
    }

    static void clear_root_table_desc(const Table& root_table) noexcept
    {
        REALM_ASSERT(!root_table.has_shared_type());
        root_table.m_descriptor = nullptr;
    }

    static Table* get_subtable_accessor(Table& table, size_t col_ndx,
                                        size_t row_ndx) noexcept
    {
        return table.get_subtable_accessor(col_ndx, row_ndx);
    }

    static const Table* get_link_target_table_accessor(const Table& table,
                                                       size_t col_ndx) noexcept
    {
        return const_cast<Table&>(table).get_link_target_table_accessor(col_ndx);
    }

    static Table* get_link_target_table_accessor(Table& table, size_t col_ndx) noexcept
    {
        return table.get_link_target_table_accessor(col_ndx);
    }

    static void adj_acc_insert_rows(Table& table, size_t row_ndx,
                                    size_t num_rows) noexcept
    {
        table.adj_acc_insert_rows(row_ndx, num_rows);
    }

    static void adj_acc_erase_row(Table& table, size_t row_ndx) noexcept
    {
        table.adj_acc_erase_row(row_ndx);
    }

    static void adj_acc_swap_rows(Table& table, size_t row_ndx_1, size_t row_ndx_2) noexcept
    {
        table.adj_acc_swap_rows(row_ndx_1, row_ndx_2);
    }

    static void adj_acc_move_over(Table& table, size_t from_row_ndx,
                                  size_t to_row_ndx) noexcept
    {
        table.adj_acc_move_over(from_row_ndx, to_row_ndx);
    }

    static void adj_acc_clear_root_table(Table& table) noexcept
    {
        table.adj_acc_clear_root_table();
    }

    static void adj_acc_clear_nonroot_table(Table& table) noexcept
    {
        table.adj_acc_clear_nonroot_table();
    }

    static void adj_insert_column(Table& table, size_t col_ndx)
    {
        table.adj_insert_column(col_ndx); // Throws
    }

    static void adj_add_column(Table& table)
    {
        size_t num_cols = table.m_cols.size();
        table.adj_insert_column(num_cols); // Throws
    }

    static void adj_erase_column(Table& table, size_t col_ndx) noexcept
    {
        table.adj_erase_column(col_ndx);
    }

    static void adj_move_column(Table& table, size_t col_ndx_1, size_t col_ndx_2) noexcept
    {
        table.adj_move_column(col_ndx_1, col_ndx_2);
    }

    static bool is_marked(const Table& table) noexcept
    {
        return table.is_marked();
    }

    static void mark(Table& table) noexcept
    {
        table.mark();
    }

    static void unmark(Table& table) noexcept
    {
        table.unmark();
    }

    static void recursive_mark(Table& table) noexcept
    {
        table.recursive_mark();
    }

    static void mark_link_target_tables(Table& table, size_t col_ndx_begin) noexcept
    {
        table.mark_link_target_tables(col_ndx_begin);
    }

    static void mark_opposite_link_tables(Table& table) noexcept
    {
        table.mark_opposite_link_tables();
    }

    static Descriptor* get_root_table_desc_accessor(Table& root_table) noexcept
    {
        return root_table.m_descriptor;
    }

    typedef Table::AccessorUpdater AccessorUpdater;
    static void update_accessors(Table& table, const size_t* col_path_begin,
                                 const size_t* col_path_end, AccessorUpdater& updatder)
    {
        table.update_accessors(col_path_begin, col_path_end, updatder); // Throws
    }

    static void refresh_accessor_tree(Table& table)
    {
        table.refresh_accessor_tree(); // Throws
    }

    static void set_ndx_in_parent(Table& table, size_t ndx_in_parent) noexcept
    {
        table.set_ndx_in_parent(ndx_in_parent);
    }

    static void set_shared_subspec_ndx_in_parent(Table& table, size_t spec_ndx_in_parent) noexcept
    {
        table.m_spec.set_ndx_in_parent(spec_ndx_in_parent);
    }

    static bool is_link_type(ColumnType type) noexcept
    {
        return Table::is_link_type(type);
    }

    static void bump_version(Table& table, bool bump_global = true) noexcept
    {
        table.bump_version(bump_global);
    }

    static bool is_cross_table_link_target(const Table& table)
    {
        return table.is_cross_table_link_target();
    }

    static Group* get_parent_group(const Table& table) noexcept
    {
        return table.get_parent_group();
    }

    static Replication* get_repl(Table& table) noexcept
    {
        return table.get_repl();
    }

    static void register_view(Table& table, const TableViewBase* view)
    {
        table.register_view(view); // Throws
    }

    static void unregister_view(Table& table, const TableViewBase* view) noexcept
    {
        table.unregister_view(view);
    }
};


} // namespace realm

#endif // REALM_TABLE_HPP
