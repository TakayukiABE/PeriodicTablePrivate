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
#ifndef REALM_ALLOC_SLAB_HPP
#define REALM_ALLOC_SLAB_HPP

#include <stdint.h> // unint8_t etc
#include <vector>
#include <string>
#include <atomic>

#include <realm/util/features.h>
#include <realm/util/file.hpp>
#include <realm/alloc.hpp>
#include <realm/disable_sync_to_disk.hpp>

namespace realm {

// Pre-declarations
class Group;
class GroupWriter;


/// Thrown by Group and SharedGroup constructors if the specified file
/// (or memory buffer) does not appear to contain a valid Realm
/// database.
struct InvalidDatabase;


/// The allocator that is used to manage the memory of a Realm
/// group, i.e., a Realm database.
///
/// Optionally, it can be attached to an pre-existing database (file
/// or memory buffer) which then becomes an immuatble part of the
/// managed memory.
///
/// To attach a slab allocator to a pre-existing database, call
/// attach_file() or attach_buffer(). To create a new database
/// in-memory, call attach_empty().
///
/// For efficiency, this allocator manages its mutable memory as a set
/// of slabs.
class SlabAlloc: public Allocator {
public:
    /// File format versions:
    ///
    ///   1   Initial file format version
    ///
    ///   2   FIXME: Does anybody remember what happened here?
    ///
    ///   3   Supporting null on string columns broke the file format in following way: Index appends an 'X'
    ///       character to all strings except the null string, to be able to distinguish between null and
    ///       empty string.

    // Bumped to 3 because of null support of String columns and because of new format of index
    static constexpr int library_file_format = 3;

    ~SlabAlloc() noexcept override;
    SlabAlloc();

    struct Config {
        bool is_shared = false;
        bool read_only = false;
        bool no_create = false;
        bool skip_validate = false;
        bool server_sync_mode = false;
        bool session_initiator = false;
        bool clear_file = false;
        const char* encryption_key = nullptr;
    };

    /// Attach this allocator to the specified file.
    ///
    /// When used by free-standing Group instances, no concurrency is
    /// allowed. When used on behalf of SharedGroup, concurrency is
    /// allowed, but read_only and no_create must both be false in
    /// this case.
    ///
    /// It is an error to call this function on an attached
    /// allocator. Doing so will result in undefined behavor.
    ///
    /// Except for \a path, the parameters are passed in through a
    /// configuration object.
    ///
    /// \param is_shared Must be true if, and only if we are called on
    /// behalf of SharedGroup.
    ///
    /// \param read_only Open the file in read-only mode. This implies
    /// \a no_create.
    ///
    /// \param no_create Fail if the file does not already exist.
    ///
    /// \param bool skip_validate Skip validation of file header. In a
    /// set of overlapping SharedGroups, only the first one (the one
    /// that creates/initlializes the coordination file) may validate
    /// the header, otherwise it will result in a race condition.
    ///
    /// \param encryption_key 32-byte key to use to encrypt and decrypt
    /// the backing storage, or nullptr to disable encryption.
    ///
    /// \param server_sync_mode bool indicating whether the database is operated
    /// in server_synchronization mode or not. If the database is created,
    /// this setting is stored in it. If the database exists already, it is validated
    /// that the database was created with the same setting. In case of conflict
    /// a runtime_error is thrown.
    ///
    /// \param session_initiator if set, the caller is the session initiator and
    /// guarantees exclusive access to the file. If attaching in read/write mode,
    /// the file is modified: files on streaming form is changed to non-streaming
    /// form, and if needed the file size is adjusted to match mmap boundaries.
    /// Must be set to false if is_shared is false.
    ///
    /// \param clear_file Always initialize the file as if it was a newly
    /// created file and ignore any pre-existing contents. Requires that
    /// session_initiator be true as well.
    ///
    /// \return The `ref` of the root node, or zero if there is none.
    ///
    /// \throw util::File::AccessError
    ref_type attach_file(const std::string& path, Config& cfg);

    /// Attach this allocator to the specified memory buffer.
    ///
    /// It is an error to call this function on an attached
    /// allocator. Doing so will result in undefined behavor.
    ///
    /// \return The `ref` of the root node, or zero if there is none.
    ///
    /// \sa own_buffer()
    ///
    /// \throw InvalidDatabase
    ref_type attach_buffer(char* data, size_t size);

    /// Reads file format from file header. Must be called from within a write
    /// transaction.
    int get_committed_file_format() const noexcept;

    /// Attach this allocator to an empty buffer.
    ///
    /// It is an error to call this function on an attached
    /// allocator. Doing so will result in undefined behavor.
    void attach_empty();

    /// Detach from a previously attached file or buffer.
    ///
    /// This function does not reset free space tracking. To
    /// completely reset the allocator, you must also call
    /// reset_free_space_tracking().
    ///
    /// This function has no effect if the allocator is already in the
    /// detached state (idempotency).
    void detach() noexcept;

    class DetachGuard;

    /// If a memory buffer has been attached using attach_buffer(),
    /// mark it as owned by this slab allocator. Behaviour is
    /// undefined if this function is called on a detached allocator,
    /// one that is not attached using attach_buffer(), or one for
    /// which this function has already been called during the latest
    /// attachment.
    void own_buffer() noexcept;

    /// Returns true if, and only if this allocator is currently
    /// in the attached state.
    bool is_attached() const noexcept;

    /// Returns true if, and only if this allocator is currently in
    /// the attached state and attachment was not established using
    /// attach_empty().
    bool nonempty_attachment() const noexcept;

    /// Reserve disk space now to avoid allocation errors at a later
    /// point in time, and to minimize on-disk fragmentation. In some
    /// cases, less fragmentation translates into improved
    /// performance. On flash or SSD-drives this is likely a waste.
    ///
    /// Note: File::prealloc() may misbehave under race conditions (see
    /// documentation of File::prealloc()). For that reason, to avoid race
    /// conditions, when this allocator is used in a transactional mode, this
    /// function may be called only when the caller has exclusive write
    /// access. In non-transactional mode it is the responsibility of the user
    /// to ensure non-concurrent file mutation.
    ///
    /// This function will call File::sync().
    ///
    /// It is an error to call this function on an allocator that is not
    /// attached to a file. Doing so will result in undefined behavior.
    void resize_file(size_t new_file_size);

    /// Reserve disk space now to avoid allocation errors at a later point in
    /// time, and to minimize on-disk fragmentation. In some cases, less
    /// fragmentation translates into improved performance. On SSD-drives
    /// preallocation is likely a waste.
    ///
    /// When supported by the system, a call to this function will make the
    /// database file at least as big as the specified size, and cause space on
    /// the target device to be allocated (note that on many systems on-disk
    /// allocation is done lazily by default). If the file is already bigger
    /// than the specified size, the size will be unchanged, and on-disk
    /// allocation will occur only for the initial section that corresponds to
    /// the specified size. On systems that do not support preallocation, this
    /// function has no effect. To know whether preallocation is supported by
    /// Realm on your platform, call util::File::is_prealloc_supported().
    ///
    /// This function will call File::sync() if it changes the size of the file.
    ///
    /// It is an error to call this function on an allocator that is not
    /// attached to a file. Doing so will result in undefined behavior.
    void reserve_disk_space(size_t size_in_bytes);

    /// Get the size of the attached database file or buffer in number
    /// of bytes. This size is not affected by new allocations. After
    /// attachment, it can only be modified by a call to remap().
    ///
    /// It is an error to call this function on a detached allocator,
    /// or one that was attached using attach_empty(). Doing so will
    /// result in undefined behavior.
    size_t get_baseline() const noexcept;

    /// Get the total amount of managed memory. This is the baseline plus the
    /// sum of the sizes of the allocated slabs. It includes any free space.
    ///
    /// It is an error to call this function on a detached
    /// allocator. Doing so will result in undefined behavior.
    size_t get_total_size() const noexcept;

    /// Mark all managed memory (except the attached file) as free
    /// space.
    void reset_free_space_tracking();

    /// Remap the attached file such that a prefix of the specified
    /// size becomes available in memory. If sucessfull,
    /// get_baseline() will return the specified new file size.
    ///
    /// It is an error to call this function on a detached allocator,
    /// or one that was not attached using attach_file(). Doing so
    /// will result in undefined behavior.
    ///
    /// The file_size argument must be aligned to a *section* boundary:
    /// The database file is logically split into sections, each section
    /// guaranteed to be mapped as a contiguous address range. The allocation
    /// of memory in the file must ensure that no allocation crosses the
    /// boundary between two sections.
    void remap(size_t file_size);

#ifdef REALM_DEBUG
    void enable_debug(bool enable) { m_debug_out = enable; }
    void verify() const override;
    bool is_all_free() const;
    void print() const;
#endif

protected:
    MemRef do_alloc(size_t size) override;
    MemRef do_realloc(ref_type, const char*, size_t old_size,
                    size_t new_size) override;
    // FIXME: It would be very nice if we could detect an invalid free operation in debug mode
    void do_free(ref_type, const char*) noexcept override;
    char* do_translate(ref_type) const noexcept override;
    void invalidate_cache() noexcept;
private:
    enum AttachMode {
        attach_None,        // Nothing is attached
        attach_OwnedBuffer, // We own the buffer (m_data = nullptr for empty buffer)
        attach_UsersBuffer, // We do not own the buffer
        attach_SharedFile,  // On behalf of SharedGroup
        attach_UnsharedFile // Not on behalf of SharedGroup
    };

    // A slab is a dynamically allocated contiguous chunk of memory used to
    // extend the amount of space available for database node
    // storage. Inter-node references are represented as file offsets
    // (a.k.a. "refs"), and each slab creates an apparently seamless extension
    // of this file offset addressable space. Slabes are stored as rows in the
    // Slabs table in order of ascending file offsets.
    struct Slab {
        ref_type ref_end;
        char* addr;
    };
    struct Chunk {
        ref_type ref;
        size_t size;
    };

    // Values of each used bit in m_flags
    enum {
        flags_SelectBit = 1,
        flags_ServerSyncMode = 2
    };

    // 24 bytes
    struct Header {
        uint64_t m_top_ref[2]; // 2 * 8 bytes
        // Info-block 8-bytes
        uint8_t m_mnemonic[4]; // "T-DB"
        uint8_t m_file_format[2]; // See `library_file_format`
        uint8_t m_reserved;
        // bit 0 of m_flags is used to select between the two top refs.
        // bit 1 of m_flags is to be set for persistent commit-logs (Sync support).
        // when clear, the commit-logs will be removed at the end of a session.
        // when set, the commmit-logs are persisted, and IFF the database exists
        // already at the start of a session, the commit logs too must exist.
        uint8_t m_flags;
    };

    // 16 bytes
    struct StreamingFooter {
        uint64_t m_top_ref;
        uint64_t m_magic_cookie;
    };

    static_assert(sizeof (Header) == 24, "Bad header size");
    static_assert(sizeof (StreamingFooter) == 16, "Bad footer size");

    static const Header empty_file_header;
    static const Header streaming_header;

    static const uint_fast64_t footer_magic_cookie = 0x3034125237E526C8ULL;

    util::File m_file;

    // The initial mapping is determined by m_data and m_initial_mapping_size,
    util::File::Map<char> m_initial_mapping;
    char* m_data = nullptr;
    size_t m_initial_mapping_size = 0;
    // additional sections beyond those covered by the initial mapping, are
    // managed as separate mmap allocations, each covering one section.
    size_t m_first_additional_mapping = 0;
    size_t m_num_additional_mappings = 0;
    size_t m_capacity_additional_mappings = 0;
    size_t m_initial_section_size = 0;
    int m_section_shifts = 0;
    std::unique_ptr<util::File::Map<char>[]> m_additional_mappings;
    std::unique_ptr<size_t[]> m_section_bases;
    size_t m_num_section_bases = 0;
    AttachMode m_attach_mode = attach_None;

    /// If a file or buffer is currently attached and validation was
    /// not skipped during attachement, this flag is true if, and only
    /// if the attached file has a footer specifying the top-ref, that
    /// is, if the file is on the streaming form. This member is
    /// deliberately placed here (after m_attach_mode) in the hope
    /// that it leads to less padding between members due to alignment
    /// requirements.
    bool m_file_on_streaming_form;

    enum FeeeSpaceState {
        free_space_Clean,
        free_space_Dirty,
        free_space_Invalid
    };

    /// When set to free_space_Invalid, the free lists are no longer
    /// up-to-date. This happens if do_free() or
    /// reset_free_space_tracking() fails, presumably due to
    /// std::bad_alloc being thrown during updating of the free space
    /// list. In this this case, alloc(), realloc_(), and
    /// get_free_read_only() must throw. This member is deliberately
    /// placed here (after m_attach_mode) in the hope that it leads to
    /// less padding between members due to alignment requirements.
    FeeeSpaceState m_free_space_state = free_space_Clean;

    /// File format fetched from header during attach. If less than
    /// `library_file_format`, it will be updated later during SharedGroup
    /// construction as part of a file format upgrade.
    int m_file_format;

    typedef std::vector<Slab> slabs;
    typedef std::vector<Chunk> chunks;
    slabs m_slabs;
    chunks m_free_space;
    chunks m_free_read_only;

#ifdef REALM_DEBUG
    bool m_debug_out = false;
#endif
    struct hash_entry {
        ref_type ref = 0;
        char* addr = nullptr;
        size_t version = 0;
    };
    mutable hash_entry cache[256];
    mutable size_t version = 1;
    /// Throws if free-lists are no longer valid.
    const chunks& get_free_read_only() const;

    /// Throws InvalidDatabase if the file is not a Realm file, if the file is
    /// corrupted, or if the specified encryption key is incorrect. This
    /// function will not detect all forms of corruption, though.
    void validate_buffer(const char* data, size_t len, const std::string& path, bool is_shared);

    class ChunkRefEq;
    class ChunkRefEndEq;
    class SlabRefEndEq;
    static bool ref_less_than_slab_ref_end(ref_type, const Slab&) noexcept;

    Replication* get_replication() const noexcept { return m_replication; }
    void set_replication(Replication* r) noexcept { m_replication = r; }

    /// Returns the first section boundary *above* the given position.
    size_t get_upper_section_boundary(size_t start_pos) const noexcept;

    /// Returns the first section boundary *at or below* the given position.
    size_t get_lower_section_boundary(size_t start_pos) const noexcept;

    /// Returns true if the given position is at a section boundary
    bool matches_section_boundary(size_t pos) const noexcept;

    /// Returns the index of the section holding a given address.
    /// The section index is determined solely by the minimal section size,
    /// and does not necessarily reflect the mapping. A mapping may
    /// cover multiple sections - the initial mapping often does.
    size_t get_section_index(size_t pos) const noexcept;

    /// Reverse: get the base offset of a section at a given index. Since the
    /// computation is very time critical, this method just looks it up in
    /// a table. The actual computation and setup of that table is done
    /// during initialization with the help of compute_section_base() below.
    inline size_t get_section_base(size_t index) const noexcept;

    /// Actually compute the starting offset of a section. Only used to initialize
    /// a table of predefined results, which are then used by get_section_base().
    size_t compute_section_base(size_t index) const noexcept;

    /// Find a possible allocation of 'request_size' that will fit into a section
    /// which is inside the range from 'start_pos' to 'start_pos'+'free_chunk_size'
    /// If found return the position, if not return 0.
    size_t find_section_in_range(size_t start_pos, size_t free_chunk_size,
                                      size_t request_size) const noexcept;

    friend class Group;
    friend class GroupWriter;
};

inline void SlabAlloc::invalidate_cache() noexcept { ++version; }

class SlabAlloc::DetachGuard {
public:
    DetachGuard(SlabAlloc& alloc) noexcept: m_alloc(&alloc) {}
    ~DetachGuard() noexcept;
    SlabAlloc* release() noexcept;
private:
    SlabAlloc* m_alloc;
};



// Implementation:

struct InvalidDatabase: util::File::AccessError {
    InvalidDatabase(const std::string& msg, const std::string& path):
        util::File::AccessError(msg, path)
    {
    }
};

inline void SlabAlloc::own_buffer() noexcept
{
    REALM_ASSERT_3(m_attach_mode, ==, attach_UsersBuffer);
    REALM_ASSERT(m_data);
    REALM_ASSERT(!m_file.is_attached());
    m_attach_mode = attach_OwnedBuffer;
}

inline bool SlabAlloc::is_attached() const noexcept
{
    return m_attach_mode != attach_None;
}

inline bool SlabAlloc::nonempty_attachment() const noexcept
{
    return is_attached() && m_data;
}

inline size_t SlabAlloc::get_baseline() const noexcept
{
    REALM_ASSERT_DEBUG(is_attached());
    return m_baseline;
}

inline void SlabAlloc::resize_file(size_t new_file_size)
{
    m_file.prealloc(0, new_file_size); // Throws
    bool disable_sync = get_disable_sync_to_disk();
    if (!disable_sync)
        m_file.sync(); // Throws
}

inline void SlabAlloc::reserve_disk_space(size_t size)
{
    m_file.prealloc_if_supported(0, size); // Throws
    bool disable_sync = get_disable_sync_to_disk();
    if (!disable_sync)
        m_file.sync(); // Throws
}

inline SlabAlloc::DetachGuard::~DetachGuard() noexcept
{
    if (m_alloc)
        m_alloc->detach();
}

inline SlabAlloc* SlabAlloc::DetachGuard::release() noexcept
{
    SlabAlloc* alloc = m_alloc;
    m_alloc = nullptr;
    return alloc;
}

inline bool SlabAlloc::ref_less_than_slab_ref_end(ref_type ref, const Slab& slab) noexcept
{
    return ref < slab.ref_end;
}

inline size_t SlabAlloc::get_upper_section_boundary(size_t start_pos) const noexcept
{
    return get_section_base(1+get_section_index(start_pos));
}

inline size_t SlabAlloc::get_lower_section_boundary(size_t start_pos) const noexcept
{
    return get_section_base(get_section_index(start_pos));
}

inline bool SlabAlloc::matches_section_boundary(size_t pos) const noexcept
{
    return pos == get_lower_section_boundary(pos);
}

inline size_t SlabAlloc::get_section_base(size_t index) const noexcept
{
    return m_section_bases[index];
}

} // namespace realm

#endif // REALM_ALLOC_SLAB_HPP
